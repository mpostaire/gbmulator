#include <gst/gst.h>

#include "../../core/gb.h"
#include "../common/utils.h"

static byte_t *camera_data = NULL;

static GstFlowReturn gstreamer_new_sample_cb(GstElement *sink, gpointer userdata) {
    GstSample *sample;

    /* Retrieve the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (!sample)
        return GST_FLOW_ERROR;

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map_info;
    if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ))
        return GST_FLOW_ERROR;

    if (!camera_data)
        camera_data = xmalloc(map_info.size);
    memcpy(camera_data, map_info.data, map_info.size);

    gst_buffer_unmap(buffer, &map_info);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
}

static void gstreamer_error_cb(GstBus *bus, GstMessage *msg, gpointer userdata) {
    GError *err;
    gchar *debug_info;

    gst_message_parse_error(msg, &err, &debug_info);
    eprintf("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    eprintf("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);
}

byte_t gb_camera_capture_image_cb(byte_t *pixels) {
    // camera_data is YUV 4:2:0 format so to get a grayscale image we just take
    // the Y channel: the first GB_CAMERA_SENSOR_WIDTH * GB_CAMERA_SENSOR_HEIGHT bytes
    memcpy(pixels, camera_data, GB_CAMERA_SENSOR_WIDTH * GB_CAMERA_SENSOR_HEIGHT);
    return 1;
}

gboolean camera_init(void) {
    GstElement *pipeline = gst_pipeline_new("webcam-pipeline");
    GstElement *src = gst_element_factory_make("v4l2src", "video-source");
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "video-converter");
    GstElement *aspectratiocrop = gst_element_factory_make("aspectratiocrop", "aspect-ratio-crop");
    GstElement *videoscale = gst_element_factory_make("videoscale", "video-scaler");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "caps-filter");
    GstElement *sink = gst_element_factory_make("appsink", "app-sink");

    if (!pipeline || !src || !videoconvert || !videoscale || !capsfilter || !sink) {
        eprintf("Failed creating gstreamer pipeline\n");
        return FALSE;
    }

    g_object_set(src, "device", "/dev/video0", NULL); // TODO select multiple devices

    gst_bin_add_many(GST_BIN(pipeline), src, videoconvert, aspectratiocrop, videoscale, capsfilter, sink, NULL);
    if (!gst_element_link_many(src, videoconvert, aspectratiocrop, videoscale, capsfilter, sink, NULL)) {
        eprintf("Failed linking elements to the gstreamer source\n");
        return FALSE;
    }

    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "I420", // I420 == YUV 4:2:0
                                        "width", G_TYPE_INT, GB_CAMERA_SENSOR_WIDTH,
                                        "height", G_TYPE_INT, GB_CAMERA_SENSOR_HEIGHT,
                                        NULL);
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    g_object_set(aspectratiocrop, "aspect-ratio", 1, 1, NULL);

    g_object_set(sink, "emit-signals", TRUE, NULL);
    g_signal_connect(sink, "new-sample", G_CALLBACK(gstreamer_new_sample_cb), NULL);

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback) gstreamer_error_cb, NULL);
    gst_object_unref(bus);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    return TRUE;
}

// TODO camera_quit
// TODO make this whole camera module optional and only added if compiled with gstreamer
// ---> use a variable disable switch for the makefile (false if gstreamer is installed, true if not BUT user can force it via makefile command line)
// --> do the same for zlib (for now its not forceable by the user)
void camera_quit(void) {
    if (camera_data) {
        free(camera_data);
        camera_data = NULL;
    }
}
