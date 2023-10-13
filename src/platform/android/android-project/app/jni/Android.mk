$(shell sh jni/get_sdl.sh jni release-2.28.4)

include $(call all-subdir-makefiles)
