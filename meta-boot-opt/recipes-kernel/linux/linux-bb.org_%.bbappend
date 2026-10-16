FILESEXTRAPATHS:prepend := "${THISDIR}/linux-bb.org:"
SRC_URI += "file://kernel-trim.cfg"
KERNEL_CONFIG_FRAGMENTS += "${WORKDIR}/kernel-trim.cfg"
SRC_URI += "file://0001-ARM-dts-am335x-boneblack-disable-cape-EEPROMs-and-eMMC.patch"

# The kernel Makefile calls "lz4", which is not a Yocto HOSTTOOL.
DEPENDS += "lz4-native"

# Falcon mode: SPL hands the kernel a prepared device tree ("args") instead of
# U-Boot proper fixing one up at boot. Build it here from the deployed DTB so the
# image boots without any manual step.
#
# Under falcon extlinux.conf is never read, so the command line must come from the
# same variables that generate it; add kernel arguments to
# UBOOT_EXTLINUX_KERNEL_ARGS (layer.conf) and both boot paths get them.
# Two parts cannot be shared:
# - root=: extlinux uses PARTUUID, which only exists once wic writes the card.
#   With the eMMC disabled the SD card is always mmcblk0.
# - console=: UBOOT_EXTLINUX_CONSOLE is "console=${console},${baudrate}", expanded
#   by U-Boot at run time; FALCON_CONSOLE is what that expands to on this board.
FALCON_DTB ?= "am335x-boneblack.dtb"
FALCON_ROOT ?= "root=/dev/mmcblk0p2"
FALCON_CONSOLE ?= "console=ttyO0,115200n8,115200"
FALCON_BOOTARGS ?= "${FALCON_ROOT} ${UBOOT_EXTLINUX_KERNEL_ARGS} ${FALCON_CONSOLE}"
DEPENDS += "dtc-native"

do_deploy:append() {
    out=${DEPLOYDIR}/args
    cp -L ${DEPLOYDIR}/${FALCON_DTB} $out
    fdtput -p -t s $out /chosen bootargs "${FALCON_BOOTARGS}"

    # SPL passes this blob straight to the kernel: a bad one hangs with no output.
    got=$(fdtget -t s $out /chosen bootargs)
    [ "$got" = "${FALCON_BOOTARGS}" ] || bbfatal "falcon args: bootargs mismatch (got: $got)"
    fdtget $out /memory@80000000 reg >/dev/null || bbfatal "falcon args: no /memory@80000000 node"
}
