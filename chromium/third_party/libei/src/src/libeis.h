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

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup libeis 🍦 EIS - The server API
 *
 * libeis is the server-side module. This API should be used by processes
 * that have control over input devices, e.g. Wayland compositors.
 *
 * For an example EIS implementation see the [`tools/` directory in the libei
 * repository](https://gitlab.freedesktop.org/libinput/libei/-/tree/main/tools).
 * At its core, an EIS implementation will
 * - create a context with eis_new()
 * - set up a backend with eis_setup_backend_fd() or eis_setup_backend_socket()
 * - register the eis_get_fd() with its own event loop
 * - call eis_dispatch() whenever the fd triggers
 * - call eis_get_event() and process incoming events
 *
 * And for each client:
 * - accept new clients with eis_client_connect()
 * - create one or more seats for the client with eis_client_new_seat()
 * - wait for @ref EIS_EVENT_SEAT_BIND and then
 * - create one or more devices with the bound capabilities, see
 eis_seat_new_device()
 *
 * libei clients come in "sender" and "receiver" modes, depending on whether
 * the client sends or receives events. A libeis context however may accept
 * both sender and receiver clients, the EIS implementation works as
 * corresponding receiver or sender for this client. It is up to the
 * implementation to disconnect clients that it does not want to allow. See
 * eis_client_is_sender() for details.
 *
 * @defgroup libeis-log The logging API
 * @ingroup libeis
 *
 * The API to control logging output.
 *
 * @defgroup libeis-receiver API for receiver clients
 * @ingroup libeis
 *
 * The receiver API is available only to clients that are not
 * eis_client_is_sender(). For those clients the EIS implemententation creates
 * devices and sends events **to** the libei client. IOW the EIS implementation
 * acts as the sender. The primary use-case
 * for this is input capturing, e.g. InputLeap.
 *
 * It is a client bug to call any of these functions for a client created
 * with ei_new_sender().
 *
 * @defgroup libeis-sender API for sender clients
 * @ingroup libeis
 *
 * This API is available only on clients that are
 * eis_client_is_sender(). For those clients the EIS implemententation creates
 * devices and but it is the libei client that sends events **to** EIS
 implementation. IOW the EIS implementation acts as the receiver.
 * The primary use-case is input emulation from a client, akin to xdotool.
 *
 * It is a client bug to call any of these functions for a client created
 * with ei_new_receiver().
 *
 * @defgroup libeis-seat The seat API
 * @ingroup libeis
 *
 * The API to query and interact with a struct @ref eis_seat
 *
 * @defgroup libeis-device The device API
 * @ingroup libeis
 *
 * The API to query and interact with a struct @ref eis_device
 *
 * @defgroup libeis-region The region API
 * @ingroup libeis
 *
 * The API to query a struct @ref eis_region for information
 *
 * @defgroup libeis-keymap The keymap API
 * @ingroup libeis
 *
 * The API to query a struct @ref eis_keymap for information
 *
 */

/**
 * @addtogroup libeis
 * @{
 */

/**
 * @struct eis
 */
struct eis;
/**
 * @struct eis_client
 */
struct eis_client;
/**
 * @struct eis_device
 */
struct eis_device;
/**
 * @struct eis_seat
 */
struct eis_seat;
/**
 * @struct eis_event
 */
struct eis_event;
/**
 * @struct eis_keymap
 */
struct eis_keymap;
/**
 * @struct eis_touch
 */
struct eis_touch;

/**
 * @struct eis_ping
 *
 * A callback struct returned by eis_new_ping() to handle
 * roundtrips to the client.
 *
 * @see eis_new_ping
 */
struct eis_ping;

/**
 * @struct eis_region
 * @ingroup libeis-region
 *
 * Regions are only available on devices of type @ref EIS_DEVICE_TYPE_VIRTUAL.
 *
 * A rectangular region, defined by an x/y offset and a width and a height.
 * A region defines the area on an EIS desktop layout that is accessible by
 * this device - this region may not be the full area of the desktop.
 * Input events may only be sent for points within the regions.
 *
 * The use of regions is private to the EIS compositor and coordinates do not
 * need match the size of the actual desktop. For example, a compositor may
 * set a 1920x1080 region to represent a 4K monitor and transparently map
 * input events into the respective true pixels.
 *
 * Absolute devices may have different regions, it is up to the libei client
 * to send events through the correct device to target the right pixel. For
 * example, a dual-head setup my have two absolute devices, the first with a
 * zero offset region spanning the first screen, the second with a nonzero
 * offset spanning the second screen.
 *
 * Regions should not overlap, no behavior for overlapping regions has yet
 * been defined but this may change in the future.
 *
 * Regions must be assigned when the device is created and are static for the
 * lifetime of the device. See eis_device_new_region() and eis_region_add().
 */
struct eis_region;

/**
 * @enum eis_device_type
 * @ingroup libeis-device
 *
 * The device type determines what the device represents.
 *
 * If the device type is @ref EIS_DEVICE_TYPE_VIRTUAL, the device is a
 * virtual device representing input as applied on the EIS implementation's
 * screen. A relative virtual device generates input events in logical pixels,
 * an absolute virtual device generates input events in logical pixels on one
 * of the device's regions. Virtual devices do not have a size.
 *
 * If the device type is @ref EIS_DEVICE_TYPE_PHYSICAL, the device is a
 * representation of a physical device as if connected to the EIS
 * implementation's host computer. A relative physical device generates input
 * events in mm, an absolute physical device generates input events in mm
 * within the device's specified physical size. Physical devices do not have
 * regions.
 *
 * @see eis_device_get_width
 * @see eis_device_get_height
 */
enum eis_device_type {
	EIS_DEVICE_TYPE_VIRTUAL = 1,
	EIS_DEVICE_TYPE_PHYSICAL
};

/**
 * @enum eis_device_capability
 * @ingroup libeis-device
 */
enum eis_device_capability {
	EIS_DEVICE_CAP_POINTER = (1 << 0),
	EIS_DEVICE_CAP_POINTER_ABSOLUTE = (1 << 1),
	EIS_DEVICE_CAP_KEYBOARD = (1 << 2),
	EIS_DEVICE_CAP_TOUCH = (1 << 3),
	EIS_DEVICE_CAP_SCROLL = (1 << 4),
	EIS_DEVICE_CAP_BUTTON = (1 << 5),
};

/**
 * @enum eis_keymap_type
 * @ingroup libeis-keymap
 */
enum eis_keymap_type {
	EIS_KEYMAP_TYPE_XKB = 1,
};

/**
 * @enum eis_event_type
 *
 * This enum is not exhaustive, future versions of this library may add
 * new event types.
 *
 * Unknown events must be released by the caller with eis_event_unref(),
 * see eis_get_event().
 */
enum eis_event_type {
	/**
	 * A client has connected. This is the first event from any new
	 * client.
	 * The server is expected to either call eis_client_connect() or
	 * eis_client_disconnect().
	 */
	EIS_EVENT_CLIENT_CONNECT = 1,
	/**
	 * The client has disconnected, any pending requests for this client
	 * should be discarded.
	 */
	EIS_EVENT_CLIENT_DISCONNECT,

	/**
	 * The client wants to bind or unbind a capability on this seat.
	 * Devices associated with this seat should be sent to the client or
	 * removed if the capability is no longer bound
	 */
	EIS_EVENT_SEAT_BIND,

	/**
	 * The client no longer listens to events from this device. The caller
	 * should released resources associated with this device.
	 */
	EIS_EVENT_DEVICE_CLOSED,

	/**
	 * Returned in response to eis_ping().
	 */
	EIS_EVENT_PONG = 90,

	/**
	 * This event represents a synchronization request (ping) from the client
	 * implementation. The corresponding reply (pong) will be sent when it
	 * is unref'd. It has no other API.
	 *
	 * The caller must ensure that any state changes triggered by messages
	 * received prior to this event have been resolved and communicated to the
	 * client prior to calling eis_event_unref(). E.g., if the preceding frame
	 * included a key event that caused shift to become logically depressed, the
	 * caller should ensure that the updated modifier state has been sent before
	 * calling unref.
	 */
	EIS_EVENT_SYNC,

	/**
	 * "Hardware" frame event. This event **must** be sent by the client
	 * and notifies the server that the previous set of events belong to
	 * the same logical hardware event.
	 *
	 * These events are only generated on a receiving EIS context.
	 *
	 * This event is most commonly used to implement multitouch (multiple
	 * touches may update within the same hardware scanout cycle).
	 */
	EIS_EVENT_FRAME = 100,

	/**
	 * The client is about to send events for a device. This event should
	 * be used by the server to clear the logical state of the emulated
	 * devices and/or provide UI to the user.
	 *
	 * These events are only generated on a receiving EIS context.
	 *
	 * Note that a client start multiple emulating sequences
	 * simultaneously, depending on the devices it received from the
	 * server. For example, in a synergy-like situation, the client
	 * may start emulating of pointer and keyboard once the remote device
	 * logically entered the screen.
	 *
	 * The server can cancel an ongoing emulating by suspending the
	 * device, see eis_device_suspend().
	 */
	EIS_EVENT_DEVICE_START_EMULATING = 200,
	/**
	 * Stop emulating events on this device, see @ref EIS_EVENT_DEVICE_START_EMULATING.
	 */
	EIS_EVENT_DEVICE_STOP_EMULATING,

	/* These events are only generated on a receiving EIS context */

	/**
	 * A relative motion event with delta coordinates in logical pixels or
	 * mm, depending on the device type.
	 */
	EIS_EVENT_POINTER_MOTION = 300,
	/**
	 * An absolute motion event with absolute position within the device's
	 * regions or size, depending on the device type.
	 */
	EIS_EVENT_POINTER_MOTION_ABSOLUTE = 400,
	/**
	 * A button press or release event
	 */
	EIS_EVENT_BUTTON_BUTTON = 500,
	/**
	 * A vertical and/or horizontal scroll event with logical-pixels
	 * or mm precision, depending on the device type.
	 */
	EIS_EVENT_SCROLL_DELTA = 600,
	/**
	 * An ongoing scroll sequence stopped.
	 */
	EIS_EVENT_SCROLL_STOP,
	/**
	 * An ongoing scroll sequence was cancelled.
	 */
	EIS_EVENT_SCROLL_CANCEL,
	/**
	 * A vertical and/or horizontal scroll event with a discrete range in
	 * logical scroll steps, like a scroll wheel.
	 */
	EIS_EVENT_SCROLL_DISCRETE,

	/**
	 * A key press or release event
	 */
	EIS_EVENT_KEYBOARD_KEY = 700,

	/**
	 * Event for a single touch set down on the device's logical surface.
	 * A touch sequence is always down/up with an optional motion event in
	 * between. On multitouch capable devices, several touchs eqeuences
	 * may be active at any time.
	 */
	EIS_EVENT_TOUCH_DOWN = 800,
	/**
	 * Event for a single touch released from the device's logical
	 * surface.
	 */
	EIS_EVENT_TOUCH_UP,
	/**
	 * Event for a single currently-down touch changing position (or other
	 * properties).
	 */
	EIS_EVENT_TOUCH_MOTION,
};

/**
 * This is a debugging helper to return a string of the name of the event type,
 * or NULL if the event type is invalid. For example, for @ref EIS_EVENT_TOUCH_UP this
 * function returns the string "EIS_EVENT_TOUCH_UP".
 */
const char*
eis_event_type_to_string(enum eis_event_type);

/**
 * Create a new libeis context with a refcount of 1.
 */
struct eis *
eis_new(void *user_data);

/**
 * @ingroup libeis-log
 */
enum eis_log_priority {
	EIS_LOG_PRIORITY_DEBUG = 10,
	EIS_LOG_PRIORITY_INFO = 20,
	EIS_LOG_PRIORITY_WARNING = 30,
	EIS_LOG_PRIORITY_ERROR = 40,
};

/**
 * @ingroup libeis-log
 */
struct eis_log_context;

/**
 * @ingroup libeis-log
 * @return the line number (``__LINE__``) for a given log message context.
 */
unsigned int
eis_log_context_get_line(struct eis_log_context *ctx);

/**
 * @ingroup libeis-log
 * @return the file name (``__FILE__``) for a given log message context.
 */
const char *
eis_log_context_get_file(struct eis_log_context *ctx);

/**
 * @ingroup libeis-log
 * @return the function name (``__func__``) for a given log message context.
 */
const char *
eis_log_context_get_func(struct eis_log_context *ctx);

/**
 * @ingroup libeis-log
 *
 * The log handler for library logging. This handler is only called for
 * messages with a log level equal or greater than than the one set in
 * eis_log_set_priority().
 *
 * @param eis The EIs context
 * @param priority The log priority
 * @param message The log message as a null-terminated string
 * @param ctx A log message context for this message
 */
typedef void (*eis_log_handler)(struct eis *eis,
				enum eis_log_priority priority,
				const char *message,
				struct eis_log_context *ctx);
/**
 * @ingroup libeis-log
 *
 * Change the log handler for this context. If the log handler is NULL, the
 * built-in default log function is used.
 *
 * @param eis The EIS context
 * @param log_handler The log handler or NULL to use the default log
 * handler.
 */
void
eis_log_set_handler(struct eis *eis, eis_log_handler log_handler);

/**
 * @ingroup libeis-log
 */
void
eis_log_set_priority(struct eis *eis, enum eis_log_priority priority);

/**
 * @ingroup libeis-log
 */
enum eis_log_priority
eis_log_get_priority(const struct eis *eis);

/**
 * Optional override function for eis_now().
 *
 * By default eis_now() returns the current timestamp in CLOCK_MONOTONIC. This
 * may be overridden by a caller to provide a different timestamp.
 *
 * There is rarely a need to override this function. It exists for the libeis-internal test suite.
 */
typedef uint64_t (*eis_clock_now_func)(struct eis *eis);

/**
 * Override the function that returns the current time eis_now().
 *
 * There is rarely a need to override this function. It exists for the libeis-internal test suite.
 */
void
eis_clock_set_now_func(struct eis *, eis_clock_now_func func);

struct eis *
eis_ref(struct eis *eis);

struct eis *
eis_unref(struct eis *eis);

void *
eis_get_user_data(struct eis *eis);

void
eis_set_user_data(struct eis *eis, void *user_data);

/**
 * Initialize the context that can take pre-configured socket file descriptors,
 * see eis_backend_fd_add_client().
 *
 * This is the preferred backend to use for EIS implementations as it keeps
 * the file descriptors private for each client and is not subject to race
 * conditions or unauthorized clients attempting to connect.
 */
int
eis_setup_backend_fd(struct eis *ctx);

/**
 * Add a new client to a context set up with eis_setup_backend_fd(). Returns
 * a file descriptor to be passed to ei_setup_backend_fd(), or a negative
 * errno on failure.
 */
int
eis_backend_fd_add_client(struct eis *ctx);

/**
 * Initialize the context with a UNIX socket name.
 * If the path does not start with / it is relative to $XDG_RUNTIME_DIR.
 *
 * The socket will be accessible by any client with permissions to do so and
 * it is up to the EIS implementation to authenticate clients. In almost
 * all cases, the EIS implementation should use eis_setup_backend_fd() instead.
 * This backend is primarily useful for testing and debugging.
 */
int
eis_setup_backend_socket(struct eis *ctx, const char *path);

/**
 * libeis keeps a single file descriptor for all events. This fd should be
 * monitored for events by the caller's mainloop, e.g. using select(). When
 * events are available on this fd, call eis_dispatch() immediately to
 * process.
 */
int
eis_get_fd(struct eis *eis);

/**
 * Main event dispatching function. Reads events of the file descriptors
 * and processes them internally. Use eis_get_event() to retrieve the
 * events.
 *
 * Dispatching does not necessarily queue events. This function
 * should be called immediately once data is available on the file
 * descriptor returned by eis_get_fd().
 */
void
eis_dispatch(struct eis *eis);

/**
 * Return a unique, increasing id for this struct. This ID is assigned at
 * struct creation and constant for the lifetime of the struct. The ID
 * does not exist at the protocol level.
 *
 * This is a convenience API to make it easier to put this struct into
 * e.g. a hashtable without having to use the pointer value itself.
 *
 * The ID increases by an unspecified amount.
 *
 * @since 1.4
 */
uint64_t
eis_ping_get_id(struct eis_ping *ping);

/**
 * Increase the refcount of this struct by one. Use eis_ping_unref() to decrease
 * the refcount.
 *
 * @return the argument passed into the function
 *
 * @since 1.4
 */
struct eis_ping *
eis_ping_ref(struct eis_ping *eis_ping);

/**
 * Decrease the refcount of this struct by one. When the refcount reaches
 * zero, the context disconnects from the server and all allocated resources
 * are released.
 *
 * @return always NULL
 *
 * @since 1.4
 */
struct eis_ping *
eis_ping_unref(struct eis_ping *eis_ping);

/**
 * Set a custom data pointer for this struct. libeis will not look at or
 * modify the pointer. Use eis_ping_get_user_data() to retrieve a previously set
 * user data.
 *
 * @since 1.4
 */
void
eis_ping_set_user_data(struct eis_ping *eis_ping, void *user_data);

/**
 * Return the custom data pointer for this struct. libeis will not look at or
 * modify the pointer. Use eis_ping_set_user_data() to change the user data.
 *
 * @since 1.4
 */
void *
eis_ping_get_user_data(struct eis_ping *eis_ping);

/**
 * Issue a roundtrip request to the client, resulting in
 * an @ref EIS_EVENT_PONG event when this roundtrip has been processed. Events
 * are processed in-order by the client implementation so this
 * function can be used as synchronization point between events.
 *
 * If the client is disconnected before the roundtrip is complete,
 * libeis will emulate a @ref EIS_EVENT_PONG event before @ref
 * EIS_EVENT_DISCONNECT.
 *
 * @since 1.4
 */
void
eis_ping(struct eis_ping *ping);

/**
 * Returns the next event in the internal event queue (or NULL) and removes
 * it from the queue.
 *
 * The returned event is refcounted, use eis_event_unref() to drop the
 * reference even if the event type is unknown to the caller.
 *
 * You must not call this function while holding a reference to an event
 * returned by eis_peek_event().
 */
struct eis_event *
eis_get_event(struct eis *eis);

/**
 * Returns the next event in the internal event queue (or NULL) without
 * removing that event from the queue, i.e. the next call to eis_get_event()
 * will return that same event.
 *
 * This call is useful for checking whether there is an event and/or what
 * type of event it is.
 *
 * Repeated calls to eis_peek_event() return the same event.
 *
 * The returned event is refcounted, use eis_event_unref() to drop the
 * reference.
 *
 * A caller must not call eis_get_event() while holding a ref to an event
 * returned by eis_peek_event().
 */
struct eis_event *
eis_peek_event(struct eis *eis);

/**
 * Release resources associated with this event. This function always
 * returns NULL.
 *
 * The caller cannot increase the refcount of an event. Events should be
 * considered transient data and not be held longer than required.
 * eis_event_unref() is provided for consistency (as opposed to, say,
 * eis_event_free()).
 */
struct eis_event *
eis_event_unref(struct eis_event *event);

enum eis_event_type
eis_event_get_type(struct eis_event *event);

struct eis_client *
eis_event_get_client(struct eis_event *event);

struct eis_seat *
eis_event_get_seat(struct eis_event *event);

/**
 * Returns the associated @ref eis_ping struct with this event.
 *
 * For events of type other than @ref EIS_EVENT_PONG this function
 * returns NULL.
 *
 * This does not increase the refcount of the eis_pong. Use eis_pong_ref()
 * to ekeep a reference beyond the immediate scope.
 */
struct eis_ping *
eis_event_pong_get_ping(struct eis_event *event);

/**
 * For an event of type @ref EIS_EVENT_SEAT_BIND, return the capabilities
 * requested by the client.
 *
 * This is the set of *all* capabilities bound by the client as of this event,
 * not just the changed ones.
 */
bool
eis_event_seat_has_capability(struct eis_event *event, enum eis_device_capability cap);

/**
 * Return the device from this event.
 *
 * This does not increase the refcount of the device. Use eis_device_ref()
 * to keep a reference beyond the immediate scope.
 */
struct eis_device *
eis_event_get_device(struct eis_event *event);

/**
 * Return the time for the event of type @ref EIS_EVENT_FRAME in microseconds.
 *
 * @note: Only events of type @ref EIS_EVENT_FRAME carry a timestamp. For
 * convenience, the timestamp for other device events is retrofitted by this
 * library.
 *
 * @return the event time in microseconds
 */
uint64_t
eis_event_get_time(struct eis_event *event);

/**
 * Create a new eis_ping object to trigger a round trip to the client.
 * See eis_ping() for details.
 *
 * The returned @ref eis_ping is refcounted, use eis_ping_unref() to
 * drop the reference.
 *
 * @since 1.4
 */
struct eis_ping *
eis_client_new_ping(struct eis_client *client);

struct eis_client *
eis_client_ref(struct eis_client *client);

struct eis_client *
eis_client_unref(struct eis_client *client);

void *
eis_client_get_user_data(struct eis_client *eis_client);

void
eis_client_set_user_data(struct eis_client *eis_client, void *user_data);

struct eis*
eis_client_get_context(struct eis_client *client);

/**
 * Returns true if the client is a sender, false otherwise. A sender client may
 * send events to the EIS implementation, a receiver client expects to receive
 * events from the EIS implementation.
 */
bool
eis_client_is_sender(struct eis_client *client);

/**
 * Return the name set by this client. The server is under no obligation to
 * use this name.
 */
const char *
eis_client_get_name(struct eis_client *client);

/**
 * Allow connection from the client. This can only be done once, further
 * calls to this functions are ignored.
 *
 * When receiving an event of type @ref EIS_EVENT_CLIENT_CONNECT, the server
 * should connect client as soon as possible to allow communication with the
 * server. If the client is not authorized to talk to the server, call
 * eis_client_disconnect().
 */
void
eis_client_connect(struct eis_client *client);

/**
 * Disconnect this client. Once disconnected the client may no longer talk
 * to this context, all resources associated with this client should be
 * released.
 *
 * It is not necessary to call this function when an @ref
 * EIS_EVENT_CLIENT_DISCONNECT event is received.
 */
void
eis_client_disconnect(struct eis_client *client);

/**
 * Create a new logical seat with a given name. Devices available to a
 * client belong to a bound seat, or in other words: a client cannot receive
 * events from a device until it binds to a seat and receives all devices from
 * that seat.
 *
 * This seat is not immediately active, use eis_seat_add() to bind this
 * seat on the client and notify the client of it's availability.
 *
 * The returned seat is refcounted, use eis_seat_unref() to drop the
 * reference.
 */
struct eis_seat *
eis_client_new_seat(struct eis_client *client, const char *name);

/**
 * @ingroup libeis-seat
 */
struct eis_seat *
eis_seat_ref(struct eis_seat *seat);

/**
 * @ingroup libeis-seat
 */
struct eis_seat *
eis_seat_unref(struct eis_seat *seat);

/**
 * @ingroup libeis-seat
 */
struct eis_client *
eis_seat_get_client(struct eis_seat *eis_seat);

/**
 * @ingroup libeis-seat
 */
const char *
eis_seat_get_name(struct eis_seat *eis_seat);

/**
 * @ingroup libeis-seat
 */
void *
eis_seat_get_user_data(struct eis_seat *eis_seat);

/**
 * @ingroup libeis-seat
 */
void
eis_seat_set_user_data(struct eis_seat *eis_seat, void *user_data);

/**
 * @ingroup libeis-seat
 */
bool
eis_seat_has_capability(struct eis_seat *seat,
			enum eis_device_capability cap);

/**
 * @ingroup libeis-seat
 *
 * Allow a capability on the seat. This indicates to the client
 * that it may create devices with with the given capabilities, though the
 * EIS implementation may restrict the of capabilities on a device to a
 * subset of those in the seat, see eis_device_allow_capability().
 *
 * This function must be called before eis_seat_add().
 *
 * This function has no effect if called after eis_seat_add()
 */
void
eis_seat_configure_capability(struct eis_seat *seat,
			  enum eis_device_capability cap);

/**
 * @ingroup libeis-seat
 *
 * Add this seat to its client and notify the client of the seat's
 * availability. This allows the client to create a device within this seat.
 */
void
eis_seat_add(struct eis_seat *seat);

/**
 * @ingroup libeis-seat
 *
 * Remove this seat and all its remaining devices.
 */
void
eis_seat_remove(struct eis_seat *seat);

/**
 * @ingroup libeis-seat
 */
struct eis *
eis_seat_get_context(struct eis_seat *seat);

/**
 * @ingroup libeis-device
 */
struct eis *
eis_device_get_context(struct eis_device *device);

/**
 * @ingroup libeis-device
 */
struct eis_client *
eis_device_get_client(struct eis_device *device);

/**
 * @ingroup libeis-device
 */
struct eis_seat *
eis_device_get_seat(struct eis_device *device);

/**
 * @ingroup libeis-device
 */
struct eis_device *
eis_device_ref(struct eis_device *device);

/**
 * @ingroup libeis-device
 */
struct eis_device *
eis_device_unref(struct eis_device *device);

/**
 * @ingroup libeis-device
 */
void *
eis_device_get_user_data(struct eis_device *eis_device);

/**
 * @ingroup libeis-device
 */
void
eis_device_set_user_data(struct eis_device *eis_device, void *user_data);

/**
 * @ingroup libeis-device
 *
 * Return the name of the device. The return value of this function may change after
 * eis_device_configure_name(), a caller should keep a copy of it where
 * required rather than the pointer value.
 */
const char *
eis_device_get_name(struct eis_device *device);

/**
 * @ingroup libeis-device
 */
bool
eis_device_has_capability(struct eis_device *device,
			  enum eis_device_capability cap);

/**
 * @ingroup libeis-device
 *
 * Return the width in mm of a device of type @ref EIS_DEVICE_TYPE_PHYSICAL,
 * or zero otherwise.
 */
uint32_t
eis_device_get_width(struct eis_device *device);

/**
 * @ingroup libeis-device
 *
 * Return the height in mm of a device of type @ref EIS_DEVICE_TYPE_PHYSICAL,
 * or zero otherwise.
 */
uint32_t
eis_device_get_height(struct eis_device *device);

/**
 * @ingroup libeis-seat
 *
 * Create a new device on the seat. This device is not immediately active, use
 * eis_device_add() to notify the client of it's availability.
 *
 * The returned device is refcounted, use eis_device_unref() to drop the
 * reference.
 *
 * Before calling eis_device_add(), use the following functions to set up the
 * device:
 * - eis_device_configure_type()
 * - eis_device_configure_name()
 * - eis_device_configure_capability()
 * - eis_device_new_region()
 * - eis_device_new_keymap()
 *
 * The device type of the device defaults to @ref EIS_DEVICE_TYPE_VIRTUAL.
 */
struct eis_device *
eis_seat_new_device(struct eis_seat *seat);

/**
 * @ingroup libeis-device
 *
 * Set the device type for this device. It is recommended that that the device
 * type is the first call to configure the device as the device type
 * influences which other properties on the device can be set and/or will
 * trigger warnings if invoked with wrong arguments.
 */
void
eis_device_configure_type(struct eis_device *device, enum eis_device_type type);

/**
 * @ingroup libeis-device
 */
enum eis_device_type
eis_device_get_type(struct eis_device *device);

/**
 * @ingroup libeis-device
 */
void
eis_device_configure_name(struct eis_device *device, const char *name);

/**
 * @ingroup libeis-device
 */
void
eis_device_configure_capability(struct eis_device *device, enum eis_device_capability cap);

/**
 * @ingroup libeis-device
 *
 * Configure the size in mm of a device of type @ref EIS_DEVICE_TYPE_PHYSICAL.
 *
 * Device with relative-only capabilities does not require a size. A device
 * with capability @ref EIS_DEVICE_CAP_POINTER_ABSOLUTE or @ref
 * EIS_DEVICE_CAP_TOUCH must have a size.
 *
 * This function has no effect if called on a device of type other than @ref
 * EIS_DEVICE_TYPE_PHYSICAL.
 *
 * This function has no effect if called after ei_device_add()
 */
void
eis_device_configure_size(struct eis_device *device, uint32_t width, uint32_t height);

/**
 * @ingroup libeis-device
 *
 * Create a new region on the device of type @ref EIS_DEVICE_TYPE_VIRTUAL with
 * an initial refcount of 1.  Use eis_region_add() to properly add the region
 * to the device.
 *
 * A region **must** have a size to be valid, see eis_region_set_size().
 *
 * For a device of type @ref EIS_DEVICE_TYPE_PHYSICAL this function returns
 * NULL.
 */
struct eis_region *
eis_device_new_region(struct eis_device *device);

/**
 * @ingroup libeis-region
 *
 * This call has no effect if called after eis_region_add()
 */
void
eis_region_set_size(struct eis_region *region, uint32_t w, uint32_t h);

/**
 * @ingroup libeis-region
 *
 * This call has no effect if called after eis_region_add()
 */
void
eis_region_set_offset(struct eis_region *region, uint32_t x, uint32_t y);

/**
 * @ingroup libeis-region
 *
 * Set the physical scale for this region. If unset, the scale defaults to
 * 1.0.
 *
 * A @a scale value of less or equal to 0.0 will be silently ignored.
 *
 * This call has no effect if called after eis_region_add()
 *
 * See ei_region_get_physical_scale() for details.
 */
void
eis_region_set_physical_scale(struct eis_region *region, double scale);

/**
 * @ingroup libeis-region
 *
 * Attach a unique identifier representing an external resource to this region.
 * libeis does not look at or modify the value, it is passed through to the
 * client if the client supports ei_device interface version 2 or later. Because
 * the ID is assigned by the caller libei makes no guarantee that the ID is
 * indeed unique and/or corresponds to any particular format.
 *
 * This ID can be used by the caller to identify an external resource
 * that has a relationship with this region. For example, a caller may have
 * a data stream with the video data that this region represents.
 * By attaching the same identifier to the data stream and this region a client
 * (who needs to be aware of this approach) can pair the video data stream
 * with the region. Note that in this example use-case, if the stream is resized
 * there may be a transition period where two regions have the same identifier -
 * the old region and the new region with the updated size. A client must be
 * able to handle the case where two mapping_ids are identical.
 *
 * This function has no effect if called after eis_device_add()
 *
 * @since 1.1
 */
void
eis_region_set_mapping_id(struct eis_region *region, const char *mapping_id);

/**
 * @ingroup libeis-region
 *
 * Get the unique ID for this region previously set by this
 * caller, if any, or NULL if the client does not support region mapping id
 * or no region ID has been set.
 *
 * Region IDs require the client to support the ei_device interface
 * version 2 or later. This function can be used to detect support
 * for this interface: after eis_region_add() this region's ID is set
 * to NULL if the client does not support that interface version.
 *
 * In other words, if a caller sets a region ID with
 * eis_region_set_mapping_id() and that region ID is returned after eis_region_add(),
 * the client has been notified of the region ID.
 *
 * @since 1.1
 */
const char *
eis_region_get_mapping_id(struct eis_region *region);

/**
 * @ingroup libeis-region
 *
 * Add the given region to its device. Once added, the region will be sent to
 * the client when the caller calls eis_device_add() later.
 *
 * Adding the same region twice will be silently ignored.
 */
void
eis_region_add(struct eis_region *region);

/**
 * @ingroup libeis-device
 *
 * Obtain a region from the device. This function only returns regions that
 * have been added to the device with eis_region_add(). The number of regions
 * is constant for a device once eis_device_add() has been called and the
 * indices of any region remains the same for the lifetime of
 * the device.
 *
 * Regions are shared between all capabilities. Where two capabilities need
 * different region, the EIS implementation must create multiple devices with
 * individual capabilities and regions.
 *
 * This function returns the given region or NULL if the index is larger than
 * the number of regions available.
 *
 * This does not increase the refcount of the region. Use eis_region_ref() to
 * keep a reference beyond the immediate scope.
 */
struct eis_region *
eis_device_get_region(struct eis_device *device, size_t index);

/**
 * @ingroup libeis-device
 *
 * Return the region that contains the given point x/y (in desktop-wide
 * coordinates) or NULL if the coordinates are outside all regions.
 *
 * @since 1.1
 */
struct eis_region *
eis_device_get_region_at(struct eis_device *device, double x, double y);

/**
 * @ingroup libeis-region
 */
struct eis_region *
eis_region_ref(struct eis_region *region);

/**
 * @ingroup libeis-region
 */
struct eis_region *
eis_region_unref(struct eis_region *region);

/**
 * @ingroup libeis-region
 */
void *
eis_region_get_user_data(struct eis_region *region);

/**
 * @ingroup libeis-region
 */
void
eis_region_set_user_data(struct eis_region *region, void *user_data);

/**
 * @ingroup libeis-region
 */
uint32_t
eis_region_get_x(struct eis_region *region);

/**
 * @ingroup libeis-region
 */
uint32_t
eis_region_get_y(struct eis_region *region);

/**
 * @ingroup libeis-region
 */
uint32_t
eis_region_get_width(struct eis_region *region);

/**
 * @ingroup libeis-region
 */
uint32_t
eis_region_get_height(struct eis_region *region);

/**
 * @ingroup libeis-region
 */
double
eis_region_get_physical_scale(struct eis_region *region);

/**
 * @ingroup libeis-region
 *
 * @see ei_region_contains
 */
bool
eis_region_contains(struct eis_region *region, double x, double y);

/**
 * @ingroup libeis-device
 *
 * Add this device to its seat and notify the client of the device's
 * availability.
 *
 * The device is paused, use eis_device_resume() to enable events from
 * the client.
 */
void
eis_device_add(struct eis_device *device);

/**
 * @ingroup libeis-device
 *
 * Remove the device.
 * This does not release any resources associated with this device, use
 * eis_device_unref() for any references held by the caller.
 */
void
eis_device_remove(struct eis_device *device);

/**
 * @ingroup libeis-device
 *
 * Notify the client that the device is paused and that no events
 * from the client will be processed.
 *
 * The library filters events sent by the client **after** the pause
 * notification has been processed by the client but this does not affect
 * events already in transit. In other words, the server may still receive
 * a number of events from a device after it has been paused and must
 * update its internal state accordingly.
 *
 * Pause/resume should only be used for short-term event delaying, a client
 * will expect that the device's state has not changed between pause and
 * resume. Where a device's state changes on the EIS implementation side (e.g.
 * buttons or keys are forcibly released), the device should be removed and
 * re-added as new device.
 *
 * @param device A connected device
 */
void
eis_device_pause(struct eis_device *device);

/**
 * @ingroup libeis-device
 *
 * Notify the client that the capabilities are resumed and that events
 * from the device will be processed.
 *
 * @param device A connected device
 */
void
eis_device_resume(struct eis_device *device);

/**
 * @ingroup libeis-device
 *
 * Create a new keymap of the given @a type. This keymap does not immediately
 * apply to the device, use eis_keymap_add() to apply this keymap. A keymap
 * may only be applied once and to a single device.
 *
 * The returned keymap has a refcount of at least 1, use eis_keymap_unref()
 * to release resources associated with this keymap.
 *
 * @param device The device with a @ref EIS_DEVICE_CAP_KEYBOARD capability
 * @param type The type of the keymap.
 * @param fd A memmap-able file descriptor of size @a size pointing to the
 * keymap used by this device. @a fd  can be closed by the caller after this
 * function completes. The file descriptor needs to be mmap:ed with MAP_PRIVATE.
 * @param size The size of the data at @a fd in bytes
 *
 * @return A keymap object or `NULL` on failure.
 */
struct eis_keymap *
eis_device_new_keymap(struct eis_device *device,
		      enum eis_keymap_type type, int fd, size_t size);

/**
 * @ingroup libeis-keymap
 *
 * Set the keymap on the device.
 *
 * The keymap is constant for the lifetime of the device and assigned to
 * this device individually. Where the keymap has to change, remove the
 * device and create a new one.
 *
 * If a keymap is `NULL`, the device does not have an individual keymap
 * assigned. Note that this may mean the client needs to guess at the
 * keymap layout.
 *
 * This function has no effect if called after eis_device_add()
 */
void
eis_keymap_add(struct eis_keymap *keymap);

/**
 * @ingroup libeis-keymap
 * @return the size of the keymap in bytes
 */
size_t
eis_keymap_get_size(struct eis_keymap *keymap);

/**
 * @ingroup libeis-keymap
 *
 * Returns the type for this keymap. The type specifies how to interpret the
 * data at the file descriptor returned by eis_keymap_get_fd().
 */
enum eis_keymap_type
eis_keymap_get_type(struct eis_keymap *keymap);

/**
 * @ingroup libeis-keymap
 *
 * Return a memmap-able file descriptor pointing to the keymap used by the
 * device. The keymap is constant for the lifetime of the device and
 * assigned to this device individually.
 */
int
eis_keymap_get_fd(struct eis_keymap *keymap);

/**
 * @ingroup libeis-keymap
 */
struct eis_keymap *
eis_keymap_ref(struct eis_keymap *keymap);

/**
 * @ingroup libeis-keymap
 */
struct eis_keymap *
eis_keymap_unref(struct eis_keymap *keymap);

/**
 * @ingroup libeis-keymap
 */
void *
eis_keymap_get_user_data(struct eis_keymap *eis_keymap);

/**
 * @ingroup libeis-keymap
 */
void
eis_keymap_set_user_data(struct eis_keymap *eis_keymap, void *user_data);

/**
 * @ingroup libeis-keymap
 *
 * Return the device this keymap belongs to.
 */
struct eis_device *
eis_keymap_get_device(struct eis_keymap *keymap);

/**
 * @ingroup libeis-device
 *
 * Return the keymap assigned to this device. The return value of this
 * function is the keymap (if any) after the call to
 * eis_keymap_add().
 */
struct eis_keymap *
eis_device_keyboard_get_keymap(struct eis_device *device);

/**
 * @ingroup libeis-device
 *
 * Notify the client of the current XKB modifier state.
 *
 * This should be called every time the modifier state or current
 * effective group changes.
 *
 * When the state changes due to an incoming
 * EIS_EVENT_KEYBOARD_KEY (for sender clients), this method should be
 * called when the corresponding EIS_EVENT_FRAME is processed, before
 * processing any subsequent events.
 *
 * When the state changes due to a key press with a receiver client, this
 * method should be called immediately after the corresponding call to
 * eis_device_frame(). If the change impacts multiple keyboards, this
 * method should be called for all of them.
 *
 * For changes caused by other factors, this method should be called for
 * all affected keyboards at the point the change occurs.
 */
void
eis_device_keyboard_send_xkb_modifiers(struct eis_device *device,
				       uint32_t depressed,
				       uint32_t latched,
				       uint32_t locked,
				       uint32_t group);

/**
 * @ingroup libeis-receiver
 *
 * see @ref ei_device_start_emulating
 */
void
eis_device_start_emulating(struct eis_device *device, uint32_t sequence);

/**
 * @ingroup libeis-receiver
 *
 * see @ref ei_device_stop_emulating
 */
void
eis_device_stop_emulating(struct eis_device *device);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_frame
 */
void
eis_device_frame(struct eis_device *device, uint64_t time);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_pointer_motion
 */
void
eis_device_pointer_motion(struct eis_device *device, double x, double y);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_pointer_motion_absolute
 */
void
eis_device_pointer_motion_absolute(struct eis_device *device,
				   double x, double y);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_button_button
 */
void
eis_device_button_button(struct eis_device *device,
			 uint32_t button, bool is_press);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_scroll_delta
 */
void
eis_device_scroll_delta(struct eis_device *device, double x, double y);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_scroll_discrete
 */
void
eis_device_scroll_discrete(struct eis_device *device, int32_t x, int32_t y);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_scroll_stop
 */
void
eis_device_scroll_stop(struct eis_device *device, bool stop_x, bool stop_y);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_scroll_cancel
 */
void
eis_device_scroll_cancel(struct eis_device *device, bool cancel_x, bool cancel_y);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_keyboard_key
 */
void
eis_device_keyboard_key(struct eis_device *device, uint32_t keycode, bool is_press);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_device_touch_new
 */
struct eis_touch *
eis_device_touch_new(struct eis_device *device);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_touch_down
 */
void
eis_touch_down(struct eis_touch *touch, double x, double y);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_touch_motion
 */
void
eis_touch_motion(struct eis_touch *touch, double x, double y);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_touch_up
 */
void
eis_touch_up(struct eis_touch *touch);

/**
 * @ingroup libeis-receiver
 *
 * see @ref ei_touch_cancel
 */
void
eis_touch_cancel(struct eis_touch *touch);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_touch_ref
 */
struct eis_touch *
eis_touch_ref(struct eis_touch *touch);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_touch_unref
 */
struct eis_touch *
eis_touch_unref(struct eis_touch *touch);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_touch_set_user_data
 */
void
eis_touch_set_user_data(struct eis_touch *touch, void *user_data);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_touch_get_user_data
 */
void *
eis_touch_get_user_data(struct eis_touch *touch);

/**
 * @ingroup libeis-receiver
 *
 ** see @ref ei_touch_get_device
 */
struct eis_device *
eis_touch_get_device(struct eis_touch *touch);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_DEVICE_START_EMULATING, return the
 * sequence number set by the ei client implementation.
 *
 * See ei_device_start_emulating() for details.
 */
uint32_t
eis_event_emulating_get_sequence(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_POINTER_MOTION return the relative x
 * movement in logical pixels or mm, depending on the device type.
 */
double
eis_event_pointer_get_dx(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_POINTER_MOTION return the relative y
 * movement in logical pixels or mm, depending on the device type.
 */
double
eis_event_pointer_get_dy(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_POINTER_MOTION_ABSOLUTE return the x
 * position in logical pixels or mm, depending on the device type.
 */
double
eis_event_pointer_get_absolute_x(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_POINTER_MOTION_ABSOLUTE return the y
 * position in logical pixels or mm, depending on the device type.
 */
double
eis_event_pointer_get_absolute_y(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_BUTTON_BUTTON return the button
 * code as defined in linux/input-event-codes.h
 */
uint32_t
eis_event_button_get_button(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_BUTTON_BUTTON return true if the
 * event is a button press, false for a release.
 */
bool
eis_event_button_get_is_press(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_SCROLL_DELTA return the x scroll
 * distance in logical pixels or mm, depending on the device type.
 */
double
eis_event_scroll_get_dx(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_SCROLL_DELTA return the y scroll
 * distance in logical pixels or mm, depending on the device type.
 */
double
eis_event_scroll_get_dy(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_SCROLL_STOP return whether the
 * x axis has stopped scrolling.
 *
 * For an event of type @ref EIS_EVENT_SCROLL_CANCEL return whether the
 * x axis has cancelled scrolling.
 */
bool
eis_event_scroll_get_stop_x(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_SCROLL_STOP return whether the
 * y axis has stopped scrolling.
 *
 * For an event of type @ref EIS_EVENT_SCROLL_CANCEL return whether the
 * y axis has cancelled scrolling.
 */
bool
eis_event_scroll_get_stop_y(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_SCROLL_DISCRETE return the x
 * scroll distance in fractions or multiples of 120.
 */
int32_t
eis_event_scroll_get_discrete_dx(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_SCROLL_DISCRETE return the y
 * scroll distance in fractions or multiples of 120.
 */
int32_t
eis_event_scroll_get_discrete_dy(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_KEYBOARD_KEY return the key code (as
 * defined in include/linux/input-event-codes.h).
 */
uint32_t
eis_event_keyboard_get_key(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_KEYBOARD_KEY return true if the
 * event is a key down, false for a release.
 */
bool
eis_event_keyboard_get_key_is_press(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_TOUCH_DOWN, @ref
 * EIS_EVENT_TOUCH_MOTION, or @ref EIS_EVENT_TOUCH_UP, return the tracking
 * ID of the touch.
 *
 * The tracking ID is a unique identifier for a touch and is valid from
 * touch down through to touch up but may be re-used in the future.
 * The tracking ID is randomly assigned to a touch, a client
 * must not expect any specific value.
 */
uint32_t
eis_event_touch_get_id(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_TOUCH_DOWN, or @ref
 * EIS_EVENT_TOUCH_MOTION, return the x coordinate of the touch
 * in logical pixels or mm, depending on the device type.
 */
double
eis_event_touch_get_x(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_TOUCH_DOWN, or @ref
 * EIS_EVENT_TOUCH_MOTION, return the y coordinate of the touch
 * in logical pixels or mm, depending on the device type.
 */
double
eis_event_touch_get_y(struct eis_event *event);

/**
 * @ingroup libeis-sender
 *
 * For an event of type @ref EIS_EVENT_TOUCH_UP
 * return true if the touch was cancelled instead
 * of logically released.
 *
 * Support for touch cancellation requires the EIS implementation and client to
 * support version 2 or later of the ei_touchscreen protocol interface.
 */
bool
eis_event_touch_get_is_cancel(struct eis_event *event);

/**
 * @returns a timestamp for the current time to pass into
 * eis_device_frame().
 *
 * By default, the returned timestamp is CLOCK_MONOTONIC for compatibility with
 * evdev and libinput. This can be overridden with eis_clock_set_now_func().
 */
uint64_t
eis_now(struct eis *eis);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
