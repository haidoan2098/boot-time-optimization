SUMMARY = "Boot time study image: core-image-minimal + systemd + LVGL cluster UI"

require recipes-core/images/core-image-minimal.bb

IMAGE_INSTALL:append = " motocluster-lvgl systemd-analyze"

