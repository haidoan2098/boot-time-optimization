SUMMARY = "Boot time study image: core-image-minimal + systemd + LVGL cluster UI"

require recipes-core/images/core-image-minimal.bb

IMAGE_INSTALL:append = " motocluster-lvgl systemd-analyze"


# Falcon mode: SPL loads "args" (prepared device tree, built by linux-bb.org)
# from the boot partition next to zImage.
IMAGE_BOOT_FILES:append = " args"
