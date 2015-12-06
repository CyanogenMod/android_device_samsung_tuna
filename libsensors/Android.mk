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
LOCAL_PATH := $(call my-dir)

include $(call all-named-subdir-makefiles,mlsdk)

# HAL module implemenation stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

LOCAL_MODULE := sensors.tuna

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/mlsdk/platform/include \
	$(LOCAL_PATH)/mlsdk/platform/include/linux \
	$(LOCAL_PATH)/mlsdk/platform/linux \
	$(LOCAL_PATH)/mlsdk/mllite \
	$(LOCAL_PATH)/mlsdk/mldmp \
	$(LOCAL_PATH)/mlsdk/external/aichi \
	$(LOCAL_PATH)/mlsdk/external/akmd

LOCAL_SRC_FILES := \
	sensors.cpp \
	SensorBase.cpp \
	MPLSensor.cpp \
	InputEventReader.cpp \
	LightSensor.cpp \
	ProximitySensor.cpp \
	PressureSensor.cpp \
	SamsungSensorBase.cpp \
	TemperatureSensor.cpp

LOCAL_SHARED_LIBRARIES := liblog libcutils libutils libdl libmllite libmlplatform
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
LOCAL_CPPFLAGS := -DLINUX=1
LOCAL_CLANG := true
LOCAL_CFLAGS += -Wall -Werror

include $(BUILD_SHARED_LIBRARY)
