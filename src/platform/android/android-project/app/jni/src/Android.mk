LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL2

SRC_PATH := ../../../../../../../src

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
					$(LOCAL_PATH)/$(SRC_PATH)

# Add your application source files here...
LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH)/$(SRC_PATH)/core/*.c) \
				   $(wildcard $(LOCAL_PATH)/$(SRC_PATH)/platform/common/*.c) \
				   $(wildcard $(LOCAL_PATH)/$(SRC_PATH)/platform/android/*.c)

LOCAL_CFLAGS := -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-bad-function-cast -O3

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid -lz

include $(BUILD_SHARED_LIBRARY)
