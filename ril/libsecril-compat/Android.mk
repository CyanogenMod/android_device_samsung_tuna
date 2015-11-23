LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	md5.c \
	secril-compat.c

LOCAL_SHARED_LIBRARIES := \
	libhardware_legacy \
	libbinder \
	liblog

LOCAL_CFLAGS := -Wall -Werror

LOCAL_MODULE := libsecril-compat

include $(BUILD_SHARED_LIBRARY)
