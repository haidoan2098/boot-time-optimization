FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://disable-non-bbb-hw.cfg"
KERNEL_CONFIG_FRAGMENTS += "${WORKDIR}/disable-non-bbb-hw.cfg"
