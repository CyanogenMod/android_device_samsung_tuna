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

DEVICE_FOLDER := device/samsung/tuna

# inherit from omap4
-include hardware/ti/omap4/BoardConfigCommon.mk

PRODUCT_VENDOR_KERNEL_HEADERS += $(DEVICE_FOLDER)/kernel-headers

# CM
BOARD_HARDWARE_CLASS := $(DEVICE_FOLDER)/cmhw
BACKLIGHT_PATH := "/sys/class/backlight/s6e8aa0/brightness"

# Use the non-open-source parts, if they're present
-include vendor/samsung/tuna/BoardConfigVendor.mk

TARGET_NO_BOOTLOADER := true
TARGET_NO_RADIOIMAGE := true

TARGET_BOARD_INFO_FILE := $(DEVICE_FOLDER)/board-info.txt
TARGET_BOOTLOADER_BOARD_NAME := tuna

# Processor
TARGET_BOARD_OMAP_CPU := 4460

# Kernel
BOARD_KERNEL_BASE := 0x80000000
# BOARD_KERNEL_CMDLINE :=
TARGET_KERNEL_CONFIG := cyanogenmod_tuna_defconfig
TARGET_KERNEL_SOURCE := kernel/samsung/tuna

# Use dlmalloc
MALLOC_IMPL := dlmalloc

# External SGX Module
SGX_MODULES:
	make clean -C $(HARDWARE_TI_OMAP4_BASE)/pvr-source/eurasiacon/build/linux2/omap4430_android
	cp $(TARGET_KERNEL_SOURCE)/drivers/video/omap2/omapfb/omapfb.h $(KERNEL_OUT)/drivers/video/omap2/omapfb/omapfb.h
	make -j8 -C $(HARDWARE_TI_OMAP4_BASE)/pvr-source/eurasiacon/build/linux2/omap4430_android ARCH=arm KERNEL_CROSS_COMPILE=arm-eabi- CROSS_COMPILE=arm-eabi- KERNELDIR=$(KERNEL_OUT) TARGET_PRODUCT="blaze_tablet" BUILD=release TARGET_SGX=540 PLATFORM_VERSION=4.0
	mv $(KERNEL_OUT)/../../target/kbuild/pvrsrvkm_sgx540_120.ko $(KERNEL_MODULES_OUT)
	$(ARM_EABI_TOOLCHAIN)/arm-eabi-strip --strip-unneeded $(KERNEL_MODULES_OUT)/pvrsrvkm_sgx540_120.ko

TARGET_KERNEL_MODULES += SGX_MODULES

TI_CAMERAHAL_USES_LEGACY_DOMX_DCC := true
DOMX_TUNA := true
COMMON_GLOBAL_CFLAGS += -DDOMX_TUNA
TI_CAMERAHAL_MAX_CAMERAS_SUPPORTED := 2
TI_CAMERAHAL_DEBUG_ENABLED := true

# Include HDCP keys
BOARD_CREATE_TUNA_HDCP_KEYS_SYMLINK := true

# set if the target supports FBIO_WAITFORVSYNC
TARGET_HAS_WAITFORVSYNC := true

# device-specific extensions to the updater binary
TARGET_RECOVERY_UPDATER_LIBS += librecovery_updater_tuna
TARGET_RELEASETOOLS_EXTENSIONS := $(DEVICE_FOLDER)

# Filesystem
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 685768704
BOARD_USERDATAIMAGE_PARTITION_SIZE := 14539537408
BOARD_FLASH_BLOCK_SIZE := 4096

# Disable journaling on system.img to save space.
BOARD_SYSTEMIMAGE_JOURNAL_SIZE := 0

# Enable dex-preoptimization to speed up first boot sequence
WITH_DEXPREOPT := true

# Needed for RIL
TARGET_NEEDS_BIONIC_MD5 := true

# Wifi
BOARD_WLAN_DEVICE                := bcmdhd
BOARD_WLAN_DEVICE_REV            := bcm4330_b2
WPA_SUPPLICANT_VERSION           := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER      := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER             := NL80211
BOARD_HOSTAPD_PRIVATE_LIB        := lib_driver_cmd_bcmdhd
WIFI_DRIVER_FW_PATH_PARAM        := "/sys/module/bcmdhd/parameters/firmware_path"
#WIFI_DRIVER_MODULE_PATH         := "/system/lib/modules/bcmdhd.ko"
WIFI_DRIVER_FW_PATH_STA          := "/vendor/firmware/fw_bcmdhd.bin"
WIFI_DRIVER_FW_PATH_AP           := "/vendor/firmware/fw_bcmdhd_apsta.bin"
WIFI_BAND                        := 802_11_ABG

# Bluetooth
BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(DEVICE_FOLDER)/bluetooth

BOARD_HAL_STATIC_LIBRARIES := libdumpstate.tuna

# SELinux
BOARD_SEPOLICY_DIRS += \
	$(DEVICE_FOLDER)/sepolicy

BOARD_SEPOLICY_UNION += \
	genfs_contexts \
	file_contexts \
	bluetooth.te \
	dumpdcc.te \
	init.te \
	mediaserver.te \
	pvrsrvinit.te \
	rild.te

# Recovery
RECOVERY_FSTAB_VERSION := 2
TARGET_RECOVERY_FSTAB = $(DEVICE_FOLDER)/rootdir/fstab.tuna
TARGET_RECOVERY_PIXEL_FORMAT := "BGRA_8888"
BOARD_RECOVERY_SWIPE := true
BOARD_HAS_NO_SELECT_BUTTON := true
BOARD_USE_CUSTOM_RECOVERY_FONT := \"roboto_15x24.h\"
