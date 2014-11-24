################################################

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
	MessageQueue.cpp \
	Semaphore.cpp \
	ErrorUtils.cpp
    
LOCAL_SHARED_LIBRARIES := \
	libdl \
	libui \
	libbinder \
	libutils \
	libcutils \
	liblog

LOCAL_C_INCLUDES += \
	bionic/libc/include

ifneq ($(TI_CUSTOM_DOMX_PATH),)
LOCAL_C_INCLUDES += \
	$(TI_CUSTOM_DOMX_PATH)/omx_core/inc \
	$(TI_CUSTOM_DOMX_PATH)/mm_osal/inc
else
LOCAL_C_INCLUDES += \
	hardware/ti/omap4xxx/domx/omx_core/inc \
	hardware/ti/omap4xxx/domx/mm_osal/inc
endif

LOCAL_CFLAGS += -fno-short-enums 

LOCAL_MODULE := libtiutils_custom
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
