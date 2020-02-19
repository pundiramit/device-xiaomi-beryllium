ifndef TARGET_KERNEL_USE

KERNEL_MODS := $(wildcard device/linaro/dragonboard-kernel/android-5.4/*.ko)

# Local modules are built from following tree:
# https://github.com/pundiramit/linux/tree/android-5.4-modules
LOCAL_MODS := $(wildcard device/xiaomi/beryllium/prebuilt-kernel/android-5.4/*.ko)

# Skip copying modules broken on android-5.4
# Use local module copy instead
SKIP_MODS := %/msm.ko

# BT modules go to vendor partition
ONLY_VENDOR := %/btqca.ko %/hci_uart.ko
BOARD_VENDOR_KERNEL_MODULES := $(filter $(ONLY_VENDOR),$(KERNEL_MODS))

BOARD_VENDOR_RAMDISK_KERNEL_MODULES := $(filter-out $(SKIP_MODS) $(ONLY_VENDOR),$(KERNEL_MODS))
BOARD_VENDOR_RAMDISK_KERNEL_MODULES += $(LOCAL_MODS)

else # ifdef TARGET_KERNEL_USE

KERNEL_MODS := $(wildcard device/xiaomi/beryllium/prebuilt-kernel/android-$(TARGET_KERNEL_USE)/*.ko)

# Following modules go to vendor partition
ONLY_VENDOR := %/btqca.ko %/hci_uart.ko

BOARD_VENDOR_KERNEL_MODULES := $(filter $(ONLY_VENDOR),$(KERNEL_MODS))
BOARD_VENDOR_RAMDISK_KERNEL_MODULES := $(filter-out $(ONLY_VENDOR),$(KERNEL_MODS))

endif

# Inherit the full_base and device configurations
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, device/xiaomi/beryllium/aosp_beryllium/device.mk)
$(call inherit-product, device/xiaomi/beryllium/device-common.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

# Product overrides
PRODUCT_NAME := aosp_beryllium
PRODUCT_DEVICE := aosp_beryllium
PRODUCT_BRAND := AOSP
