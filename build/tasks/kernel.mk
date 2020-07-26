ifneq ($(filter beryllium, $(TARGET_DEVICE)),)

IMAGE_GZ := device/xiaomi/beryllium/prebuilt-kernel/android-$(TARGET_KERNEL_USE)/Image.gz
DTB := device/xiaomi/beryllium/prebuilt-kernel/android-$(TARGET_KERNEL_USE)/sdm845-beryllium.dtb

$(PRODUCT_OUT)/kernel: $(IMAGE_GZ) $(DTB)
	cat $(IMAGE_GZ) $(DTB) > $@

droidcore: $(PRODUCT_OUT)/kernel

endif
