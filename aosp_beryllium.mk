# Kernel tree is hosted at https://github.com/pundiramit/linux/tree/display (beryllium_defconfig).
# cat arch/arm64/boot/Image.gz arch/arm64/boot/dts/qcom/sdm845-beryllium.dtb > Image.gz-dtb-beryllium
TARGET_PREBUILT_KERNEL ?= Image.gz-dtb-beryllium-5.2

# Inherit the full_base and device configurations
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, device/xiaomi/beryllium/aosp_beryllium/device.mk)
$(call inherit-product, device/xiaomi/beryllium/device-common.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

# Product overrides
PRODUCT_NAME := aosp_beryllium
PRODUCT_DEVICE := aosp_beryllium
PRODUCT_BRAND := AOSP
