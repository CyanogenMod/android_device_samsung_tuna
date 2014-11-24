LOCAL_PATH := $(call my-dir)

OMAP4_CAMERA_HAL_USES := OMX
# OMAP4_CAMERA_HAL_USES := USB

OMAP4_CAMERA_HAL_SRC := \
	CameraHal_Module.cpp \
	CameraHal.cpp \
	CameraHalUtilClasses.cpp \
	AppCallbackNotifier.cpp \
	ANativeWindowDisplayAdapter.cpp \
	CameraProperties.cpp \
	MemoryManager.cpp \
	Encoder_libjpeg.cpp \
	SensorListener.cpp  \
	NV12_resize.c

OMAP4_CAMERA_COMMON_SRC := \
	CameraParameters.cpp \
	TICameraParameters.cpp \
	CameraHalCommon.cpp

OMAP4_CAMERA_OMX_SRC := \
	BaseCameraAdapter.cpp \
	OMXCameraAdapter/OMX3A.cpp \
	OMXCameraAdapter/OMXAlgo.cpp \
	OMXCameraAdapter/OMXCameraAdapter.cpp \
	OMXCameraAdapter/OMXCapabilities.cpp \
	OMXCameraAdapter/OMXCapture.cpp \
	OMXCameraAdapter/OMXDefaults.cpp \
	OMXCameraAdapter/OMXExif.cpp \
	OMXCameraAdapter/OMXFD.cpp \
	OMXCameraAdapter/OMXFocus.cpp \
	OMXCameraAdapter/OMXZoom.cpp

OMAP4_CAMERA_USB_SRC := \
	BaseCameraAdapter.cpp \
	V4LCameraAdapter/V4LCameraAdapter.cpp

#
# OMX Camera HAL 
#

ifeq ($(OMAP4_CAMERA_HAL_USES),OMX)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	$(OMAP4_CAMERA_HAL_SRC) \
	$(OMAP4_CAMERA_OMX_SRC) \
	$(OMAP4_CAMERA_COMMON_SRC)

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/inc/ \
	$(DEVICE_FOLDER)/hwc \
	$(DEVICE_FOLDER)/include \
	$(LOCAL_PATH)/inc/OMXCameraAdapter \
	$(DEVICE_FOLDER)/libtiutils \
	hardware/ti/omap4xxx/tiler \
	$(DEVICE_FOLDER)/libion_ti \
	$(DOMX_PATH)/omx_core/inc \
	$(DOMX_PATH)/mm_osal/inc \
	frameworks/base/include/media/stagefright \
	frameworks/native/include/media/hardware \
	frameworks/native/include/media/openmax \
	external/jpeg \
	external/jhead

LOCAL_SHARED_LIBRARIES := \
	libui \
	libbinder \
	libutils \
	libcutils \
	liblog \
	libtiutils_custom \
	libmm_osal \
	libOMX_Core \
	libcamera_client \
	libgui \
	libdomx \
	libion_ti \
	libjpeg \
	libjhead

LOCAL_CFLAGS := -fno-short-enums -DCOPY_IMAGE_BUFFER

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.$(TARGET_BOOTLOADER_BOARD_NAME)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

else
ifeq ($(OMAP4_CAMERA_HAL_USES),USB)

#
# USB Camera Adapter
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	$(OMAP4_CAMERA_HAL_SRC) \
	$(OMAP4_CAMERA_USB_SRC) \
	$(OMAP4_CAMERA_COMMON_SRC)

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/inc/ \
	$(DEVICE_FOLDER)/hwc \
	$(DEVICE_FOLDER)/include \
	$(LOCAL_PATH)/inc/V4LCameraAdapter \
	$(DEVICE_FOLDER)/libtiutils \
	hardware/ti/omap4xxx/tiler \
	$(DEVICE_FOLDER)/libion_ti \
	frameworks/base/include/ui \
	frameworks/base/include/utils \
	frameworks/base/include/media/stagefright/openmax

LOCAL_SHARED_LIBRARIES := \
	libui \
	libbinder \
	libutils \
	libcutils \
	liblog \
	libtiutils_custom \
	libcamera_client \
	libion_ti

LOCAL_CFLAGS := -fno-short-enums -DCOPY_IMAGE_BUFFER

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.$(TARGET_BOOTLOADER_BOARD_NAME)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif
endif 
