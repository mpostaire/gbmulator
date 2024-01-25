#pragma once

#include <gst/gst.h>

#include "../../core/gb.h"

byte_t gb_camera_capture_image_cb(byte_t *pixels);

gboolean camera_init(void);

void camera_quit(void);
