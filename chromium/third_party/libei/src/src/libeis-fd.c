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
#include <sys/socket.h>
#include <sys/un.h>

#include "util-mem.h"
#include "util-macros.h"
#include "util-sources.h"
#include "util-strings.h"

#include "libeis.h"
#include "libeis-private.h"

struct eis_fd {
	struct object object;
};

static inline void
eis_fd_destroy(struct eis_fd *eis_fd)
{
}

static
OBJECT_IMPLEMENT_CREATE(eis_fd);
static
OBJECT_IMPLEMENT_UNREF_CLEANUP(eis_fd);

static void
interface_fd_destroy(struct eis *eis, void *backend)
{
	_unref_(eis_fd) *fd = backend;
}

static const struct eis_backend_interface interface = {
	.destroy = interface_fd_destroy,
};

_public_ int
eis_setup_backend_fd(struct eis *eis)
{
	assert(eis);
	assert(!eis->backend);

	eis->backend = eis_fd_create(&eis->object);
	eis->backend_interface = interface;

	return 0;
}

_public_ int
eis_backend_fd_add_client(struct eis *eis)
{
	assert(eis);
	assert(eis->backend);

	int fds[2];

	int rc = socketpair(AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0, fds);
	if (rc == -1)
		return -errno;

	struct eis_client *client = eis_client_new(eis, fds[0]);
	if (client == NULL)
		return -ENOMEM;

	eis_client_unref(client);

	return fds[1];
}
