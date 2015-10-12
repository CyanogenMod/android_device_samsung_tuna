# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Modified 2011 by InvenSense, Inc

LOCAL_PATH := $(call my-dir)

# InvenSense fragment of the HAL
include $(CLEAR_VARS)

LOCAL_MODULE := libinvensense_hal.$(TARGET_BOOTLOADER_BOARD_NAME)

LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\" -Werror -Wall
LOCAL_CFLAGS += -DCONFIG_MPU_SENSORS_MPU3050=1

LOCAL_SRC_FILES := SensorBase.cpp MPLSensor.cpp

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../mlsdk/platform/include \
	$(LOCAL_PATH)/../mlsdk/platform/include/linux \
	$(LOCAL_PATH)/../mlsdk/platform/linux \
	$(LOCAL_PATH)/../mlsdk/mllite \
	$(LOCAL_PATH)/../mlsdk/mldmp \
	$(LOCAL_PATH)/../mlsdk/external/aichi \
	$(LOCAL_PATH)/../mlsdk/external/akmd

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libdl libmllite libmlplatform
LOCAL_CPPFLAGS := -DLINUX=1
LOCAL_LDFLAGS := -rdynamic

include $(BUILD_SHARED_LIBRARY)
