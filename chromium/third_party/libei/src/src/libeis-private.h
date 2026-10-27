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

#pragma once

#include <stdarg.h>
#include <inttypes.h>

#include "util-object.h"

#include "libeis.h"
#include "brei-shared.h"
#include "util-macros.h"
#include "util-list.h"
#include "util-sources.h"
#include "util-structs.h"

#include "libeis-button.h"
#include "libeis-callback.h"
#include "libeis-client.h"
#include "libeis-connection.h"
#include "libeis-device.h"
#include "libeis-event.h"
#include "libeis-handshake.h"
#include "libeis-keyboard.h"
#include "libeis-pingpong.h"
#include "libeis-pingpong.h"
#include "libeis-pointer-absolute.h"
#include "libeis-pointer.h"
#include "libeis-region.h"
#include "libeis-scroll.h"
#include "libeis-seat.h"
#include "libeis-touchscreen.h"

struct eis_backend_interface {
	void (*destroy)(struct eis *eis, void *backend);
};

struct eis {
	struct object object;
	void *user_data;
	struct sink *sink;
	struct list clients;

	struct eis_backend_interface backend_interface;
	void *backend;
	struct list event_queue;

	struct {
		eis_log_handler handler;
		enum eis_log_priority priority;
	} log;

	eis_clock_now_func clock_now;

	const struct eis_proto_requests *requests;
};

void
eis_init_object(struct eis *eis, struct object *parent);

int
eis_init(struct eis *eis);

struct eis *
eis_get_context(struct eis *eis);

void
eis_queue_connect_event(struct eis_client *client);

void
eis_queue_disconnect_event(struct eis_client *client);

void
eis_queue_pong_event(struct eis_client *client, struct eis_ping *ping);

void
eis_queue_sync_event(struct eis_client *client, object_id_t newid, uint32_t version);

void
eis_queue_seat_bind_event(struct eis_seat *seat, uint32_t capabilities);

void
eis_queue_device_closed_event(struct eis_device *device);

void
eis_queue_frame_event(struct eis_device *device, uint64_t time);

void
eis_queue_device_start_emulating_event(struct eis_device *device, uint32_t sequence);

void
eis_queue_device_stop_emulating_event(struct eis_device *device);

void
eis_queue_pointer_rel_event(struct eis_device *device, double x, double y);

void
eis_queue_pointer_abs_event(struct eis_device *device,
			    double x, double y);

void
eis_queue_pointer_button_event(struct eis_device *device, uint32_t button,
			       bool is_press);

void
eis_queue_pointer_scroll_event(struct eis_device *device,
			       double x, double y);

void
eis_queue_pointer_scroll_discrete_event(struct eis_device *device,
					int32_t x, int32_t y);
void
eis_queue_pointer_scroll_stop_event(struct eis_device *device, bool x, bool y);

void
eis_queue_pointer_scroll_cancel_event(struct eis_device *device, bool x, bool y);

void
eis_queue_keyboard_key_event(struct eis_device *device, uint32_t key,
			     bool is_press);

void
eis_queue_touch_down_event(struct eis_device *device, uint32_t touchid,
			   double x, double y);

void
eis_queue_touch_motion_event(struct eis_device *device, uint32_t touchid,
			     double x, double y);

void
eis_queue_touch_up_event(struct eis_device *device, uint32_t touchid);

void
eis_queue_touch_cancel_event(struct eis_device *device, uint32_t touchid);

_printf_(6, 7) void
eis_log_msg(struct eis *eis,
	    enum eis_log_priority priority,
	    const char *file, int lineno, const char *func,
	    const char *format, ...);

_printf_(6, 0) void
eis_log_msg_va(struct eis *eis,
	       enum eis_log_priority priority,
	       const char *file, int lineno, const char *func,
	       const char *format,
	       va_list args);

#define log_debug(T_, ...) \
	eis_log_msg((T_), EIS_LOG_PRIORITY_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_info(T_, ...) \
	eis_log_msg((T_), EIS_LOG_PRIORITY_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_warn(T_, ...) \
	eis_log_msg((T_), EIS_LOG_PRIORITY_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_error(T_, ...) \
	eis_log_msg((T_), EIS_LOG_PRIORITY_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define log_bug(T_, ...) \
	eis_log_msg((T_), EIS_LOG_PRIORITY_ERROR, __FILE__, __LINE__, __func__, "🪳  libeis bug:  " __VA_ARGS__)
#define log_bug_client(T_, ...) \
	eis_log_msg((T_), EIS_LOG_PRIORITY_ERROR, __FILE__, __LINE__, __func__, "🪲  Bug: " __VA_ARGS__)

#define DISCONNECT_IF_INVALID_VERSION(eis_client_, intf_, id_, version_) do { \
	struct eis_client *_client = (eis_client_); \
	uint32_t _version = (version_); \
	uint64_t _id = (id_); \
	if (_client->interface_versions.intf_ < _version) { \
		struct eis *_eis = eis_client_get_context(_client); \
		log_bug(_eis, "Received invalid version %u for object id %#" PRIx64 ". Disconnecting", _version, _id); \
		return brei_result_new(EIS_CONNECTION_DISCONNECT_REASON_PROTOCOL, "Received invalid version %u for object id %#" PRIx64 ".", _version, _id); \
	} \
} while(0)
