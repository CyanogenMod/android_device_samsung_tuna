LOCAL_PATH:= $(call my-dir)
OMAP4_NEXT_FOLDER := hardware/ti/omap4

include $(LOCAL_PATH)/android-api.mk

TI_CAMERAHAL_INTERFACE ?= ALL

TI_CAMERAHAL_COMMON_CFLAGS := \
    $(ANDROID_API_CFLAGS) \
    -DLOG_TAG=\"CameraHal\" \
    -DCOPY_IMAGE_BUFFER \
    -fno-short-enums

ifdef TI_CAMERAHAL_DEBUG_ENABLED
    # Enable CameraHAL debug logs
    TI_CAMERAHAL_COMMON_CFLAGS += -DCAMERAHAL_DEBUG
endif

ifdef TI_CAMERAHAL_VERBOSE_DEBUG_ENABLED
    # Enable CameraHAL verbose debug logs
    TI_CAMERAHAL_COMMON_CFLAGS += -DCAMERAHAL_DEBUG_VERBOSE
endif

ifdef TI_CAMERAHAL_DEBUG_FUNCTION_NAMES
    # Enable CameraHAL function enter/exit logging
    TI_CAMERAHAL_COMMON_CFLAGS += -DTI_UTILS_FUNCTION_LOGGER_ENABLE
endif

ifdef TI_CAMERAHAL_DEBUG_TIMESTAMPS
    # Enable timestamp logging
    TI_CAMERAHAL_COMMON_CFLAGS += -DTI_UTILS_DEBUG_USE_TIMESTAMPS
endif

ifndef TI_CAMERAHAL_DONT_USE_RAW_IMAGE_SAVING
    # Enabled saving RAW images to file
    TI_CAMERAHAL_COMMON_CFLAGS += -DCAMERAHAL_USE_RAW_IMAGE_SAVING
endif

ifdef TI_CAMERAHAL_PROFILING
    # Enable OMX Camera component profiling
    TI_CAMERAHAL_COMMON_CFLAGS += -DCAMERAHAL_OMX_PROFILING
endif

ifdef TI_CAMERAHAL_MAX_CAMERAS_SUPPORTED
    TI_CAMERAHAL_COMMON_CFLAGS += -DMAX_CAMERAS_SUPPORTED=$(TI_CAMERAHAL_MAX_CAMERAS_SUPPORTED)
endif

ifdef TI_CAMERAHAL_TREAT_FRONT_AS_BACK
    TI_CAMERAHAL_COMMON_CFLAGS += -DTREAT_FRONT_AS_BACK
endif

ifeq ($(findstring omap5, $(TARGET_BOARD_PLATFORM)),omap5)
    TI_CAMERAHAL_COMMON_CFLAGS += -DCAMERAHAL_OMAP5_CAPTURE_MODES
endif

ifeq ($(ENHANCED_DOMX),true)
    TI_CAMERAHAL_COMMON_CFLAGS += -DENHANCED_DOMX
endif

ifdef ARCH_ARM_HAVE_NEON
    TI_CAMERAHAL_COMMON_CFLAGS += -DARCH_ARM_HAVE_NEON
endif

ifeq ($(BOARD_VENDOR),motorola-omap4)
    TI_CAMERAHAL_COMMON_CFLAGS += -DMOTOROLA_CAMERA
endif

ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),piranha)
    TI_CAMERAHAL_COMMON_CFLAGS += -DCAMERAHAL_PIRANHA
endif

ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),tuna)
    TI_CAMERAHAL_COMMON_CFLAGS += -DCAMERAHAL_TUNA
endif

TI_CAMERAHAL_OMX_CFLAGS := -DOMX_CAMERA_ADAPTER
TI_CAMERAHAL_USB_CFLAGS := -DV4L_CAMERA_ADAPTER


