#
# Copyright (C) 2011 The Android Open-Source Project
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
#

# WARNING: Everything listed here will be built on ALL platforms,
# including x86, the emulator, and the SDK.  Modules must be uniquely
# named (liblights.tuna), and must build everywhere, or limit themselves
# to only building on ARM if they include assembly. Individual makefiles
# are responsible for having their own logic, for fine-grained control.

LOCAL_PATH := $(call my-dir)

ifeq ($(BOARD_CREATE_TUNA_HDCP_KEYS_SYMLINK), true)
include $(CLEAR_VARS)

LOCAL_MODULE := tuna_hdcp_keys
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := FAKE
LOCAL_MODULE_SUFFIX := -timestamp

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): HDCP_KEYS_FILE := /factory/hdcp.keys
$(LOCAL_BUILT_MODULE): SYMLINK := $(TARGET_OUT_VENDOR)/firmware/hdcp.keys
$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/Android.mk
$(LOCAL_BUILT_MODULE):
	$(hide) echo "Symlink: $(SYMLINK) -> $(HDCP_KEYS_FILE)"
	$(hide) mkdir -p $(dir $@)
	$(hide) mkdir -p $(dir $(SYMLINK))
	$(hide) rm -rf $@
	$(hide) rm -rf $(SYMLINK)
	$(hide) ln -sf $(HDCP_KEYS_FILE) $(SYMLINK)
	$(hide) touch $@


#Creating Gralloc SymLink
GRALLOC_SYMLINK := $(TARGET_OUT_VENDOR)/lib/hw/gralloc.$(TARGET_BOARD_PLATFORM).so
$(GRALLOC_SYMLINK): GRALLOC_FILE := gralloc.omap$(TARGET_BOARD_OMAP_CPU).so
$(GRALLOC_SYMLINK): $(LOCAL_INSTALLED_MODULE) $(LOCAL_PATH)/Android.mk
	@echo "Symlink: $@ -> $(GRALLOC_FILE)"
	@mkdir -p $(TARGET_OUT_VENDOR)/lib/hw
	@rm -rf $@
	$(hide) ln -fs $(GRALLOC_FILE) $@

ALL_DEFAULT_INSTALLED_MODULES += $(GRALLOC_SYMLINK)

# for mm/mmm
all_modules: $(GRALLOC_SYMLINK)

include $(call all-makefiles-under,$(LOCAL_PATH))

endif
