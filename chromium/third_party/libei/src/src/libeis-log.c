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

#include <stdio.h>
#include <time.h>

#include "util-macros.h"
#include "util-color.h"
#include "util-object.h"
#include "util-strings.h"
#include "libeis-private.h"

struct eis_log_context {
	const char *file;
	const char *func;
	int line;
};

_public_
OBJECT_IMPLEMENT_GETTER(eis_log_context, file, const char *);
_public_
OBJECT_IMPLEMENT_GETTER(eis_log_context, func, const char *);
_public_
OBJECT_IMPLEMENT_GETTER(eis_log_context, line, unsigned int);

static void
eis_default_log_handler(struct eis *eis,
		        enum eis_log_priority priority,
			const char *message,
			struct eis_log_context *ctx)
{
	struct lut {
		const char *color;
		const char *prefix;
	} lut[] = {
		{ .color = ansi_colorcode[RED],		.prefix = "<undefined>", }, /* debug starts at 10 */
		{ .color = ansi_colorcode[HIGHLIGHT],	.prefix = "DEBUG", },
		{ .color = ansi_colorcode[GREEN],	.prefix = "INFO", },
		{ .color = ansi_colorcode[BLUE],	.prefix = "WARN", },
		{ .color = ansi_colorcode[RED],		.prefix = "ERROR", },
	};
	static time_t last_time = 0;
	const char *reset_code = ansi_colorcode[RESET];

	run_only_once {
		if (!isatty(STDERR_FILENO)) {
			struct lut *l;
			ARRAY_FOR_EACH(lut, l)
				l->color = "";
			reset_code = "";
		}
	}

	time_t now = time(NULL);
	char timestamp[64];

	if (last_time != now) {
		struct tm *tm = localtime(&now);
		strftime(timestamp, sizeof(timestamp), "%T", tm);
	} else {
		xsnprintf(timestamp, sizeof(timestamp), "...");
	}

	size_t idx = priority/10;
	assert(idx < ARRAY_LENGTH(lut));
	fprintf(stderr, " EIS: %8s | %s%4s%s | %s\n", timestamp,
		lut[idx].color, lut[idx].prefix, reset_code, message);

	last_time = now;
}

_public_ void
eis_log_set_handler(struct eis *eis, eis_log_handler log_handler)
{
	eis->log.handler = log_handler ? log_handler : eis_default_log_handler;
}

_public_ void
eis_log_set_priority(struct eis *eis, enum eis_log_priority priority)
{
	switch (priority) {
	case EIS_LOG_PRIORITY_DEBUG:
	case EIS_LOG_PRIORITY_INFO:
	case EIS_LOG_PRIORITY_WARNING:
	case EIS_LOG_PRIORITY_ERROR:
		break;
	default:
		abort();
	}
	eis->log.priority = priority;
}

_public_ enum eis_log_priority
eis_log_get_priority(const struct eis *eis)
{
	return eis->log.priority;
}

void
eis_log_msg(struct eis *eis,
	   enum eis_log_priority priority,
	   const char *file, int lineno, const char *func,
	   const char *format, ...)
{
	va_list args;

	va_start(args, format);
	eis_log_msg_va(eis, priority, file, lineno, func, format, args);
	va_end(args);
}

void
eis_log_msg_va(struct eis *eis,
	      enum eis_log_priority priority,
	      const char *file, int lineno, const char *func,
	      const char *format,
	      va_list ap)
{
	if (eis->log.priority > priority)
		return;

	_cleanup_free_ char *message = xvaprintf(format, ap);
	struct eis_log_context ctx = {
		.file = file,
		.line = lineno,
		.func = func,
	};
	eis->log.handler(eis, priority, message, &ctx);
}

#ifdef _enable_tests_
#include "util-munit.h"

struct log_handler_check {
	enum eis_log_priority min_priority;
	const char *expected_message;
};

static void
test_loghandler(struct eis *eis,
		enum eis_log_priority priority,
		const char *message,
		struct eis_log_context *ctx)
{
	struct log_handler_check *check = eis_get_user_data(eis);

	munit_assert_int(priority, >=, check->min_priority);
	munit_assert_string_equal(message, check->expected_message);
	munit_assert_ptr_not_null(ctx);
	/* Second arg is the line number, if this code ever moves further up,
	 * this test may fail */
	munit_assert_int(eis_log_context_get_line(ctx), >, 170);
	munit_assert_true(strendswith(eis_log_context_get_file(ctx), "libeis-log.c"));
	munit_assert_string_equal(eis_log_context_get_func(ctx), "test_log_handler");
}

MUNIT_TEST(test_log_handler)
{
	struct log_handler_check check = {0};
	struct eis *eis = eis_new(&check);

	munit_assert_ptr_equal(eis->log.handler, eis_default_log_handler);
	munit_assert_int(eis->log.priority, ==, EIS_LOG_PRIORITY_INFO);

	eis_log_set_handler(eis, test_loghandler);

	check.min_priority = EIS_LOG_PRIORITY_INFO; /* default */
	check.expected_message = "info message";

	log_debug(eis, "default is below this level");
	log_info(eis, "info message");

	check.expected_message = "error message";
	log_error(eis, "error message");

	check.min_priority = EIS_LOG_PRIORITY_ERROR;
	eis_log_set_priority(eis, EIS_LOG_PRIORITY_ERROR);
	munit_assert_int(eis->log.priority, ==, EIS_LOG_PRIORITY_ERROR);
	log_error(eis, "error message");
	log_warn(eis, "warn message");
	log_info(eis, "info message");
	log_debug(eis, "debug message");

	eis_log_set_priority(eis, EIS_LOG_PRIORITY_DEBUG);

	/* Make sure they come through at the right priority */
	check.min_priority = EIS_LOG_PRIORITY_ERROR;
	check.expected_message = "error message";
	log_error(eis, "error message");
	check.min_priority = EIS_LOG_PRIORITY_WARNING;
	check.expected_message = "warn message";
	log_warn(eis, "warn message");
	check.min_priority = EIS_LOG_PRIORITY_INFO;
	check.expected_message = "info message";
	log_info(eis, "info message");
	check.min_priority = EIS_LOG_PRIORITY_DEBUG;
	check.expected_message = "debug message";
	log_debug(eis, "debug message");

	/* Can't capture this easily, so this is mostly just a crash test */
	eis_log_set_handler(eis, NULL);
	munit_assert_ptr_equal(eis->log.handler, eis_default_log_handler);
	log_debug(eis, "ignore this, testing NULL log handler");

	eis_unref(eis);

	return MUNIT_OK;
}
#endif
