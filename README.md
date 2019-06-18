# device/xiaomi/beryllium (AOSP device config for Xiaomi Poco F1)

# How to unlock and root Poco F1?
```
IMPORTANT NOTICE -->

UNLOCKING AND ROOTING MAY VOID YOUR PHONE WARRANTY AND
MAY BRICK YOUR DEVICE AS WELL. I'M NOT RESPONSIBLE FOR
EITHER OF THAT.
```

Here is a reasonable guide to get you started on
unlocking and rooting Poco F1 -->
https://forum.xda-developers.com/poco-f1/how-to/xiaomi-poco-f1-unlock-bootloader-custom-t3839405

Just for the records I downloaded and installed following
external packages to unlock and root my device-->
* miflash_unlock-en-3.3.525.23.zip (MS Windows only tool for unlocking)
* twrp-3.3.0-0-beryllium.img (Recovery)
* beryllium-9.6.10-9.0-vendor-firmware.zip (LineageOS dependency)
* lineage-16.0-20190612-nightly-beryllium-signed.zip
* Magisk-v19.3.zip (Root)
* MagiskManager-v7.2.0.apk

Also Dont forget to take a backup of your images from
TWRP and copy them to your Host machine. It will come
very handy. Believe me :)

# How to build and flash aosp_beryllium images?

* Download source and build AOSP images for Poco F1 (Beryllium) -->

```
repo init -u https://android.googlesource.com/platform/manifest -b master
git clone git@github.com:pundiramit/android-local-manifests.git .repo/local_manifests
repo sync -j$nproc
source build/envsetup.sh
lunch aosp_beryllium-userdebug
make -j$nproc
```

* Flash and boot AOSP images -->

Avoid a known smmu crash by turning OFF the display -->
```
fastboot oem select-display-panel none
fastboot reboot bootloader
```

Flash AOSP images and reboot -->
```
fastboot flash system system.img
fastboot flash vendor vendor.img
fastboot flash userdata userdata.img
fastboot flash boot boot.img
fastboot reboot
```

# Boot AOSP images.

* Use ADB to login. These images do not boot to UI due
to missing display drm and panel driver support. Login
to Android console instead -->
```
adb shell
```

* When done with testing, reboot into fastboot  mode,
turn ON the display -->
```
adb reboot bootloader
fastboot oem select-display-panel
fastboot reboot bootloader
```

# How to run custom kernels?

Use standard abootimg commands to update kernel
(Image.gz-dtb) in boot.img and fastboot flash the
updated boot.img.

My mainline tracking kernel is hosted at -->
```
https://github.com/pundiramit/linux/tree/master (beryllium_defconfig).
```

Prepare bootable kernel image (Image.gz-dtb) by running -->
```
$ cat arch/arm64/boot/Image.gz arch/arm64/boot/dts/qcom/sdm845-beryllium.dtb > arch/arm64/boot/Image.gz-dtb
```

# Known Issues -->
* Can not shutdown the device from commandline. Not even
  from TWRP reboot menu. For now I flash LineageOS image
  from TWRP and then shutdown from TWRP reboot menu.
* "fastboot boot boot.img" doesn't work.

# ToDo -->
* Display and Touch panel
* Connectivity
* Energy Aware Scheduler
