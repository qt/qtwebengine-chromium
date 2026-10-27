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

#include <sys/resource.h>

#include "util-munit.h"
#include "util-mem.h"
#include "util-strings.h"

/* All MUNIT_TEST functions are in the test_functions_section ELF section.
 * __start and __stop point to the start and end of that section. See the
 * __attribute__(section) documentation.
 */
DECLARE_TEST_SECTION();
DECLARE_GLOBAL_SETUP_SECTION();

/* If the tests don't use MUNIT_GLOBAL_SETUP the the above has no linkage, so
 * let's always define a noop internal one.
 */
MUNIT_GLOBAL_SETUP(__internal)
{
}

int
munit_tests_run(int argc, char **argv)
{
	size_t count = 1; /* For NULL-terminated entry */

	foreach_test(t) {
		count++;
	}

	_cleanup_free_ MunitTest *tests = calloc(count, sizeof(*tests));
	size_t idx = 0;
	foreach_test(t) {
		MunitTest test = {
			.name = xaprintf("%s", t->name),
			.test = t->func,
		};
		tests[idx++] = test;
	}

	struct munit_setup setup = {
		.argc = argc,
		.argv = argv,
		.userdata = NULL,
	};
	size_t setup_count = 0;
	foreach_setup(s) {
		if (!streq(s->name, "__internal")) {
			assert(setup_count == 0); /* Only one setup func per suite */
			s->func(&setup);
			++setup_count;
		}
	}

	MunitSuite suite = {
		"",
		tests,
		NULL,
		1,
		MUNIT_SUITE_OPTION_NONE,
	};

	/* Disable coredumps */
	const struct rlimit corelimit = { 0, 0 };
	setrlimit(RLIMIT_CORE, &corelimit);

	int rc = munit_suite_main(&suite, setup.userdata, setup.argc, setup.argv);

	for (idx = 0; idx < count; idx++)
		free(tests[idx].name);

	return rc;
}
