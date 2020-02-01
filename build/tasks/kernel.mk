ifneq ($(filter aosp_beryllium, $(TARGET_DEVICE)),)

IMAGE_GZ := device/linaro/dragonboard-kernel/android-5.4/Image.gz
DTB := device/xiaomi/beryllium/prebuilt-kernel/android-5.4/sdm845-beryllium.dtb

$(PRODUCT_OUT)/kernel: $(IMAGE_GZ) $(DTB)
	cat $(IMAGE_GZ) $(DTB) > $@

droidcore: $(PRODUCT_OUT)/kernel

endif
