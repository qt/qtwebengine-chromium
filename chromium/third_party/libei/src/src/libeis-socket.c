/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>

#include "util-io.h"
#include "util-object.h"
#include "util-mem.h"
#include "util-macros.h"
#include "util-sources.h"
#include "util-strings.h"

#include "libeis.h"
#include "libeis-private.h"

struct eis_socket {
	struct object object;
	struct source *listener;
	char *socketpath;
	char *lockpath;
	int lockfd;
};

static inline void
eis_socket_destroy(struct eis_socket *socket)
{
	source_remove(socket->listener);
	socket->listener = source_unref(socket->listener);
	if (socket->lockpath) {
		unlink(socket->lockpath);
		xclose(socket->lockfd);
		free(socket->lockpath);
	}
	if (socket->socketpath) {
		unlink(socket->socketpath);
		free(socket->socketpath);
	}
}

static
OBJECT_IMPLEMENT_CREATE(eis_socket);
static
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_socket);

static
OBJECT_IMPLEMENT_PARENT(eis_socket, eis);

static void
interface_socket_destroy(struct eis *eis, void *backend)
{
	struct eis_socket *socket = backend;
	eis_socket_unref(socket);

}

static const struct eis_backend_interface interface = {
	.destroy = interface_socket_destroy,
};

static void
listener_dispatch(struct source *source, void *data)
{
	struct eis_socket *socket = data;
	struct eis *eis = eis_socket_parent(socket);

	log_debug(eis, "New client connection waiting");
	int fd = accept4(source_get_fd(source), NULL, NULL, SOCK_NONBLOCK|SOCK_CLOEXEC);
	if (fd == -1)
		return;

	struct eis_client *client = eis_client_new(eis, fd);
	eis_client_unref(client);
}

_public_ int
eis_setup_backend_socket(struct eis *eis, const char *socketpath)
{
	assert(eis);
	assert(!eis->backend);
	assert(socketpath);
	assert(socketpath[0] != '\0');

	_unref_(eis_socket) *eis_socket = eis_socket_create(&eis->object);

	_cleanup_free_ char *path = NULL;
	if (socketpath[0] == '/') {
		path = xstrdup(socketpath);
	} else {
		const char *xdg = getenv("XDG_RUNTIME_DIR");
		if (!xdg)
			return -ENOTDIR;
		path = xaprintf("%s/%s", xdg, socketpath);
	}

	/* Create a lockfile first, if that succeeds but the real socket path exists
	 * it's a leftover from an unclean shutdown and we can remove the old
	 * socket file. */
	_cleanup_free_ char *lockfile = xaprintf("%s.lock", path);
	_cleanup_close_ int lockfd = open(lockfile, O_CREAT|O_CLOEXEC|O_RDWR,
					  (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP));
	int rc = flock(lockfd, LOCK_EX | LOCK_NB);
	if (rc < 0) {
		log_error(eis, "Failed to create lockfile %s, is another EIS running?", lockfile);
		return -errno;
	}

	struct stat st;
	rc = lstat(path, &st);
	if (rc < 0) {
		if (errno != ENOENT) {
			log_error(eis, "Failed to stat socket path %s (%s)",
				  path, strerror(errno));
			return -errno;
		}
	} else if (st.st_mode & (S_IWUSR|S_IWGRP)) {
		unlink(path);
	}

	/* Lockfile succeeded and path is unlinked (if it existed), let's set
	 * up the socket */
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
		.sun_path = {0},
	};
	if (!xsnprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path))
		return -EINVAL;

	_cleanup_close_ int sockfd = socket(AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK, 0);
	if (sockfd == -1)
		return -errno;

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
		return -errno;

	if (listen(sockfd, 2) == -1)
		return -errno;

	struct source *s = source_new(sockfd, listener_dispatch, eis_socket);
	rc = sink_add_source(eis->sink, s);
	if (rc == 0) {
		eis_socket->listener = source_ref(s);
		eis_socket->socketpath = steal(&path);
		eis_socket->lockpath = steal(&lockfile);
		eis_socket->lockfd = lockfd;
		eis->backend = steal(&eis_socket);
		eis->backend_interface = interface;
	}

	source_unref(s);
	sockfd = -1;
	lockfd = -1;

	return rc;
}
