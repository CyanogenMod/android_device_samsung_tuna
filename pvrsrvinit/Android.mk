LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := pvrsrvinit.c
LOCAL_LDFLAGS := -L vendor/samsung/tuna/proprietary/vendor/lib
LOCAL_LDLIBS := -lsrv_init_SGX540_120 -lsrv_um_SGX540_120
LOCAL_MODULE_PATH := $(TARGET_OUT)/bin/
LOCAL_MODULE := pvrsrvinit
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

#Create PVR SymLink
include $(CLEAR_VARS)

LOCAL_MODULE := libPVRScopeServices.so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := FAKE

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): PVR_FILE := libPVRScopeServices_SGX540_120.so
$(LOCAL_BUILT_MODULE): SYMLINK := $(TARGET_OUT_VENDOR)/lib/$(LOCAL_MODULE)
$(LOCAL_BUILT_MODULE): $(LOCAL_PATH)/Android.mk
$(LOCAL_BUILT_MODULE):
	$(hide) echo "Symlink: $(SYMLINK) -> $(PVR_FILE)"
	$(hide) mkdir -p $(dir $@)
	$(hide) mkdir -p $(dir $(SYMLINK))
	$(hide) rm -rf $@
	$(hide) rm -rf $(SYMLINK)
	$(hide) ln -sf $(PVR_FILE) $(SYMLINK)
	$(hide) touch $@
