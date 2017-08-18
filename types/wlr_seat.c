#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

static void resource_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void wl_pointer_set_cursor(struct wl_client *client,
	   struct wl_resource *resource,
	   uint32_t serial,
	   struct wl_resource *surface,
	   int32_t hotspot_x,
	   int32_t hotspot_y) {
	wlr_log(L_DEBUG, "TODO: wl_pointer_set_cursor");
}

static const struct wl_pointer_interface wl_pointer_impl = {
	.set_cursor = wl_pointer_set_cursor,
	.release = resource_destroy
};

static void wl_pointer_destroy(struct wl_resource *resource) {
	struct wlr_seat_handle *handle = wl_resource_get_user_data(resource);
	if (handle->pointer) {
		handle->pointer = NULL;
	}
}

static void wl_seat_get_pointer(struct wl_client *client,
		struct wl_resource *_handle, uint32_t id) {
	struct wlr_seat_handle *handle = wl_resource_get_user_data(_handle);
	if (!(handle->wlr_seat->capabilities & WL_SEAT_CAPABILITY_POINTER)) {
		return;
	}
	if (handle->pointer) {
		// TODO: this is probably a protocol violation but it simplifies our
		// code and it'd be stupid for clients to create several pointers for
		// the same seat
		wl_resource_destroy(handle->pointer);
	}
	handle->pointer = wl_resource_create(client, &wl_pointer_interface,
		wl_resource_get_version(_handle), id);
	wl_resource_set_implementation(handle->pointer, &wl_pointer_impl,
		handle, &wl_pointer_destroy);
}

static const struct wl_keyboard_interface wl_keyboard_impl = {
	.release = resource_destroy
};

static void wl_keyboard_destroy(struct wl_resource *resource) {
	struct wlr_seat_handle *handle = wl_resource_get_user_data(resource);
	if (handle->keyboard) {
		handle->keyboard = NULL;
	}
}

static void wl_seat_get_keyboard(struct wl_client *client,
		struct wl_resource *_handle, uint32_t id) {
	struct wlr_seat_handle *handle = wl_resource_get_user_data(_handle);
	if (!(handle->wlr_seat->capabilities & WL_SEAT_CAPABILITY_KEYBOARD)) {
		return;
	}
	if (handle->keyboard) {
		// TODO: this is probably a protocol violation but it simplifies our
		// code and it'd be stupid for clients to create several pointers for
		// the same seat
		wl_resource_destroy(handle->keyboard);
	}
	handle->keyboard = wl_resource_create(client, &wl_keyboard_interface,
		wl_resource_get_version(_handle), id);
	wl_resource_set_implementation(handle->keyboard, &wl_keyboard_impl,
		handle, &wl_keyboard_destroy);
	wl_signal_emit(&handle->wlr_seat->events.keyboard_bound, handle);
}

static const struct wl_touch_interface wl_touch_impl = {
	.release = resource_destroy
};

static void wl_touch_destroy(struct wl_resource *resource) {
	struct wlr_seat_handle *handle = wl_resource_get_user_data(resource);
	if (handle->touch) {
		handle->touch = NULL;
	}
}

static void wl_seat_get_touch(struct wl_client *client,
		struct wl_resource *_handle, uint32_t id) {
	struct wlr_seat_handle *handle = wl_resource_get_user_data(_handle);
	if (!(handle->wlr_seat->capabilities & WL_SEAT_CAPABILITY_TOUCH)) {
		return;
	}
	if (handle->touch) {
		// TODO: this is probably a protocol violation but it simplifies our
		// code and it'd be stupid for clients to create several pointers for
		// the same seat
		wl_resource_destroy(handle->touch);
	}
	handle->touch = wl_resource_create(client, &wl_touch_interface,
		wl_resource_get_version(_handle), id);
	wl_resource_set_implementation(handle->touch, &wl_touch_impl,
		handle, &wl_touch_destroy);
}

