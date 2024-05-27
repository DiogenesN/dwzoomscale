/* dwzoomscale */
/* YOU NEED YDOTOOL */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "create-shm.h"
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

/* Global variables */
static int fd = 0;
static off_t size = 0;

/* Dummy funtion */
static void noop() {}

enum pointer_event_mask {
	POINTER_EVENT_ENTER = 1 << 0,
	POINTER_EVENT_LEAVE = 1 << 1,
};

struct pointer_event {
	uint32_t event_mask;
	wl_fixed_t surface_x;
	wl_fixed_t surface_y;
	uint32_t serial;
};

/* Wayland code */
struct client_state {
	/* Globals */
	struct wl_shm *wl_shm;
	struct wl_display *wl_display;
	struct wl_registry *wl_registry;
	struct wl_compositor *wl_compositor;
	struct wl_seat *wl_seat;
	struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1;
	struct zwp_virtual_keyboard_manager_v1 *zwp_virtual_keyboard_manager_v1;
	/* Objects */
	struct wl_surface *wl_surface;
	struct wl_pointer *wl_pointer;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct zwlr_layer_shell_v1 *layer_shell;
	/* State */
	uint32_t last_frame;
	int width;
	int height;
	bool closed;
	struct pointer_event pointer_event;
};

static const struct zwlr_layer_surface_v1_listener layer_surface_listener;
static void zwlr_layer_surface_close(void *data, struct zwlr_layer_surface_v1 *surface);

/* get pointer events */
static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
						struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	(void)surface;
	(void)wl_pointer;
	struct client_state *state = data;
	state->pointer_event.event_mask |= POINTER_EVENT_ENTER;
	state->pointer_event.serial = serial;
	state->pointer_event.surface_x = surface_x,
	state->pointer_event.surface_y = surface_y;
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
																struct wl_surface *surface) {
	(void)surface;
	(void)wl_pointer;
	struct client_state *client_state = data;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
}

static void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
	(void)wl_pointer;
	struct client_state *state = data;
	struct pointer_event *event = &state->pointer_event;
	if (event->event_mask & POINTER_EVENT_ENTER) {
		// Enter surface
		const char *filename = "/tmp/.dwzoomscale_keymap";
		// Check if the file exists
		if (access(filename, F_OK) != -1) {
			// File exists do nothing
		}
		else {
			fprintf(stderr, "Keymap filr does not exist, created kaymap\n");
			// File does not exist create keymap
			/////////////////////////////////// Generating keymap //////////////////////////////////
			struct xkb_context *context;
			struct xkb_keymap *keymap;
			char *keymap_string;

			// Create an XKB context
			context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
			if (!context) {
				fprintf(stderr, "Failed to create XKB context\n");
				return;
			}

			// Define a keymap (replace "us" with your desired layout)
			keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
			if (!keymap) {
				fprintf(stderr, "Failed to create XKB keymap\n");
				xkb_context_unref(context);
				return;
			}

			// Get the keymap as a string
			keymap_string = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
			if (!keymap_string) {
				fprintf(stderr, "Failed to get keymap string\n");
				xkb_keymap_unref(keymap);
				xkb_context_unref(context);
				return;
			}

			// Save the keymap string to a file
			FILE *file = fopen("/tmp/.dwzoomscale_keymap", "w");
			if (!file) {
				fprintf(stderr, "Failed to open file for writing\n");
				free(keymap_string);
				xkb_keymap_unref(keymap);
				xkb_context_unref(context);
				return;
			}

			fputs(keymap_string, file);
			fclose(file);

			// Clean up resources
			free(keymap_string);
			xkb_keymap_unref(keymap);
			xkb_context_unref(context);
		}

		fd = open("/tmp/.dwzoomscale_keymap", O_RDONLY);
		size = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		zwp_virtual_keyboard_v1_keymap(state->zwp_virtual_keyboard_v1, XKB_KEYMAP_FORMAT_TEXT_V1,
																						fd,	size);
		zwp_virtual_keyboard_v1_key(state->zwp_virtual_keyboard_v1, 0, 125,
													WL_KEYBOARD_KEY_STATE_RELEASED);
	}

	if (event->event_mask & POINTER_EVENT_LEAVE) {
		// Leave surface
		zwp_virtual_keyboard_v1_keymap(state->zwp_virtual_keyboard_v1, XKB_KEYMAP_FORMAT_TEXT_V1,
																						fd,	size);
		zwp_virtual_keyboard_v1_key(state->zwp_virtual_keyboard_v1, 0, 125,
																	WL_KEYBOARD_KEY_STATE_PRESSED);
		close(fd);
	}
	memset(event, 0, sizeof(*event));
}