TI_CAMERAHAL_COMMON_INCLUDES := \
    $(OMAP4_NEXT_FOLDER)/include \
    $(OMAP4_NEXT_FOLDER)/hwc \
    external/jpeg \
    external/jhead \
    $(OMAP4_NEXT_FOLDER)/libtiutils \
    $(LOCAL_PATH)/inc \
    system/media/camera/include

ifdef ANDROID_API_JB_OR_LATER
TI_CAMERAHAL_COMMON_INCLUDES += \
    frameworks/native/include/media/hardware
else
TI_CAMERAHAL_COMMON_INCLUDES += \
    frameworks/base/include/media/stagefright
endif

TI_CAMERAHAL_OMX_INCLUDES := \
    frameworks/native/include/media/openmax \
    $(DOMX_PATH)/mm_osal/inc \
    $(DOMX_PATH)/omx_core/inc \
    $(LOCAL_PATH)/inc/OMXCameraAdapter

TI_CAMERAHAL_USB_INCLUDES := \
    $(LOCAL_PATH)/inc/V4LCameraAdapter


TI_CAMERAHAL_COMMON_SRC := \
    CameraHal_Module.cpp \
    CameraHal.cpp \
    CameraHalUtilClasses.cpp \
    AppCallbackNotifier.cpp \
    ANativeWindowDisplayAdapter.cpp \
    BufferSourceAdapter.cpp \
    CameraProperties.cpp \
    BaseCameraAdapter.cpp \
    MemoryManager.cpp \
    Encoder_libjpeg.cpp \
    Decoder_libjpeg.cpp \
    SensorListener.cpp  \
    NV12_resize.cpp \
    CameraParameters.cpp \
    TICameraParameters.cpp \
    CameraHalCommon.cpp \
    FrameDecoder.cpp \
    SwFrameDecoder.cpp \
    OmxFrameDecoder.cpp \
    DecoderFactory.cpp

TI_CAMERAHAL_OMX_SRC := \
    OMXCameraAdapter/OMX3A.cpp \
    OMXCameraAdapter/OMXAlgo.cpp \
    OMXCameraAdapter/OMXCameraAdapter.cpp \
    OMXCameraAdapter/OMXCapabilities.cpp \
    OMXCameraAdapter/OMXCapture.cpp \
    OMXCameraAdapter/OMXReprocess.cpp \
    OMXCameraAdapter/OMXDefaults.cpp \
    OMXCameraAdapter/OMXExif.cpp \
    OMXCameraAdapter/OMXFD.cpp \
    OMXCameraAdapter/OMXFocus.cpp \
    OMXCameraAdapter/OMXMetadata.cpp \
    OMXCameraAdapter/OMXZoom.cpp \
    OMXCameraAdapter/OMXDccDataSave.cpp

ifdef TI_CAMERAHAL_USES_LEGACY_DOMX_DCC
TI_CAMERAHAL_OMX_CFLAGS += -DUSES_LEGACY_DOMX_DCC
else
TI_CAMERAHAL_OMX_SRC += OMXCameraAdapter/OMXDCC.cpp
endif

TI_CAMERAHAL_USB_SRC := \
    V4LCameraAdapter/V4LCameraAdapter.cpp \
    V4LCameraAdapter/V4LCapabilities.cpp


TI_CAMERAHAL_EXIF_LIBRARY := libexif
# libexif is now libjhead in later API levels.

ifdef ANDROID_API_KK_OR_LATER
ifdef ANDROID_API_LP_OR_LATER
    TI_CAMERAHAL_EXIF_LIBRARY := libjhead
else ifneq ($(filter 4.4.3 4.4.4,$(PLATFORM_VERSION)),)
    # Only 4.4.3 and 4.4.4 KK use libjhead
    TI_CAMERAHAL_EXIF_LIBRARY := libjhead
endif
endif

TI_CAMERAHAL_COMMON_SHARED_LIBRARIES := \
    libui \
    libbinder \
    libutils \
    libcutils \
    liblog \
    libtiutils \
    libcamera_client \
    libgui \
    libjpeg \
    $(TI_CAMERAHAL_EXIF_LIBRARY)

