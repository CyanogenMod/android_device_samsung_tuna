MLSDK_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libmlplatform

LOCAL_CFLAGS := -D_REENTRANT -DLINUX -DANDROID
LOCAL_CFLAGS += -Wall -Werror

LOCAL_C_INCLUDES := \
	$(MLSDK_PATH)/platform/include \
	$(MLSDK_PATH)/platform/include/linux \
	$(MLSDK_PATH)/platform/linux \
	$(MLSDK_PATH)/platform/linux/kernel \
	$(MLSDK_PATH)/mllite

LOCAL_SRC_FILES := \
	mlsdk/platform/linux/mlos_linux.c \
	mlsdk/platform/linux/mlsl_linux_mpu.c

LOCAL_SHARED_LIBRARIES := liblog libm libutils libcutils
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libmllite
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DNDEBUG -D_REENTRANT -DLINUX -DANDROID
LOCAL_CFLAGS += -DUNICODE -D_UNICODE -DSK_RELEASE
LOCAL_CFLAGS += -DI2CDEV=\"/dev/mpu\"
LOCAL_CFLAGS += -Wall -Werror

# optionally apply the compass filter. this is set in
# BoardConfig.mk
ifeq ($(BOARD_INVENSENSE_APPLY_COMPASS_NOISE_FILTER),true)
	LOCAL_CFLAGS += -DAPPLY_COMPASS_FILTER
endif

LOCAL_C_INCLUDES := \
	$(MLSDK_PATH)/mllite \
	$(MLSDK_PATH)/mlutils \
	$(MLSDK_PATH)/platform/include \
	$(MLSDK_PATH)/platform/include/linux \
	$(MLSDK_PATH)/platform/linux

LOCAL_SRC_FILES := \
	mlsdk/mllite/accel.c \
	mlsdk/mllite/compass.c \
	mlsdk/mllite/mldl_cfg_mpu.c \
	mlsdk/mllite/dmpDefault.c \
	mlsdk/mllite/ml.c \
	mlsdk/mllite/mlarray.c \
	mlsdk/mllite/mlFIFO.c \
	mlsdk/mllite/mlFIFOHW.c \
	mlsdk/mllite/mlMathFunc.c \
	mlsdk/mllite/ml_stored_data.c \
	mlsdk/mllite/mlcontrol.c \
	mlsdk/mllite/mldl.c \
	mlsdk/mllite/mldmp.c \
	mlsdk/mllite/mlstates.c \
	mlsdk/mllite/mlsupervisor.c \
	mlsdk/mllite/mlBiasNoMotion.c \
	mlsdk/mllite/mlSetGyroBias.c \
	mlsdk/mllite/mlcompat.c \
	mlsdk/mlutils/checksum.c \

LOCAL_SHARED_LIBRARIES := libm libutils libcutils liblog libmlplatform
include $(BUILD_SHARED_LIBRARY)
