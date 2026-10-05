SUMMARY = "MotoCluster LVGL, the fixed UI for the boot time study"
DESCRIPTION = "Cluster face drawn with LVGL's software renderer straight to /dev/fb0. \
Source is vendored from meta-bbb-fastboot (MIT). Prints [PHASE]/[KPI] timing lines to stderr."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=adb120a1e2a2f6267925869e0fc5f369"

DEPENDS = "lvgl"

SRC_URI = " \
    file://MotoClusterLvgl \
    file://motocluster-lvgl.service \
"

# scarthgap: sources are unpacked into WORKDIR (UNPACKDIR arrives in styhead).
S = "${WORKDIR}/MotoClusterLvgl"

inherit cmake pkgconfig systemd

# liblvgl.so is linked dynamically (baseline), so it must be on the target.
RDEPENDS:${PN} += "lvgl"

SYSTEMD_SERVICE:${PN} = "motocluster-lvgl.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/motocluster-lvgl.service ${D}${systemd_system_unitdir}/
}
