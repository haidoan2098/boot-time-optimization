FILESEXTRAPATHS:prepend := "${THISDIR}/linux-bb.org:"
SRC_URI += "file://kernel-trim.cfg"
KERNEL_CONFIG_FRAGMENTS += "${WORKDIR}/kernel-trim.cfg"
SRC_URI += "file://0001-ARM-dts-am335x-boneblack-disable-cape-EEPROMs-and-eMMC.patch"

# The kernel Makefile calls "lz4", which is not a Yocto HOSTTOOL.
DEPENDS += "lz4-native"

# Falcon mode: SPL hands the kernel a prepared device tree ("args") instead of
# U-Boot proper fixing one up at boot. Build it here from the deployed DTB so the
# image boots without any manual step. Same command line as extlinux.conf except
# root=: PARTUUID is only known when wic writes the card, and with the eMMC
# disabled the SD card is always mmcblk0.
FALCON_DTB ?= "am335x-boneblack.dtb"
FALCON_BOOTARGS ?= "root=/dev/mmcblk0p2 rootwait rw earlycon console=ttyO0,115200n8,115200"
DEPENDS += "dtc-native"

do_deploy:append() {
    cp ${DEPLOYDIR}/${FALCON_DTB} ${DEPLOYDIR}/args
    fdtput -p -t s ${DEPLOYDIR}/args /chosen bootargs "${FALCON_BOOTARGS}"
}
