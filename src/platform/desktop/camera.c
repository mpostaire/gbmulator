#include <gst/gst.h>
#include <adwaita.h>

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
    if (camera_data) {
        memcpy(pixels, camera_data, GB_CAMERA_SENSOR_WIDTH * GB_CAMERA_SENSOR_HEIGHT);
        return 1;
    } else {
        return 0;
    }
}

static GstElement *pipeline;

static gsize devices_len;
static gchar **devices_paths;
static GtkStringList *devices_names;

gboolean camera_find_devices(void) {
    // TODO return list of name and path (elem[i] == path_i, elem[i + 1] == name_i)
    // implem the gui first to see chat kind of data structure to feed into the gui to generate it from this function
    // and avoir unecessary conversions from string list to other
    devices_names = gtk_string_list_new(NULL);

    GstDeviceMonitor *monitor = gst_device_monitor_new();
    if (!gst_device_monitor_add_filter(monitor, "Video/Source", NULL))
        eprintf("couldn't create filter, trying anyway\n");
    if (!gst_device_monitor_start(monitor)) {
        eprintf("camera monitor couldn't start\n");
        return FALSE;
    }

    devices_len = 0;

    GList *devices = gst_device_monitor_get_devices(monitor);
    for (GList *iter = devices; iter != NULL; iter = g_list_next(iter)) {
        GstDevice *device = GST_DEVICE(iter->data);
        GstStructure *props = gst_device_get_properties(device);
        if (g_strcmp0("v4l2", gst_structure_get_string(props, "device.api")))
            continue;

        const gchar *path = gst_structure_get_string(props, "object.path");
        path = strchr(path, '/');
        const gchar *name = gst_structure_get_string(props, "node.description");

        devices_len++;
        devices_paths = xrealloc(devices_paths, sizeof(*devices_paths) * devices_len);
        size_t path_len = strnlen(path, 256) + 1;
        devices_paths[devices_len - 1] = xmalloc(path_len);
        strncpy(devices_paths[devices_len - 1], path, path_len);

        gtk_string_list_append(GTK_STRING_LIST(devices_names), name);
        gst_structure_free(props);
    }

    g_list_free(devices);

    gst_device_monitor_stop(monitor);

    return TRUE;
}

void camera_play(void) {
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void camera_pause(void) {
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
}

GtkStringList *camera_get_devices_names(gsize *len) {
    *len = devices_len;
    return devices_names;
}

gchar ***camera_get_devices_paths(gsize *len) {
    *len = devices_len;
    return &devices_paths;
}

static guint new_sample_src_id;
static guint bus_error_src_id;
gboolean camera_init(gchar *device_path) {
    pipeline = gst_pipeline_new("webcam-pipeline");
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

    g_object_set(src, "device", device_path, NULL);

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
    new_sample_src_id = g_signal_connect(sink, "new-sample", G_CALLBACK(gstreamer_new_sample_cb), NULL);

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_signal_watch(bus);
    bus_error_src_id = g_signal_connect(G_OBJECT(bus), "message::error", (GCallback) gstreamer_error_cb, NULL);
    gst_object_unref(bus);

    return TRUE;
}

void camera_quit(void) {
    if (camera_data) {
        free(camera_data);
        camera_data = NULL;
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(pipeline));
    g_source_remove(new_sample_src_id);
    g_source_remove(bus_error_src_id);
}