static const struct wl_pointer_listener wl_pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = noop,
	.button = noop,
	.axis = noop,
	.frame = wl_pointer_frame,
	.axis_source = noop,
	.axis_stop = noop,
	.axis_discrete = noop,
};

/* Button BUffer */
static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	(void)data;
	/* Sent by the compositor when it's no longer using this buffer */
	wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release,
};

static struct wl_buffer *draw_frame(struct client_state *state) {
	const int width = state->width;
	const int height = state->height;
	int stride = width * 4;
	int size = stride * height;

	int fd = allocate_shm_file(size);

	if (fd == -1) {
		return NULL;
	}

	uint32_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if (data == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
																		WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	munmap(data, size);
	wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
	return buffer;
}

static void wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
	(void)data;
	(void)wl_seat;
	struct client_state *state = data;

	bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;

	if (have_pointer && state->wl_pointer == NULL) {
		state->wl_pointer = wl_seat_get_pointer(state->wl_seat);
		wl_pointer_add_listener(state->wl_pointer, &wl_pointer_listener, state);
	}
	else if (!have_pointer && state->wl_pointer != NULL) {
		wl_pointer_release(state->wl_pointer);
		state->wl_pointer = NULL;
	}
}

static const struct wl_seat_listener wl_seat_listener = {
	.capabilities = wl_seat_capabilities,
	.name = noop,
};

static void registry_global(void *data, struct wl_registry *wl_registry, uint32_t name,
													const char *interface, uint32_t version) {
	(void)version;
	struct client_state *state = data;

	if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
	}
	else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->wl_compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, 4);
	}
	else if (strcmp(interface, wl_seat_interface.name) == 0) {
		state->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 7);
		wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
	}
	else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
		state->zwp_virtual_keyboard_manager_v1 = wl_registry_bind(wl_registry, name,
												&zwp_virtual_keyboard_manager_v1_interface, 1);
	}
	else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->layer_shell = wl_registry_bind(wl_registry, name, &zwlr_layer_shell_v1_interface, 1);
	}
}

static void registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
	(void)data;
	(void)name;
	(void)wl_registry;
	/* This space deliberately left blank */
}

static const struct wl_registry_listener wl_registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/// configure zwlr_layer_surface_v1
static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface,
											uint32_t serial, uint32_t width, uint32_t height) {
	(void)width;
	(void)height;
	struct client_state *state = data;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	struct wl_buffer *buffer = draw_frame(state);
	wl_surface_attach(state->wl_surface, buffer, 0, 0);
	wl_surface_commit(state->wl_surface);
}

static void zwlr_layer_surface_close(void *data, struct zwlr_layer_surface_v1 *surface) {
	(void)surface;
	struct client_state *state = data;
	state->closed = true;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = zwlr_layer_surface_close,
};

int main(void) {
	struct client_state state = { 0 };
	state.width = 3;
	state.height = 3;

	state.wl_display = wl_display_connect(NULL);
	state.wl_registry = wl_display_get_registry(state.wl_display);
	wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
	wl_display_roundtrip(state.wl_display);
	state.zwp_virtual_keyboard_v1 = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
										state.zwp_virtual_keyboard_manager_v1, state.wl_seat);
	state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
	state.layer_surface = zwlr_layer_shell_v1_get_layer_surface(state.layer_shell,
																state.wl_surface,
																NULL,
																ZWLR_LAYER_SHELL_V1_LAYER_TOP,
																"");
	zwlr_layer_surface_v1_add_listener(state.layer_surface, &layer_surface_listener, &state);
	zwlr_layer_surface_v1_set_size(state.layer_surface, state.width, state.height);
	zwlr_layer_surface_v1_set_anchor(state.layer_surface, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | \
																ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    wl_surface_commit(state.wl_surface);

	while (!state.closed && wl_display_dispatch(state.wl_display)) {
		/* This space deliberately left blank */
	}
	if (state.zwp_virtual_keyboard_v1) {
		zwp_virtual_keyboard_v1_destroy(state.zwp_virtual_keyboard_v1);
	}
	if (state.zwp_virtual_keyboard_manager_v1) {
		zwp_virtual_keyboard_manager_v1_destroy(state.zwp_virtual_keyboard_manager_v1);
	}
	if (state.layer_surface) {
		zwlr_layer_surface_v1_destroy(state.layer_surface);
	}
	if (state.layer_shell) {
		zwlr_layer_shell_v1_destroy(state.layer_shell);
	}
	if (state.wl_surface) {
		wl_surface_destroy(state.wl_surface);
	}
	if (state.wl_seat) {
		wl_seat_destroy(state.wl_seat);
	}
	if (state.wl_registry) {
		wl_registry_destroy(state.wl_registry);
	}
	if (state.wl_display) {
		wl_display_disconnect(state.wl_display);
	}
	printf("Wayland client terminated!\n");

    return 0;
}
