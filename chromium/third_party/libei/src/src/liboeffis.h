/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Red Hat, Inc.
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @addtogroup liboeffis 🚌 liboeffis - An XDG RemoteDesktop portal wrapper API
 *
 * liboeffis is a helper library for applications that do not want to or cannot
 * interact with the XDG RemoteDesktop DBus portal directly.
 *
 * liboeffis will:
 * - connect to the DBus session bus and the ``org.freedesktop.portal.Desktop`` bus name
 * - Start a ``org.freedesktop.portal.RemoteDesktop`` session, select the devices and invoke
 *   ``RemoteDesktop.ConnectToEIS()``
 * - Close everything in case of error or disconnection
 *
 * liboeffis is intentionally kept simple, any more complex needs should be
 * handled by an application talking to DBus directly.
 *
 * @{
 */

/**
 * @struct oeffis
 *
 * The main context to interact with liboeffis. A liboeffis context is a single
 * connection to DBus session bus and must only be used once.
 *
 * @note A caller must keep the oeffis context alive after a successful
 * connection. Destroying the context results in the DBus connection being
 * closed and that again results in our EIS fd being invalidated by the
 * portal and/or the EIS implementation.
 *
 * An @ref oeffis context is refcounted, see oeffis_unref().
 */
struct oeffis;

/**
 * Create a new oeffis context. The context is refcounted and must be
 * released with oeffis_unref().
 */
struct oeffis *
oeffis_new(void *user_data);

/**
 * Increase the refcount of this struct by one. Use oeffis_unref() to decrease
 * the refcount.
 *
 * @return the argument passed into the function
 */
struct oeffis *
oeffis_ref(struct oeffis *oeffis);

/**
 * Decrease the refcount of this struct by one. When the refcount reaches
 * zero, the context disconnects from DBus and all allocated resources
 * are released.
 *
 * @note A caller must keep the oeffis context alive after a successful
 * connection. Destroying the context results in the DBus connection being
 * closed and that again results in our EIS fd being invalidated by the
 * portal and/or the EIS implementation.
 *
 * @return always NULL
 */
struct oeffis *
oeffis_unref(struct oeffis *oeffis);

/**
 * Set a custom data pointer for this context. liboeffis will not look at or
 * modify the pointer. Use oeffis_get_user_data() to retrieve a previously set
 * user data.
 */
void
oeffis_set_user_data(struct oeffis *oeffis, void *user_data);

/**
 * Return the custom data pointer for this context. liboeffis will not look at or
 * modify the pointer. Use oeffis_set_user_data() to change the user data.
 */
void *
oeffis_get_user_data(struct oeffis *oeffis);

/**
 * liboeffis keeps a single file descriptor for all events. This fd should be
 * monitored for events by the caller's mainloop, e.g. using select(). When
 * events are available on this fd, call oeffis_dispatch() immediately to
 * process the events.
 */
int
oeffis_get_fd(struct oeffis *oeffis);

/**
 * Get a `dup()` of the file descriptor. This function should only be called
 * after an event of type @ref OEFFIS_EVENT_CONNECTED_TO_EIS. Otherwise,
 * this function returns -1 and errno is set to the `dup()` error.
 * If this function is called when liboeffis is not connected to EIS, the errno
 * is set to `ENODEV`.
 *
 * Repeated calls to this functions will return additional duplicated file
 * descriptors. There is no need for a well-written application to call this
 * function more than once.
 *
 * The caller is responsible for closing the returned fd.
 *
 * @return The EIS fd or -1 on failure or before the fd was retrieved.
 */
int
oeffis_get_eis_fd(struct oeffis *oeffis);

/**
 * The bitmask of devices to request. This bitmask matches the devices
 * bitmask in the XDG RemoteDesktop portal.
 */
enum oeffis_device {
	/**
	 * Request all devices. Note that this is not a simple mask of the
	 * other enum values here but it relies on the RemoteDesktop portal
	 * stack to use the default "all" value. The exact set of devices
	 * is thus dependent on the implementation - running against an
	 * older portal stack may mean a subset of the devices here, running
	 * against a portal stack more modern than liboeffis may mean
	 * selecting for devices unknown to liboeffis at compile time.
	 *
	 * If in doubt, use an explicit mask of desired devices instead.
	 */
	OEFFIS_DEVICE_ALL_DEVICES = 0,
	OEFFIS_DEVICE_KEYBOARD = (1 << 0),
	OEFFIS_DEVICE_POINTER = (1 << 1),
	OEFFIS_DEVICE_TOUCHSCREEN = (1 << 2),
};

/**
 * Connect this oeffis instance to a RemoteDesktop session with the given device
 * mask selected.
 *
 * This initiates the DBus communication, starts a RemoteDesktop session and
 * selects the devices. On success, the @ref
 * OEFFIS_EVENT_CONNECTED_TO_EIS event is created and the EIS fd can be
 * retrieved with oeffis_get_eis_fd().
 *
 * Any failure in the above process or any other DBus communication error
 * once connected, including caller bugs, result in the oeffis context being
 * disconnected and an @ref OEFFIS_EVENT_DISCONNECTED event. Once
 * disconnected, the context should be released with oeffis_unref().
 * An @ref OEFFIS_EVENT_DISCONNECTED indicates a communication error and
 * oeffis_get_error_message() is set with an appropriate error message.
 *
 * If the RemoteDesktop session is closed by the
 * compositor, an @ref OEFFIS_EVENT_CLOSED event is created and the context
 * should be released with oeffis_unref(). Unlike a disconnection, an @ref
 * OEFFIS_EVENT_CLOSED event signals intentional closure by the portal. For
 * example, this may happen as a result of user interaction to terminate the
 * RemoteDesktop session.
 *
 * @note A caller must keep the oeffis context alive after a successful
 * connection. Destroying the context results in the DBus connection being
 * closed and that again results in our EIS fd being invalidated by the
 * portal and/or the EIS implementation.
 *
 * @warning Due to the asynchronous nature of DBus and libei, it is not
 * guaranteed that an event of type @ref OEFFIS_EVENT_DISCONNECTED or @ref
 * OEFFIS_EVENT_CLOSED is received before the EIS fd becomes invalid.
 *
 * @param oeffis A new oeffis context
 * @param devices A bitmask of @ref oeffis_device
 */
void
oeffis_create_session(struct oeffis *oeffis, uint32_t devices);

/**
 * See oeffis_create_session() but this function allows to specify the busname to connect to.
 * This function should only be used for testing.
 */
void
oeffis_create_session_on_bus(struct oeffis *oeffis, const char *busname, uint32_t devices);

enum oeffis_event_type {
	OEFFIS_EVENT_NONE = 0, /**< No event currently available */
	OEFFIS_EVENT_CONNECTED_TO_EIS, /**< The RemoteDesktop session was created and an eis fd is available */
	OEFFIS_EVENT_CLOSED, /**< The session was closed by the compositor or portal */
	OEFFIS_EVENT_DISCONNECTED, /**< We were disconnected from the Bus due to an error */
};

/**
 * Process pending events. This function must be called immediately after
 * the file descriptor returned by oeffis_get_fd() signals data is
 * available.
 *
 * After oeffis_dispatch() completes, zero or more events may be available
 * by oeffis_get_event().
 */
void
oeffis_dispatch(struct oeffis *oeffis);

/**
 * Return the next available event, if any. If no event is currently
 * available, @ref OEFFIS_EVENT_NONE is returned.
 *
 * Calling oeffis_dispatch() does not guarantee events are available to the
 * caller. A single call oeffis_dispatch() may cause more than one event to
 * be available.
 */
enum oeffis_event_type
oeffis_get_event(struct oeffis *oeffis);

/**
 * If the session was @ref OEFFIS_EVENT_DISCONNECTED, return the error message
 * that caused the disconnection. The returned string is owned by the oeffis context.
 */
const char *
oeffis_get_error_message(struct oeffis *oeffis);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