ifdef ANDROID_API_JB_MR1_OR_LATER
TI_CAMERAHAL_COMMON_SHARED_LIBRARIES += \
    libion_ti
TI_CAMERAHAL_COMMON_CFLAGS += -DUSE_LIBION_TI
else
TI_CAMERAHAL_COMMON_SHARED_LIBRARIES += \
    libion
endif

TI_CAMERAHAL_OMX_SHARED_LIBRARIES := \
    libmm_osal \
    libOMX_Core \
    libdomx


ifdef OMAP_ENHANCEMENT_CPCAM
TI_CAMERAHAL_COMMON_STATIC_LIBRARIES += \
    libcpcamcamera_client
endif


ifeq ($(TI_CAMERAHAL_INTERFACE),OMX)
# ====================
#  OMX Camera Adapter
# --------------------

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    $(TI_CAMERAHAL_COMMON_SRC) \
    $(TI_CAMERAHAL_OMX_SRC)

LOCAL_C_INCLUDES := \
    $(TI_CAMERAHAL_COMMON_INCLUDES) \
    $(TI_CAMERAHAL_OMX_INCLUDES)

LOCAL_SHARED_LIBRARIES := \
    $(TI_CAMERAHAL_COMMON_SHARED_LIBRARIES) \
    $(TI_CAMERAHAL_OMX_SHARED_LIBRARIES)

LOCAL_STATIC_LIBRARIES := $(TI_CAMERAHAL_COMMON_STATIC_LIBRARIES)

LOCAL_CFLAGS := \
    $(TI_CAMERAHAL_COMMON_CFLAGS) \
    $(TI_CAMERAHAL_OMX_CFLAGS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.tuna
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

else ifeq ($(TI_CAMERAHAL_INTERFACE),USB)
# ====================
#  USB Camera Adapter
# --------------------

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    $(TI_CAMERAHAL_COMMON_SRC) \
    $(TI_CAMERAHAL_USB_SRC)

LOCAL_C_INCLUDES := \
    $(TI_CAMERAHAL_COMMON_INCLUDES) \
    $(TI_CAMERAHAL_USB_INCLUDES)

LOCAL_SHARED_LIBRARIES := \
    $(TI_CAMERAHAL_COMMON_SHARED_LIBRARIES)

LOCAL_STATIC_LIBRARIES := $(TI_CAMERAHAL_COMMON_STATIC_LIBRARIES)

LOCAL_CFLAGS := \
    $(TI_CAMERAHAL_COMMON_CFLAGS) \
    $(TI_CAMERAHAL_USB_CFLAGS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.tuna
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

else ifeq ($(TI_CAMERAHAL_INTERFACE),ALL)
# =====================
#  ALL Camera Adapters
# ---------------------

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    $(TI_CAMERAHAL_COMMON_SRC) \
    $(TI_CAMERAHAL_OMX_SRC) \
    $(TI_CAMERAHAL_USB_SRC)

LOCAL_C_INCLUDES := \
    $(TI_CAMERAHAL_COMMON_INCLUDES) \
    $(TI_CAMERAHAL_OMX_INCLUDES) \
    $(TI_CAMERAHAL_USB_INCLUDES)

LOCAL_SHARED_LIBRARIES := \
    $(TI_CAMERAHAL_COMMON_SHARED_LIBRARIES) \
    $(TI_CAMERAHAL_OMX_SHARED_LIBRARIES)

LOCAL_STATIC_LIBRARIES := $(TI_CAMERAHAL_COMMON_STATIC_LIBRARIES)

LOCAL_CFLAGS := \
    $(TI_CAMERAHAL_COMMON_CFLAGS) \
    $(TI_CAMERAHAL_OMX_CFLAGS) \
    $(TI_CAMERAHAL_USB_CFLAGS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE := camera.tuna
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif

$(clear-android-api-vars)
