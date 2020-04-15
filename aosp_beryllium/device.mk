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

PRODUCT_SOONG_NAMESPACES += \
    device/xiaomi/beryllium

# copied from crosshatch
# setup dalvik vm configs
$(call inherit-product, frameworks/native/build/phone-xhdpi-2048-dalvik-heap.mk)


PRODUCT_COPY_FILES := \
    $(LOCAL_PATH)/fstab.aosp_beryllium:$(TARGET_COPY_OUT_VENDOR)/etc/init/fstab.aosp_beryllium \
    $(LOCAL_PATH)/fstab.ramdisk:$(TARGET_COPY_OUT_RAMDISK)/fstab.aosp_beryllium \
    $(LOCAL_PATH)/fstab.ramdisk:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.aosp_beryllium \
    device/xiaomi/beryllium/init.common.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.aosp_beryllium.rc \
    device/xiaomi/beryllium/init.common.usb.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.aosp_beryllium.usb.rc \
    device/xiaomi/beryllium/common.kl:$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/aosp_beryllium.kl

# Build generic Audio HAL
PRODUCT_PACKAGES := audio.primary.aosp_beryllium

PRODUCT_PACKAGES += \
    pd-mapper \
    qrtr-lookup \
    rmtfs \
    tqftpserv

PRODUCT_COPY_FILES += \
    device/xiaomi/beryllium/qcom/init.qcom.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.qcom.rc
