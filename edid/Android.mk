# Copyright (C) Texas Instruments - http://www.ti.com/
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

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    lib/edid_parser.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libedid

include $(BUILD_SHARED_LIBRARY)

# ====================
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    cmd/parse_hdmi_edid.c

LOCAL_SHARED_LIBRARIES:= \
    libutils \
    libedid

LOCAL_MODULE:= parse_hdmi_edid
LOCAL_MODULE_TAGS:= optional

include $(BUILD_EXECUTABLE)

