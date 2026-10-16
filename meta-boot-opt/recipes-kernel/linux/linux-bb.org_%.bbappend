FILESEXTRAPATHS:prepend := "${THISDIR}/linux-bb.org:"
SRC_URI += "file://kernel-trim.cfg"
KERNEL_CONFIG_FRAGMENTS += "${WORKDIR}/kernel-trim.cfg"
