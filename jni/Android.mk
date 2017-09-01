LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := andhook
#LOCAL_CFLAGS	:= -fvisibility=hidden
LOCAL_SRC_FILES := hello-jni.c
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := andhookxx
#LOCAL_CFLAGS	:= -fvisibility=hidden
LOCAL_CFLAGS   := -D_BUILD_MAIN -Werror,-Wformat-security
LOCAL_SRC_FILES := hello-jni.c
include $(BUILD_EXECUTABLE)
