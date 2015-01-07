################################################

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    DebugUtils.cpp \
    MessageQueue.cpp \
    Semaphore.cpp \
    ErrorUtils.cpp

LOCAL_SHARED_LIBRARIES:= \
    libdl \
    libui \
    libbinder \
    libutils \
    libcutils

LOCAL_C_INCLUDES += \
    frameworks/native/include

LOCAL_C_INCLUDES += \
    bionic/libc/include 

LOCAL_C_INCLUDES += \
    $(DOMX_PATH)/omx_core/inc \
    $(DOMX_PATH)/mm_osal/inc

LOCAL_CFLAGS += -fno-short-enums $(ANDROID_API_CFLAGS)

ifdef TI_UTILS_MESSAGE_QUEUE_DEBUG_ENABLED
    # Enable debug logs
    LOCAL_CFLAGS += -DMSGQ_DEBUG
endif

ifdef TI_UTILS_MESSAGE_QUEUE_DEBUG_FUNCTION_NAMES
    # Enable function enter/exit logging
    LOCAL_CFLAGS += -DTI_UTILS_FUNCTION_LOGGER_ENABLE
endif

LOCAL_MODULE:= libtiutils_custom
LOCAL_MODULE_TAGS:= optional

include $(BUILD_SHARED_LIBRARY)