static void wl_seat_destroy(struct wl_resource *resource) {
	struct wlr_seat_handle *handle = wl_resource_get_user_data(resource);
	if (handle->pointer) {
		wl_resource_destroy(handle->pointer);
	}
	if (handle->keyboard) {
		wl_resource_destroy(handle->keyboard);
	}
	if (handle->touch) {
		wl_resource_destroy(handle->touch);
	}
	wl_signal_emit(&handle->wlr_seat->events.client_unbound, handle);
	wl_list_remove(&handle->link);
	free(handle);
}

struct wl_seat_interface wl_seat_impl = {
	.get_pointer = wl_seat_get_pointer,
	.get_keyboard = wl_seat_get_keyboard,
	.get_touch = wl_seat_get_touch,
	.release = resource_destroy
};

static void wl_seat_bind(struct wl_client *wl_client, void *_wlr_seat,
		uint32_t version, uint32_t id) {
	struct wlr_seat *wlr_seat = _wlr_seat;
	assert(wl_client && wlr_seat);
	if (version > 6) {
		wlr_log(L_ERROR, "Client requested unsupported wl_seat version, disconnecting");
		wl_client_destroy(wl_client);
		return;
	}
	struct wlr_seat_handle *handle = calloc(1, sizeof(struct wlr_seat_handle));
	handle->wl_resource = wl_resource_create(
			wl_client, &wl_seat_interface, version, id);
	handle->wlr_seat = wlr_seat;
	wl_resource_set_implementation(handle->wl_resource, &wl_seat_impl,
		handle, wl_seat_destroy);
	wl_list_insert(&wlr_seat->handles, &handle->link);
	wl_seat_send_capabilities(handle->wl_resource, wlr_seat->capabilities);
	wl_signal_emit(&wlr_seat->events.client_bound, handle);
}

struct wlr_seat *wlr_seat_create(struct wl_display *display, const char *name) {
	struct wlr_seat *wlr_seat = calloc(1, sizeof(struct wlr_seat));
	if (!wlr_seat) {
		return NULL;
	}
	struct wl_global *wl_global = wl_global_create(display,
		&wl_seat_interface, 6, wlr_seat, wl_seat_bind);
	if (!wl_global) {
		free(wlr_seat);
		return NULL;
	}
	wlr_seat->wl_global = wl_global;
	wlr_seat->name = strdup(name);
	wl_list_init(&wlr_seat->handles);
	wl_signal_init(&wlr_seat->events.client_bound);
	wl_signal_init(&wlr_seat->events.client_unbound);
	wl_signal_init(&wlr_seat->events.keyboard_bound);
	return wlr_seat;
}

void wlr_seat_destroy(struct wlr_seat *wlr_seat) {
	if (!wlr_seat) {
		return;
	}

	wl_global_destroy(wlr_seat->wl_global);
	free(wlr_seat->name);
	free(wlr_seat);
}

struct wlr_seat_handle *wlr_seat_handle_for_client(struct wlr_seat *wlr_seat,
		struct wl_client *client) {
	assert(wlr_seat);
	struct wlr_seat_handle *handle;
	wl_list_for_each(handle, &wlr_seat->handles, link) {
		if (wl_resource_get_client(handle->wl_resource) == client) {
			return handle;
		}
	}
	return NULL;
}

void wlr_seat_set_capabilities(struct wlr_seat *wlr_seat,
		uint32_t capabilities) {
	wlr_seat->capabilities = capabilities;
	struct wlr_seat_handle *handle;
	wl_list_for_each(handle, &wlr_seat->handles, link) {
		wl_seat_send_capabilities(handle->wl_resource, capabilities);
	}
}

void wlr_seat_set_name(struct wlr_seat *wlr_seat, const char *name) {
	free(wlr_seat->name);
	wlr_seat->name = strdup(name);
	struct wlr_seat_handle *handle;
	wl_list_for_each(handle, &wlr_seat->handles, link) {
		wl_seat_send_name(handle->wl_resource, name);
	}
}