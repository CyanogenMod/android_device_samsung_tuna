LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := dumpdcc
LOCAL_SRC_FILES := dumpdcc.c
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Wall -Werror

include $(BUILD_EXECUTABLE)
