DB845C_KERNEL_DIR := device/linaro/dragonboard-kernel/android-5.4
DB845C_MODS := $(wildcard $(DB845C_KERNEL_DIR)/*.ko)

# Local modules are built from following tree:
# https://github.com/pundiramit/linux/tree/android-5.4-modules
LOCAL_KERNEL_DIR := device/xiaomi/beryllium/prebuilt-kernel/android-5.4
LOCAL_MODS := $(wildcard $(LOCAL_KERNEL_DIR)/*.ko)

# Skip copying modules broken on android-5.4
# Use local module copy instead
DB845C_SKIP_MODS := %/msm.ko

# BT modules go to vendor partition
DB845C_ONLY_VENDOR := %/btqca.ko %/hci_uart.ko
BOARD_VENDOR_KERNEL_MODULES := $(filter $(DB845C_ONLY_VENDOR),$(DB845C_MODS))

BOARD_VENDOR_RAMDISK_KERNEL_MODULES := $(filter-out $(DB845C_SKIP_MODS) $(DB845C_ONLY_VENDOR),$(DB845C_MODS))
BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(LOCAL_MODS)

# Inherit the full_base and device configurations
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, device/xiaomi/beryllium/aosp_beryllium/device.mk)
$(call inherit-product, device/xiaomi/beryllium/device-common.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

# Product overrides
PRODUCT_NAME := aosp_beryllium
PRODUCT_DEVICE := aosp_beryllium
PRODUCT_BRAND := AOSP
