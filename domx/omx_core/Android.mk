LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/OMX_Core.c \
	src/OMX_Core_Wrapper.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/inc \
	$(LOCAL_PATH)/../mm_osal/inc

LOCAL_SHARED_LIBRARIES := \
	libdl \
	liblog \
	libmm_osal

LOCAL_CFLAGS += -DSTATIC_TABLE -D_Android -DCHECK_SECURE_STATE

LOCAL_MODULE := libOMX_Core
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
