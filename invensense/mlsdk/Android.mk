LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libmlplatform

LOCAL_CFLAGS := -D_REENTRANT -DLINUX -DANDROID
LOCAL_CFLAGS += -DCONFIG_MPU_SENSORS_MPU3050

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/platform/include \
	$(LOCAL_PATH)/platform/include/linux \
	$(LOCAL_PATH)/platform/linux \
	$(LOCAL_PATH)/platform/linux/kernel \
	$(LOCAL_PATH)/mllite

LOCAL_SRC_FILES := \
	platform/linux/mlos_linux.c \
	platform/linux/mlsl_linux_mpu.c

LOCAL_SHARED_LIBRARIES := liblog libm libutils libcutils
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libmllite
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DNDEBUG -D_REENTRANT -DLINUX -DANDROID
LOCAL_CFLAGS += -DCONFIG_MPU_SENSORS_MPU3050
LOCAL_CFLAGS += -DUNICODE -D_UNICODE -DSK_RELEASE
LOCAL_CFLAGS += -DI2CDEV=\"/dev/mpu\"

# optionally apply the compass filter. this is set in
# BoardConfig.mk
ifeq ($(BOARD_INVENSENSE_APPLY_COMPASS_NOISE_FILTER),true)
	LOCAL_CFLAGS += -DAPPLY_COMPASS_FILTER
endif

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/mldmp \
	$(LOCAL_PATH)/mllite \
	$(LOCAL_PATH)/platform/include \
	$(LOCAL_PATH)/platform/include/linux \
	$(LOCAL_PATH)/mlutils \
	$(LOCAL_PATH)/mlapps/common \
	$(LOCAL_PATH)/mllite/akmd \
	$(LOCAL_PATH)/platform/linux

LOCAL_SRC_FILES := \
	mllite/accel.c \
	mllite/compass.c \
	mllite/pressure.c \
	mllite/mldl_cfg_mpu.c \
	mllite/dmpDefault.c \
	mllite/ml.c \
	mllite/mlarray.c \
	mllite/mlarray_legacy.c \
	mllite/mlFIFO.c \
	mllite/mlFIFOHW.c \
	mllite/mlMathFunc.c \
	mllite/ml_stored_data.c \
	mllite/mlcontrol.c \
	mllite/mldl.c \
	mllite/mldmp.c \
	mllite/mlstates.c \
	mllite/mlsupervisor.c \
	mllite/mlBiasNoMotion.c \
	mllite/mlSetGyroBias.c \
	mllite/ml_mputest.c \
	mlutils/mputest.c \
	mlutils/checksum.c

LOCAL_SHARED_LIBRARIES := libm libutils libcutils liblog libmlplatform
include $(BUILD_SHARED_LIBRARY)
