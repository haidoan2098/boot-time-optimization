FILESEXTRAPATHS:prepend := "${THISDIR}/linux-bb.org:"
SRC_URI += "file://kernel-trim.cfg"
KERNEL_CONFIG_FRAGMENTS += "${WORKDIR}/kernel-trim.cfg"
SRC_URI += "file://0001-ARM-dts-am335x-boneblack-disable-cape-EEPROMs-and-eMMC.patch"
