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

# Copied from crosshatch
# Set lmkd options
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.lmk.low=1001 \
    ro.lmk.medium=800 \
    ro.lmk.critical=0 \
    ro.lmk.critical_upgrade=false \
    ro.lmk.upgrade_pressure=100 \
    ro.lmk.downgrade_pressure=100 \
    ro.lmk.kill_heaviest_task=true \
    ro.lmk.kill_timeout_ms=100 \
    ro.lmk.use_minfree_levels=true \

$(call inherit-product-if-exists, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

PRODUCT_COPY_FILES := \
    device/xiaomi/beryllium/prebuilt-kernel/$(TARGET_PREBUILT_KERNEL):kernel \
    $(LOCAL_PATH)/fstab.aosp_beryllium:$(TARGET_COPY_OUT_VENDOR)/etc/init/fstab.aosp_beryllium \
    $(LOCAL_PATH)/fstab.ramdisk:$(TARGET_COPY_OUT_RAMDISK)/fstab.aosp_beryllium \
    device/xiaomi/beryllium/init.common.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.aosp_beryllium.rc \
    device/xiaomi/beryllium/init.common.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.aosp_beryllium.usb.rc \
    device/xiaomi/beryllium/ueventd.common.rc:$(TARGET_COPY_OUT_VENDOR)/ueventd.aosp_beryllium.rc \
    device/xiaomi/beryllium/common.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/aosp_beryllium.kl

# Build generic Audio HAL
PRODUCT_PACKAGES := audio.primary.aosp_beryllium

# Copy firmware files
$(call inherit-product-if-exists, device/xiaomi/beryllium/firmware/device.mk)
