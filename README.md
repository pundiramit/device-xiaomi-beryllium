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

  NOTE: To get display working on PocoF1, we need supported Adreno
        firmware binaries, otherwise PocoF1 will not boot to UI.

        Adreno binaries are shipped with non-distributable license,
        hence I'm not shipping them in my build setup. You can
        extract Adreno a630_* firmware binaries from a working
        device build. I extracted mine from
        lineage-16.0-20190612-nightly-beryllium-signed.zip ;)

        Then copy the binaries to out vendor directory
        i.e. out/target/product/aosp_beryllium/vendor/firmware,
        and run "make -j$nproc" to create vendor.img again.

* Flash and boot AOSP images -->

```
fastboot flash system system.img
fastboot flash vendor vendor.img
fastboot flash userdata userdata.img
fastboot flash boot boot.img
fastboot reboot
```

# Boot AOSP images.

* Display is still a WIP. Bootanimation doesn't work, we get a
  white splash screen instead. 30 seconds or so into boot (amid
  white screen), we need to enforce a suspend-resume by pressing
  the power button for the UI/home-screen to show up.
* Touchpanel is in TODO as well, so not much you can do from UX
  point of view.
  You can use input keyevents from adb to move around (been there, done that!)

# How to run custom kernels?

Use standard abootimg commands to update kernel
(Image.gz-dtb) in boot.img and fastboot flash the
updated boot.img.

My working kernel is hosted at -->
```
https://github.com/pundiramit/linux/tree/display (beryllium_defconfig).
```

Prepare bootable kernel image (Image.gz-dtb) by running -->
```
$ cat arch/arm64/boot/Image.gz arch/arm64/boot/dts/qcom/sdm845-beryllium.dtb > arch/arm64/boot/Image.gz-dtb
```

# Known Issues -->
* Can not shutdown the device from commandline. So reboot
  into TWRP recovery and shutdown PocoF1 from there instead.

# ToDo -->
* Fix Boot Animation
* Enable Touch panel
* Connectivity
* Energy Aware Scheduler
