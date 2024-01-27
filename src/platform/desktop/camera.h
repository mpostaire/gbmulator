#pragma once

#include <gst/gst.h>

#include "../../core/gb.h"

byte_t gb_camera_capture_image_cb(byte_t *pixels);

void camera_play(void);

void camera_pause(void);

gboolean camera_find_devices(void);

GtkStringList *camera_get_devices_names(gsize *len);

gchar ***camera_get_devices_paths(gsize *len);

/**
 * gst_init must have been called before
 */
gboolean camera_init(gchar *device_path);

void camera_quit(void);
