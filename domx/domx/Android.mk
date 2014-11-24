LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	omx_rpc/src/omx_rpc.c \
	omx_rpc/src/omx_rpc_skel.c \
	omx_rpc/src/omx_rpc_stub.c \
	omx_rpc/src/omx_rpc_config.c \
	omx_rpc/src/omx_rpc_platform.c \
	omx_proxy_common/src/omx_proxy_common.c \
	profiling/src/profile.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/omx_rpc/inc \
	$(LOCAL_PATH)/../omx_core/inc \
	$(LOCAL_PATH)/../mm_osal/inc \
	$(LOCAL_PATH)/profiling/inc \
	$(DEVICE_FOLDER)/hwc/ \
	$(DEVICE_FOLDER)/libion_ti/ \
	system/core/include/cutils \
	hardware/libhardware/include

LOCAL_CFLAGS += -D_Android -DENABLE_GRALLOC_BUFFERS -DANDROID_QUIRK_LOCK_BUFFER -DUSE_ENHANCED_PORTRECONFIG -DUSE_ION

ifneq ($(DEBUG_FORCE_STRICT_ALIASING),true)
	LOCAL_CFLAGS += -fno-strict-aliasing
endif

LOCAL_SHARED_LIBRARIES := \
	libmm_osal \
	libc \
	liblog \
	libion_ti \
	libcutils

LOCAL_MODULE := libdomx
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
