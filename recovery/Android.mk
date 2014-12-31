ifneq (,$(findstring $(TARGET_DEVICE),tuna toro toroplus maguro))

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# Edify extension functions for doing bootloader updates on Tuna devices.

LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += bootable/recovery system/vold external/openssl/include
LOCAL_SRC_FILES := recovery_updater.c bootloader.c

# should match TARGET_RECOVERY_UPDATER_LIBS set in BoardConfig.mk
LOCAL_MODULE := librecovery_updater_tuna

include $(BUILD_STATIC_LIBRARY)

endif
