# BeagleBone Black boot time optimization

Yocto layer that boots a BeagleBone Black (AM3358, single Cortex-A8) to a rendered
instrument cluster UI (LVGL on HDMI) in about **6.5 s from SPL**, down from 26.5 s with the
stock configuration. Each optimization is a separate commit, measured on hardware.

## Results

| | Stock | Optimized | Reduction |
|---|---|---|---|
| **Cluster UI on screen** | 26.5 s | **6.5 s** | **75%** |
| First picture on screen | 13.2 s | 3.1 s | 76% |

Measured from the `U-Boot SPL` banner to the first frame drawn by the UI application.
Per-stage timings and the effect of each change are in
[`notes/baseline-boot-time.md`](notes/baseline-boot-time.md).

What was changed:

1. UI service started without systemd's default dependencies.
2. Kernel config trimmed, unused device tree nodes disabled, U-Boot autoboot delay removed.
3. Kernel compressed with LZ4 instead of XZ; Falcon mode (SPL boots the kernel directly).
4. Kernel info messages kept off the serial console (`loglevel=4`).

## Platform

| | |
|---|---|
| Board | BeagleBone Black (AM3358, 512 MiB), boots from microSD |
| Display | HDMI via TDA19988, 1280x1024, 16 bpp |
| Build system | Yocto Project 5.0 (scarthgap), poky |
| Machine | `beaglebone` (meta-ti/meta-beagle) |
| Kernel | `linux-bb.org` 6.12.34-ti |
| Bootloader | `u-boot-bb.org` 2025.10 |
| Init | systemd |
| UI | LVGL, fbdev, software rendering |

## Repository layout

```
.
├── meta-boot-opt/                    Yocto layer
│   ├── conf/
│   │   └── layer.conf                Layer config; kernel arguments for both boot paths
│   ├── recipes-bsp/u-boot/
│   │   ├── u-boot-bb.org_%.bbappend
│   │   └── u-boot-bb.org/
│   │       ├── bootdelay.cfg         No autoboot countdown
│   │       └── falcon.cfg            SPL boots the kernel directly
│   ├── recipes-kernel/linux/
│   │   ├── linux-bb.org_%.bbappend   Fragment, DT patch, Falcon "args" generation
│   │   └── linux-bb.org/
│   │       ├── kernel-trim.cfg       Config trim, LZ4 compression
│   │       └── 0001-ARM-dts-...patch Disable cape EEPROMs and eMMC
│   ├── recipes-graphics/lvgl/
│   │   └── lvgl_%.bbappend           fbdev, RGB565, fonts
│   ├── recipes-hmi/motocluster-lvgl/
│   │   ├── motocluster-lvgl_1.0.bb   Cluster UI application
│   │   └── files/
│   │       └── motocluster-lvgl.service
│   └── recipes-core/images/
│       └── boot-opt-image.bb         core-image-minimal + UI
├── measurements/                     Serial and on-target logs per step
└── notes/
    └── baseline-boot-time.md         Analysis report
```

## Build

Layers (`bblayers.conf`):

```
poky/meta
poky/meta-poky
poky/meta-yocto-bsp
meta-arm/meta-arm
meta-arm/meta-arm-toolchain
meta-ti/meta-ti-bsp
meta-ti/meta-beagle
meta-openembedded/meta-oe
meta-boot-opt
```

`local.conf`:

```
MACHINE = "beaglebone"
INIT_MANAGER = "systemd"
```

Build and write the card:

```sh
bitbake boot-opt-image
bmaptool copy boot-opt-image-beaglebone.rootfs.wic.xz /dev/sdX
```

The image is self-contained: no manual step on the target is required.

## Boot flow

```
ROM -> SPL -> zImage (LZ4) + args -> Linux -> systemd -> motocluster-lvgl
```

SPL loads `zImage` and `args` from the FAT boot partition and jumps to the kernel; U-Boot
proper does not run. `args` is the board device tree with `/chosen/bootargs` filled in,
generated at build time and checked (bootargs, memory node) before deployment.

- **Kernel command line:** add arguments to `UBOOT_EXTLINUX_KERNEL_ARGS` in
  `meta-boot-opt/conf/layer.conf`. Both `args` and `extlinux.conf` are generated from it.
  `extlinux.conf` is only read when SPL falls back to U-Boot proper.
- **Root device:** `args` uses `root=/dev/mmcblk0p2`. The on-board eMMC is disabled in the
  device tree, so the microSD card is always `mmcblk0`.
- **U-Boot prompt:** press `c` on the serial console while SPL runs. SPL also falls back to
  U-Boot proper when `args` or the kernel is missing.

## Measurement

Capture on the host before a cold power-on:

```sh
grabserial -d /dev/ttyUSB0 -b 115200 -t -m "^U-Boot SPL" -q "beaglebone login:" -e 150 \
  -o measurements/<step>-runNN.grabserial.log
```

- `-m "^U-Boot SPL"` resets the timestamp at the SPL banner. Existing files are overwritten.
- With `loglevel=4`, kernel milestones no longer reach the serial console. Their timestamps
  are read from `dmesg` on the target and offset by the host time of `Booting Linux`, which
  is still printed.
- T2 is the framebuffer console handover (`Console: switching to colour dummy device 80x30`)
  minus 0.15 s, the measured gap between the application's first frame and that line.

Log files are named `<step>-runNN.<kind>.log`; `diag-*` files are diagnostic captures and
are not used for T1/T2.

## Known limitations

- Falcon mode bypasses U-Boot proper, which is where A/B slot selection, boot counting and
  rollback are usually implemented. Kernel and device tree are not signature-checked.
- The root device is selected by path, not by PARTUUID. Revisit if the eMMC is enabled or
  becomes the boot medium.
- The boot ROM occasionally fails its first attempt to read the microSD card and tries UART
  boot (`CCCC...` on the console), adding several seconds before SPL. This is outside T1/T2.
- systemd status messages are still printed on the serial console.
- Userspace is now the largest stage and has not been optimized beyond the UI service unit.
