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

/* A simple tool that connects to an EIS implementation via the RemoteDesktop
 * portal using liboeffis
 */

#include "config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <sys/poll.h>

#include "src/util-macros.h"
#include "src/util-mem.h"
#include "src/util-io.h"

#include "liboeffis.h"

#ifndef MESON_BUILDDIR
#error MESON_BUILDDIR must be defined
#endif

DEFINE_UNREF_CLEANUP_FUNC(oeffis);

static bool stop = false;

static void sighandler(int signal) {
	stop = true;
}

static void start_debug_events(int fd)
{
	pid_t pid = fork();
	assert(pid != -1);

	if (pid == 0) {
		_cleanup_free_ char *fdstr = xaprintf("%d", fd);
		execl(MESON_BUILDDIR "/ei-debug-events",
		      "ei-debug-events",
		      "--socketfd", fdstr, NULL);
		fprintf(stderr, "Failed to fork: %m\n");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	_unref_(oeffis) *oeffis = oeffis_new(NULL);

	signal(SIGINT, sighandler);

	struct pollfd fds = {
		.fd = oeffis_get_fd(oeffis),
		.events = POLLIN,
		.revents = 0,
	};

	oeffis_create_session(oeffis, OEFFIS_DEVICE_POINTER|OEFFIS_DEVICE_KEYBOARD);

	while (!stop && poll(&fds, 1, 1000) > -1) {
		oeffis_dispatch(oeffis);

		enum oeffis_event_type e = oeffis_get_event(oeffis);
		switch (e) {
		case OEFFIS_EVENT_NONE:
			break;
		case OEFFIS_EVENT_CONNECTED_TO_EIS: {
			_cleanup_close_ int eisfd = oeffis_get_eis_fd(oeffis);
			printf("# We have an eisfd: lucky number %d\n", eisfd);
			/* Note: unless we get closed/disconnected, we are
			 * started now, or will be once the compositor is
			 * happy with it.
			 *
			 * We still need to keep dispatching events though
			 * to be able to receive the Session.Closed signal
			 * or any disconnections. And we must keep the
			 * oeffis context alive - once the context is
			 * destroyed we disconnect from DBus which will
			 * cause the compositor or the portal to invalidate
			 * our EIS fd.
			 */
			start_debug_events(eisfd);
			break;
		}
		case OEFFIS_EVENT_DISCONNECTED:
			fprintf(stderr, "Disconnected: %s\n", oeffis_get_error_message(oeffis));
			stop = true;
			break;
		case OEFFIS_EVENT_CLOSED:
			printf("Closing\n");
			stop = true;
			break;
		}
	}

	return 0;
}
