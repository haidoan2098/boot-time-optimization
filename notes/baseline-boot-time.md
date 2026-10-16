# Thời gian boot BeagleBone Black: đến UI infotainment

## Baseline: cấu hình mặc định
Chưa tối ưu, **trung bình 3 lần đo**. Log: `baseline-run01..03.grabserial.log`, `baseline-run01..03.board.log`. Mốc 0 = dòng `U-Boot SPL` trên serial (chưa tính ROM code).

### Tổng quan: bật nguồn đến UI mất bao lâu?
**Khoảng 26,5 giây** từ SPL đến khi UI chính hiện xong (T2).

| Giai đoạn | Thời gian | Tỷ lệ |
|---|---|---|
| 1. Bootloader (SPL, U-Boot) | 4,16 s | 16% |
| 2. Giải nén (`Starting kernel` → dòng kernel đầu) | 4,44 s | 17% |
| 3. Kernel (đến `Run /sbin/init`) | 4,99 s | 19% |
| 4. Userspace (init → first-frame) | 12,93 s | 49% |
| **Tổng đến T2** | **≈ 26,5 s** | |

- **T1** (hình đầu tiên, `fbcon` lên HDMI): ≈ 13,2 s (chỉ là chữ console của kernel, chưa phải GUI).
- **T2** (UI chính xong): ≈ 26,5 s, dao động 26,36 đến 26,60 s.
- Chưa tính ROM code chạy trước SPL, vì nó không in gì ra serial.

### Sơ đồ chuỗi boot
Màu đỏ: hai khoản lớn nhất.

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'fontFamily': '-apple-system, BlinkMacSystemFont, Segoe UI, Roboto, monospace',
    'fontSize': '12px',
    'titleColor': '#0969da',
    'gridColor': '#d0d7de',
    'lineColor': '#0969da',
    'sectionBkgColor': '#f6f8fa',
    'altSectionBkgColor': '#ffffff',
    'sectionBkgColor2': '#f6f8fa',
    'doneTaskBkgColor': '#2da44e',
    'doneTaskBorderColor': '#1a7f37',
    'doneTaskTextColor': '#ffffff',
    'critBkgColor': '#cf222e',
    'critBorderColor': '#a40e26',
    'critTextColor': '#ffffff',
    'activeTaskBkgColor': '#0969da',
    'activeTaskBorderColor': '#0550ae',
    'activeTaskTextColor': '#ffffff',
    'milestoneBkgColor': '#d4a72c',
    'milestoneBorderColor': '#9a6700'
  }
}}%%
gantt
    title Boot Chain Baseline (0.00s ➔ 26.52s)
    dateFormat x
    axisFormat %S s
    tickInterval 2second

    section 1. Bootloader (4.16s)
    SPL nạp U-Boot [0.00s → 0.78s] (0.78s)            :done, a1, 0, 778
    U-Boot init [0.78s → 1.45s] (0.67s)               :done, a2, 778, 1446
    Autoboot delay [1.45s → 3.52s] (2.08s)            :crit, a3, 1446, 3522
    Read extlinux, DTB, kernel [3.52s → 4.16s] (0.64s):done, a4, 3522, 4158

    section 2. Decompress (4.44s)
    zImage decompress [4.16s → 8.60s] (4.44s)         :crit, a5, 4158, 8600

    section 3. Kernel (4.99s)
    Kernel to /sbin/init [8.60s → 13.59s] (4.99s)     :active, a6, 8600, 13590
    T1 fbcon ready @ 13.19s                           :milestone, 13190, 13190

    section 4. Userspace (12.93s)
    Init to first-frame [13.59s → 26.52s] (12.93s)    :active, a7, 13590, 26520
    T2 first-frame @ 26.52s                           :milestone, 26520, 26520
```

### Mốc trên log
Dòng kernel/journal: `t_SPL` = timestamp kernel `[ x ]` + 8,60.

| t_SPL (s) | Dòng log | Nguồn | Δ (s) | Ý nghĩa |
|---|---|---|---|---|
| 0,00 | `U-Boot SPL 2025.10-ti-...` | serial | | SPL bắt đầu |
| 0,78 | `U-Boot 2025.10-ti-...` | serial | 0,78 | SPL nạp xong U-Boot |
| 1,45 | `Hit any key to stop autoboot:  2` | serial | 0,67 | U-Boot khởi tạo xong (riêng `MMC:` ≈ 0,40) |
| 3,52 | `switch to partitions #0, OK` | serial | 2,08 | hết đếm ngược autoboot |
| 4,16 | `Starting kernel ...` | serial | 0,64 | đọc xong extlinux, zImage (≈ 0,55), DTB |
| 8,60 | `[    0.000000] Booting Linux on physical CPU 0x0` | serial | 4,44 | dòng kernel đầu tiên |
| 13,19 | `[    4.59] Console: switching to colour frame buffer device 160x64` | serial | 4,59 | **T1**: fbcon lên HDMI |
| 13,59 | `[    4.99] Run /sbin/init as init process` | serial | 0,40 | kernel giao cho init |
| 25,67 | `[   17.07] systemd[1]: Started MotoCluster LVGL ...` | journal | 12,08 | systemd start app |
| 26,52 | `[   17.92] motocluster-lvgl: [PHASE] first-frame ...` | journal | 0,85 | **T2**: UI vẽ xong |
| 31,21 | `beaglebone login:` | serial | 4,69 | dấu nhắc đăng nhập (ngoài mục tiêu) |

### Chi tiết từng giai đoạn

#### Trước kernel: 8,60 s (đo trực tiếp bằng `grabserial`)
| Bước | Thời gian |
|---|---|
| SPL nạp U-Boot | 0,78 s |
| U-Boot khởi tạo | 0,67 s |
| Đếm ngược autoboot (chờ thuần túy) | 2,08 s |
| Đọc `extlinux.conf`, `zImage`, DTB | 0,64 s |
| `Starting kernel` đến dòng kernel đầu tiên (không in gì) | 4,44 s |

#### Kernel: 5,33 s
| Mốc (thời gian kernel) | |
|---|---|
| `fbcon` lên HDMI | 4,6 s |
| Mount root xong | 5,2 s |
| Giao cho init | 5,3 s |

Có một khoảng trống 1,6 đến 1,8 s không có log, chưa rõ nguyên nhân (xem 5.7).

#### Userspace: 12,59 s (từ lúc kernel giao cho init đến first-frame)
Mốc chính của systemd (tính từ lúc systemd bắt đầu, trung bình 3 lần):

| Mốc | Thời điểm |
|---|---|
| `local-fs.target` xong | 6,0 s |
| `systemd-resolved` bắt đầu (mất 3,6 s) | 7,2 s |
| `basic.target` | 11,0 s |
| `systemd-logind` bắt đầu (mất 1,65 s) | 12,4 s |
| `multi-user.target` | 14,4 s |

App LVGL: systemd ghi `Started` ở kernel 17,07 s, app thật sự chạy sau đó ~0,57 s, và first-frame ở
kernel 17,92 s (app tự đo 274 ms sau khi chạy).

### Bên trong app
| Chỉ số | Lần 1 | Lần 2 | Lần 3 | Trung bình |
|---|---|---|---|---|
| `Started` (journal, kernel s) | 17,142 | 16,997 | 17,069 | 17,07 |
| `lv-init` (ms sau exec) | 33 | 36 | 23 | 31 |
| `ui-built` (ms) | 98 | 97 | 84 | 93 |
| `first-frame` (ms) | 293 | 296 | 234 | 274 |
| Từ `Started` đến exec, s (suy ra) | 0,55 | 0,71 | 0,45 | 0,57 |

### Bên trong systemd (`critical-chain`, mốc từ lúc systemd bắt đầu, trung bình 3 lần)
| Unit | Bắt đầu @ (s) | Mất (s) |
|---|---|---|
| `--.slice` / `--.mount` | 1,66 | |
| `systemd-journald.socket` | 2,01 | |
| `tmpfiles-setup-dev-early` | 3,30 | 1,06 |
| `tmpfiles-setup-dev` | 4,90 | 0,60 |
| `var-volatile.mount` | 5,57 | 0,31 |
| `local-fs.target` | 6,03 | |
| `tmpfiles-setup` | 6,14 | 0,95 |
| `systemd-resolved` | 7,20 | **3,63** |
| `sysinit.target` | 10,88 | |
| `basic.target` | 10,95 | |
| `systemd-logind` | 12,40 | 1,65 |
| `multi-user.target` | 14,38 | |

Ghi chú: `systemd-resolved` ổn định 3,59 đến 3,67 s qua 3 lần. Ở các boot trước (lần đầu tiên) chuỗi còn có
`systemd-sysusers` (+1,53 s) và `resolved` mất 2,84 s; chưa rõ vì sao chuỗi khác đi. Phụ thuộc của app
(`systemctl show motocluster-lvgl -p After`): `local-fs.target`, `systemd-journald.socket`, `basic.target`,
`sysinit.target`. Chỉ `local-fs.target` do file `.service` khai báo; phần còn lại do `DefaultDependencies=yes` thêm ngầm.

### systemd `blame` (từ lần boot đầu, không phải đường găng)
| Unit | Thời gian |
|---|---|
| `dev-mmcblk0p2.device` | 13,6 s (thời gian chờ udev, không chặn gì) |
| `systemd-udev-trigger.service` | 6,5 s |
| `systemd-resolved.service` | 2,8 s |
| `systemd-timesyncd.service` | 2,5 s |
| `systemd-logind.service` | 1,96 s |
| `systemd-networkd.service` | 1,88 s |

Nhiều unit rất đơn giản (mount, modprobe) cũng mất 1,5 đến 1,7 s; nghi do tranh một lõi CPU (chưa kiểm chứng).

## Tối ưu 1: app chạy ngay khi systemd sẵn sàng
Cấu hình rootfs (unit systemd), **1 lần đo**. Log: `opt1-run01.grabserial.log`. Chỉ đổi file unit của app, phần còn lại giữ nguyên như baseline.

### Quan sát
Ở baseline, systemd đã chạy từ sớm (`Run /sbin/init` @ 13,59 s) nhưng app đến 25,67 s mới được `Started`,
tức là **chờ ≈ 12 s** dù app chỉ cần `/dev/fb0` (kernel tạo sẵn @ 13,19 s) và rootfs.
`systemd-analyze critical-chain` cho thấy app đứng sau `basic.target`, mà `basic.target` lại đứng sau
`sysinit.target` và `systemd-resolved` (≈ 3,6 s).

### Nguyên nhân
- File unit chỉ khai báo `After=local-fs.target`, nhưng `systemctl show motocluster-lvgl -p After` còn có
  `sysinit.target`, `basic.target`, `systemd-journald.socket`.
- Phần thêm ngầm đến từ `DefaultDependencies=yes` (mặc định của mọi service, xem `man systemd.service`):
  systemd tự gắn `After=sysinit.target basic.target` để service thường chỉ chạy khi hệ thống đã khởi tạo xong.
- App không dùng mạng, DNS, DBus, udev; nó không cần chờ những thứ đó.

### Thay đổi
Đổi đúng hai dòng trong `motocluster-lvgl.service`:

```diff
 [Unit]
 Description=MotoCluster LVGL (fixed UI for boot time study)
-After=local-fs.target
+DefaultDependencies=no
+RequiresMountsFor=/usr/bin
```

- `DefaultDependencies=no`: bỏ các ràng buộc ngầm, app không còn chờ `basic.target`.
- `RequiresMountsFor=/usr/bin`: chỉ chờ đúng mount chứa binary của app.
- `/dev/fb0` được đảm bảo bởi thứ tự boot (driver built-in, có trước init), không phải bởi unit.

### Kết quả
| Giai đoạn | Baseline | Tối ưu 1 | Chênh |
|---|---|---|---|
| 1. Bootloader | 4,16 s | 4,15 s | 0 |
| 2. Giải nén | 4,44 s | 4,44 s | 0 |
| 3. Kernel | 4,99 s | 5,05 s | +0,06 (nhiễu, kernel không đổi) |
| 4. Userspace (init → first-frame) | 12,93 s | ≈ 3,44 s | **−9,5** |
| T1 (fbcon) | 13,19 s | 13,19 s | 0 |
| **T2** | **26,52 s** | **≈ 17,08 s** | **−9,4 s (−35%)** |

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'fontFamily': '-apple-system, BlinkMacSystemFont, Segoe UI, Roboto, monospace',
    'fontSize': '12px',
    'titleColor': '#0969da',
    'gridColor': '#d0d7de',
    'lineColor': '#0969da',
    'sectionBkgColor': '#f6f8fa',
    'altSectionBkgColor': '#ffffff',
    'sectionBkgColor2': '#f6f8fa',
    'doneTaskBkgColor': '#2da44e',
    'doneTaskBorderColor': '#1a7f37',
    'doneTaskTextColor': '#ffffff',
    'critBkgColor': '#cf222e',
    'critBorderColor': '#a40e26',
    'critTextColor': '#ffffff',
    'activeTaskBkgColor': '#0969da',
    'activeTaskBorderColor': '#0550ae',
    'activeTaskTextColor': '#ffffff',
    'milestoneBkgColor': '#d4a72c',
    'milestoneBorderColor': '#9a6700'
  }
}}%%
gantt
    title Boot Chain Optimization 1 (0.00s ➔ 17.08s)
    dateFormat x
    axisFormat %S s
    tickInterval 2second

    section 1. Bootloader (4.15s)
    SPL nạp U-Boot [0.00s → 0.78s] (0.78s)            :done, b1, 0, 775
    U-Boot init [0.78s → 1.44s] (0.67s)               :done, b2, 775, 1443
    Autoboot delay [1.44s → 3.52s] (2.08s)            :crit, b3, 1443, 3519
    Read extlinux, DTB, kernel [3.52s → 4.15s] (0.63s):done, b4, 3519, 4153

    section 2. Decompress (4.44s)
    zImage decompress [4.15s → 8.60s] (4.44s)         :crit, b5, 4153, 8596

    section 3. Kernel (5.05s)
    Kernel to /sbin/init [8.60s → 13.64s] (5.05s)     :active, b6, 8596, 13644
    T1 fbcon ready @ 13.19s                           :milestone, 13191, 13191

    section 4. Userspace (3.44s)
    Init to first-frame [13.64s → 17.08s] (3.44s)     :active, b7, 13644, 17080
    T2 first-frame @ 17.08s                           :milestone, 17080, 17080
```

## Tối ưu 2: cắt kernel config, tắt bootdelay, tắt node DT không dùng
Cấu hình kernel, U-Boot và device tree. Ba bước nhỏ, mỗi bước đổi đúng một thứ, đo riêng.

### Tối ưu 2a: tắt driver phần cứng không có trên BBB
**1 lần đo.** Log: `opt2a-run01.grabserial.log`; dữ liệu initcall: `diag-initcall01.initcall.log`.

**Quan sát.** `initcall_debug` (`diag-initcall01.initcall.log`) cho thấy kernel bb.org là config đa board: khởi tạo cả
driver cho SoC và PMIC khác. Mọi thứ built-in (`=y`) đều chạy initcall lúc boot, kể cả khi không có thiết bị.

**Cách chọn "chắc chắn không dùng".** Đối chiếu với device tree BBB dùng (`am335x-boneblack*.dts*`,
`am33xx*.dtsi`, `am335x-bone-common.dtsi`): driver không có node nào thì không bao giờ probe.
Không cắt theo tên: `INPUT_TPS65218_PWRBUTTON` mang tên TPS65218 nhưng phục vụ cả nút nguồn TPS65217 của BBB, giữ.

**Thay đổi.** Fragment `disable-non-bbb-hw.cfg` (15 option), nạp qua `KERNEL_CONFIG_FRAGMENTS` trong
`linux-bb.org_%.bbappend` (recipe meta-ti không tự trộn `.cfg` từ `SRC_URI` như `linux-yocto`):

| Nhóm | Option tắt |
|---|---|
| SoC khác | `SOC_OMAP5`, `SOC_DRA7XX` |
| PMIC khác | `MFD/REGULATOR_PALMAS`, `_TPS65218`, `_TPS65219`, `MFD_TPS6594(_I2C/_SPI)`, `REGULATOR_TPS6594` |
| Khối chỉ có trên OMAP4/5/DRA7 | `MFD_OMAP_USB_HOST`, `REGULATOR_TI_ABB`, `REGULATOR_PBIAS` |

**Kết quả** (so với Tối ưu 1):

| Chỉ số | Tối ưu 1 | Tối ưu 2a | Chênh |
|---|---|---|---|
| Kích thước zImage | 8 466 944 B | 8 389 120 B | −76 KiB (−0,9%) |
| 2. Giải nén | 4,44 s | 4,39 s | −0,06 |
| 3. Kernel | 5,05 s | 4,97 s | −0,08 |
| T1 (fbcon) | 13,19 s | 13,11 s | −0,08 |
| **T2** | ≈ 17,08 s | **≈ 16,96 s** | −0,12 (dưới ngưỡng nhiễu 0,25 s) |

Khối Giải nén giảm gần đúng tỷ lệ với kích thước zImage: dấu hiệu đầu tiên ủng hộ giả thuyết
4,4 s là thời gian giải nén.

### Tối ưu 2b: tắt đếm ngược autoboot của U-Boot
**1 lần đo.** Log: `opt2b-run01.grabserial.log`.

**Quan sát.** `Hit any key to stop autoboot: 2` → `switch to partitions` mất 2,08 s, lần nào cũng như nhau:
chờ thuần túy.

**Nguyên nhân.** U-Boot không đọc được `uboot.env` nên dùng env mặc định, trong đó `bootdelay` lấy từ
`CONFIG_BOOTDELAY=2` lúc build.

**Thay đổi.** Fragment `bootdelay.cfg` (`CONFIG_BOOTDELAY=0`) trong `u-boot-bb.org_%.bbappend`; với U-Boot,
`u-boot-configure.inc` của poky tự trộn mọi `.cfg` trong `SRC_URI`. Giá trị `0` vẫn kiểm tra phím một lần,
giữ được đường vào prompt khi cần debug.

**Kết quả** (so với Tối ưu 2a):

| Chỉ số | Tối ưu 2a | Tối ưu 2b | Chênh |
|---|---|---|---|
| Autoboot | 2,08 s | 0,07 s | −2,01 |
| 1. Bootloader | 4,15 s | 2,13 s | −2,02 |
| T1 (fbcon) | 13,11 s | 11,09 s | −2,02 |
| **T2** | ≈ 16,96 s | **≈ 14,95 s** | **−2,01** |

### Tối ưu 2c: thu nhỏ kernel và tắt node DT không dùng
**1 lần đo.** Log: `opt2c-run01.grabserial.log`. Bước này gộp ba thay đổi trong một lần build.

**Nguyên tắc chọn.** Không chỉ có bật/tắt: tính năng sản phẩm có thể cần sau này (network, USB) giữ nguyên;
thứ không cần lúc boot nhưng có thể dùng sau chuyển thành module (`=m`), ra khỏi đường boot; thứ phần cứng
không bao giờ có thì tắt (`n`). LSM và tracing là `bool` (không làm module được) nên giữ nguyên.

**Thay đổi.**

| Nơi | Thay đổi | Lý do |
|---|---|---|
| `kernel-trim.cfg` | `MTD` = n (kéo theo JFFS2, UBIFS) | BBB không có NAND/NOR (`NAND: 0 MiB`) |
| | `MD` = n | Không có kịch bản RAID |
| | `BTRFS_FS`, `F2FS_FS`, `NFS_FS` = m | Có thể dùng sau (USB/SD, mount từ host), không cần lúc boot |
| Patch `am335x-boneblack.dts` | `cape_eeprom0..3` disabled (giữ bus i2c2) | Không cắm cape; probe trả `-ENODEV` |
| | `mmc2` (eMMC) disabled | Boot và rootfs đều trên SD |

**Kết quả** (so với Tối ưu 2b):

| Chỉ số | Tối ưu 2b | Tối ưu 2c | Chênh |
|---|---|---|---|
| Kích thước zImage | 8 389 120 B | 6 996 480 B | −1,33 MiB (−16,6%) |
| 1. Bootloader | 2,13 s | 2,04 s | −0,09 (đọc zImage nhỏ hơn) |
| 2. Giải nén | 4,39 s | 3,70 s | −0,68 |
| 3. Kernel | 4,92 s | 2,80 s | −2,12 |
| T1 (fbcon) | 11,09 s | 8,27 s | −2,83 |
| **T2** | ≈ 14,95 s | **≈ 12,07 s** | **−2,89** |

- Giải nén giảm 15,5%, gần đúng tỷ lệ zImage nhỏ đi (16,6%), cùng xu hướng với Tối ưu 2a: khối này là giải nén
  kernel (XZ).
- Kernel giảm vượt dự đoán vì **khoảng trống ≈ 1,64 s trước `Initialise system trusted keyrings` biến mất**,
  dù `BLK_DEV_IO_TRACE` vẫn bật. Vậy `init_blk_tracer` chỉ là initcall rơi đúng lúc phải chờ, không phải nguyên
  nhân. Thay đổi nào trong ba nhóm gỡ được nó: chưa tách (cần bật lại từng nhóm để tìm).

### Tổng kết Tối ưu 2
| Giai đoạn | Baseline | Tối ưu 1 | Tối ưu 2 (2a + 2b + 2c) |
|---|---|---|---|
| 1. Bootloader | 4,16 s | 4,15 s | 2,04 s |
| 2. Giải nén | 4,44 s | 4,44 s | 3,70 s |
| 3. Kernel | 4,99 s | 5,05 s | 2,80 s |
| 4. Userspace (init → first-frame) | 12,93 s | ≈ 3,44 s | ≈ 3,52 s |
| T1 (fbcon) | 13,19 s | 13,19 s | 8,27 s |
| **T2** | **26,52 s** | **≈ 17,08 s** | **≈ 12,07 s (−14,45 s, −54% so với baseline)** |

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'fontFamily': '-apple-system, BlinkMacSystemFont, Segoe UI, Roboto, monospace',
    'fontSize': '12px',
    'titleColor': '#0969da',
    'gridColor': '#d0d7de',
    'lineColor': '#0969da',
    'sectionBkgColor': '#f6f8fa',
    'altSectionBkgColor': '#ffffff',
    'sectionBkgColor2': '#f6f8fa',
    'doneTaskBkgColor': '#2da44e',
    'doneTaskBorderColor': '#1a7f37',
    'doneTaskTextColor': '#ffffff',
    'critBkgColor': '#cf222e',
    'critBorderColor': '#a40e26',
    'critTextColor': '#ffffff',
    'activeTaskBkgColor': '#0969da',
    'activeTaskBorderColor': '#0550ae',
    'activeTaskTextColor': '#ffffff',
    'milestoneBkgColor': '#d4a72c',
    'milestoneBorderColor': '#9a6700'
  }
}}%%
gantt
    title Boot Chain Optimization 2 (0.00s ➔ 12.07s)
    dateFormat x
    axisFormat %S s
    tickInterval 2second

    section 1. Bootloader (2.04s)
    SPL nạp U-Boot [0.00s → 0.77s] (0.77s)            :done, c1, 0, 773
    U-Boot init [0.77s → 1.44s] (0.67s)               :done, c2, 773, 1441
    Autoboot delay [1.44s → 1.51s] (0.07s)            :done, c3, 1441, 1510
    Read extlinux, DTB, kernel [1.51s → 2.04s] (0.53s):done, c4, 1510, 2044

    section 2. Decompress (3.70s)
    zImage decompress [2.04s → 5.75s] (3.70s)         :crit, c5, 2044, 5748

    section 3. Kernel (2.80s)
    Kernel to /sbin/init [5.75s → 8.55s] (2.80s)      :active, c6, 5748, 8550
    T1 fbcon ready @ 8.27s                            :milestone, 8265, 8265

    section 4. Userspace (3.52s)
    Init to first-frame [8.55s → 12.07s] (3.52s)      :active, c7, 8550, 12068
    T2 first-frame @ 12.07s                           :milestone, 12068, 12068
```

Phân bổ T2 hiện tại: Giải nén 31%, Userspace 29%, Kernel 23%, Bootloader 17%. Mỗi bước mới 1 lần đo sạch.

## Tối ưu 3: kỹ thuật
Thay cơ chế bằng cái khác (khác Tối ưu 1–2 chỉ chỉnh, tắt, bỏ bớt).

### Tối ưu 3a: nén kernel bằng LZ4 thay XZ
**1 lần đo.** Log: `opt3a-run01.grabserial.log`.

**Quan sát.** Khối Giải nén (`Starting kernel` → `Booting Linux`) là khối lớn nhất của T2 sau Tối ưu 2 (3,70 s).
Kernel đang nén bằng XZ (`CONFIG_KERNEL_XZ=y` trong `arch/arm/configs/bb.org_defconfig`; mặc định của
`init/Kconfig` là GZIP). Bộ giải nén chạy trong `arch/arm/boot/compressed/` (`head.S` → `decompress_kernel()` →
`lib/decompress_unxz.c`), cache đã bật trước khi giải nén.

**Vì sao bb.org chọn XZ** [suy luận, không có ghi chép]: config dùng chung cho mọi board Beagle, ưu tiên file nhỏ
(phân vùng boot, tải về, cập nhật), không ưu tiên thời gian boot.

**Mô hình để chọn.** Chỉ tính những gì xảy ra lúc boot:

    thời gian = kích thước file nén ÷ tốc độ đọc SD (U-Boot) + thời gian giải nén trên board

**So sánh các thuật toán** (ARM32 hỗ trợ 5 loại; benchmark trên host i7-1260P, 1 luồng, cùng `Image` 24,18 MiB
của kernel Tối ưu 2c, cùng tham số nén như `scripts/Makefile.lib`; giá trị tuyệt đối khác board, chỉ dùng để so sánh):

| Thuật toán | Năm | Mục tiêu thiết kế | Kích thước (MiB) | So với GZIP | Giải nén (MB/s) |
|---|---|---|---|---|---|
| GZIP | 1992 | Cân bằng | 9,91 | 0% | 190 |
| LZO | 1996 | Tốc độ | 11,06 | +11,7% | 439 |
| LZMA | 1998 | Tỷ lệ nén | 7,20 | −27,4% | 72 |
| XZ | 2009 | Tỷ lệ nén | 6,61 | −33,3% | 75 |
| LZ4 | 2011 | Tốc độ giải nén | 11,88 | +19,9% | 2 130 |

**Yếu tố đưa vào và bỏ qua.**

| Yếu tố | Xét? | Lý do |
|---|---|---|
| Tốc độ giải nén | Có | Xảy ra mỗi lần boot, trên CPU yếu |
| Kích thước file | Có, gián tiếp | Chỉ qua thời gian đọc SD |
| Thời gian nén | Không | Chỉ xảy ra lúc build trên host |
| Dung lượng lưu trữ | Không | Thẻ SD và phân vùng boot còn dư nhiều |
| Kích thước gói cập nhật, RAM lúc giải nén | Không | Chưa có OTA; RAM đủ |
| ZSTD | Không | ARM32 không có `HAVE_KERNEL_ZSTD` |

**Quyết định: LZ4.** Trên board này CPU là nút thắt, không phải SD: LZ4 to hơn XZ khoảng 5 MB (đọc chậm hơn
một chút) nhưng giải nén nhanh hơn nhiều. LZ4 chỉ thua GZIP khi tốc độ đọc rất thấp, xa dưới SD hiện tại và
SPL ở chế độ Falcon, nên lựa chọn vẫn đứng nếu sau này đổi cách nạp kernel.

**Thay đổi.**
- `kernel-trim.cfg`: `# CONFIG_KERNEL_XZ is not set`, `CONFIG_KERNEL_LZ4=y`.
- `linux-bb.org_%.bbappend`: `DEPENDS += "lz4-native"`. Makefile kernel gọi `lz4`, không có trong HOSTTOOLS
  (chỉ có `lz4c`); `kernel.bbclass` chỉ tự thêm `lz4-native` khi *initramfs* nén LZ4 (`INITRAMFS_FSTYPES`), vì
  lựa chọn trong Kconfig chỉ biết được ở `do_configure`, sau khi bitbake đã tính `DEPENDS` lúc parse.

**Kết quả** (so với Tối ưu 2c):

| Chỉ số | Tối ưu 2c | Tối ưu 3a | Chênh |
|---|---|---|---|
| Kích thước zImage | 6 996 480 B | 12 517 888 B | +5,5 MB |
| 1. Bootloader (đọc zImage lâu hơn) | 2,04 s | 2,39 s | +0,35 |
| 2. Giải nén | 3,70 s | 0,53 s | −3,18 |
| 3. Kernel | 2,80 s | 2,79 s | 0 |
| 4. Userspace (init → first-frame) | ≈ 3,52 s | ≈ 3,48 s | 0 |
| T1 (fbcon) | 8,27 s | 5,43 s | −2,83 |
| **T2** | ≈ 12,07 s | **≈ 9,20 s** | **−2,87 (−65% so với baseline)** |

**Host khác board.** Bảng host chọn đúng thứ hạng nhưng không đúng độ lớn: LZ4 nhanh hơn XZ khoảng 28 lần trên
host, chỉ khoảng 7 lần trên board. [suy luận] XZ bị giới hạn bởi tính toán (tỷ lệ theo CPU); LZ4 chủ yếu sao chép
bộ nhớ, trên Cortex-A8 bị giới hạn bởi băng thông RAM và cache, nơi host vượt xa board.

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'fontFamily': '-apple-system, BlinkMacSystemFont, Segoe UI, Roboto, monospace',
    'fontSize': '12px',
    'titleColor': '#0969da',
    'gridColor': '#d0d7de',
    'lineColor': '#0969da',
    'sectionBkgColor': '#f6f8fa',
    'altSectionBkgColor': '#ffffff',
    'sectionBkgColor2': '#f6f8fa',
    'doneTaskBkgColor': '#2da44e',
    'doneTaskBorderColor': '#1a7f37',
    'doneTaskTextColor': '#ffffff',
    'critBkgColor': '#cf222e',
    'critBorderColor': '#a40e26',
    'critTextColor': '#ffffff',
    'activeTaskBkgColor': '#0969da',
    'activeTaskBorderColor': '#0550ae',
    'activeTaskTextColor': '#ffffff',
    'milestoneBkgColor': '#d4a72c',
    'milestoneBorderColor': '#9a6700'
  }
}}%%
gantt
    title Boot Chain Optimization 3a (0.00s ➔ 9.20s)
    dateFormat x
    axisFormat %S s
    tickInterval 1second

    section 1. Bootloader (2.39s)
    SPL nạp U-Boot [0.00s → 0.77s] (0.77s)            :done, d1, 0, 773
    U-Boot init [0.77s → 1.44s] (0.67s)               :done, d2, 773, 1441
    Autoboot delay [1.44s → 1.51s] (0.07s)            :done, d3, 1441, 1510
    Read extlinux, DTB, kernel [1.51s → 2.39s] (0.88s):crit, d4, 1510, 2393

    section 2. Decompress (0.53s)
    zImage decompress LZ4 [2.39s → 2.92s] (0.53s)     :done, d5, 2393, 2922

    section 3. Kernel (2.79s)
    Kernel to /sbin/init [2.92s → 5.72s] (2.79s)      :active, d6, 2922, 5716
    T1 fbcon ready @ 5.43s                            :milestone, 5431, 5431

    section 4. Userspace (3.48s)
    Init to first-frame [5.72s → 9.20s] (3.48s)       :active, d7, 5716, 9197
    T2 first-frame @ 9.20s                            :milestone, 9197, 9197
```

Phân bổ T2 hiện tại: Userspace 38%, Kernel 30%, Bootloader 26%, Giải nén 6%.

### Tối ưu 3b: Falcon mode (SPL nạp thẳng kernel, bỏ U-Boot proper)
**1 lần đo.** Log: `opt3b-run01.grabserial.log`; lần thử khi chưa có `args`: `diag-falcon-fallback01.grabserial.log`.

**Cơ chế.** Bình thường: ROM → SPL (SRAM nội, khởi tạo DDR) → U-Boot proper (driver đầy đủ, env, `extlinux.conf`,
sửa DTB lúc chạy) → kernel. Falcon: SPL tự nạp kernel và một DTB đã sửa sẵn ("args") rồi nhảy vào kernel; U-Boot
proper không chạy. SPL quá nhỏ để đọc `extlinux.conf` hay sửa DTB, nên vị trí file và cmdline phải cố định từ trước.
Tài liệu: `doc/develop/falcon.rst` trong source U-Boot (đọc đúng bản đang build; bản "latest" trên web có thêm
Secure Falcon dùng FIT, bản 2025.10 chưa có).

**Vì sao trước đây Falcon không chạy.** `am335x_evm_defconfig` đã bật sẵn `SPL_OS_BOOT`, nhưng
`spl_start_uboot()` (`board/ti/am335x/board.c`) chỉ vào Falcon khi env có `boot_os=yes`, mà SPL không có nơi lưu
env (`SPL_ENV_IS_NOWHERE`). Falcon có sẵn nhưng bị khoá.

**Thay đổi.**

| Nơi | Thay đổi | Lý do |
|---|---|---|
| `u-boot-bb.org/falcon.cfg` | `SPL_FS_LOAD_KERNEL_NAME="zImage"` | SPL nhận zImage trần qua `bootz_setup()` (cần `CMD_BOOTZ`), không cần uImage |
| | `SPL_ENV_SUPPORT`, `SPL_NET` = n (kéo theo `SPL_ETH`, `SPL_USB_ETHER`) | Bỏ điều kiện `boot_os`; các đường boot mạng/USB của SPL gọi API env |
| `linux-bb.org_%.bbappend` | `do_deploy` tạo `args` = DTB + `/chosen/bootargs` (`fdtput`, `dtc-native`) | Image tự đủ, không có bước tay trên board |
| `boot-opt-image.bb` | `IMAGE_BOOT_FILES` thêm `args` | SPL đọc `args` từ phân vùng boot (FAT) |

Cmdline trong `args` giống `extlinux.conf`, trừ `root=/dev/mmcblk0p2` thay `PARTUUID` (PARTUUID chỉ có khi wic tạo
thẻ, sau khi kernel đã build; eMMC đã tắt ở Tối ưu 2c nên thẻ SD luôn là `mmcblk0`). Giữ nguyên đọc qua FAT, cách
SPL đọc thẻ (PIO) và bố cục thẻ: chỉ thay đổi bắt buộc để Falcon chạy.

**Hướng đã thử và bỏ.** Tạo `args` bằng `spl export fdt` ở prompt U-Boot (cách trong tài liệu): lệnh này đi qua
`bootm`, không nhận zImage (`Wrong Image Type for bootm command`) rồi crash (`data abort`). Ngoài ra đây là bước làm
tay, không hợp với yêu cầu image build ra phải chạy ngay.

**Kết quả** (so với Tối ưu 3a). Không còn dòng `Starting kernel` nên khối 1 và 2 chỉ đo được gộp lại.

| Chỉ số | Tối ưu 3a | Tối ưu 3b | Chênh |
|---|---|---|---|
| 1 + 2. SPL → `Booting Linux` | 2,92 s | 1,88 s | −1,04 |
| 3. Kernel | 2,79 s | 2,92 s | +0,13 (Falcon không đụng kernel; nhiều khả năng nhiễu) |
| T1 (fbcon) | 5,43 s | 4,40 s | −1,04 |
| **T2** | ≈ 9,20 s | **≈ 8,20 s** | **−1,00 (−69% so với baseline)** |

Lần thử khi chưa có `args`: SPL báo `spl_load_image_fat_os: error reading image args, err - -2` rồi tự quay về
U-Boot proper và boot bình thường; chi phí của lần thử thất bại rất nhỏ.

**Đánh đổi và còn thiếu cho sản phẩm.**
- Cmdline của `args` đang viết cứng, tách khỏi `extlinux.conf`: đổi một nơi mà quên nơi kia thì không có tác dụng và
  không báo lỗi. Cần lấy từ biến chung và kiểm tra `args` lúc build.
- `root=` theo đường dẫn thiết bị: đúng khi chỉ có thẻ SD; phải xem lại nếu bật eMMC hoặc boot từ eMMC.
- Chưa kiểm chứng giả định "U-Boot chỉ thêm `bootargs` vào DTB" (so `/sys/firmware/fdt` với DTB gốc).
- Lối vào U-Boot chỉ còn phím `c` trên serial trong lúc SPL chạy; sản phẩm cần nút hoặc GPIO.
- Không kiểm tra chữ ký kernel/`args` (Secure Falcon cần U-Boot mới hơn).
- Bỏ U-Boot proper là bỏ luôn chỗ thường đặt logic cập nhật A/B và rollback; phải quyết định ở mức sản phẩm.

**Quan sát ngoài phạm vi đo.** Thỉnh thoảng ROM không đọc được MLO từ thẻ SD ở lần thử đầu, chuyển sang boot qua
UART (in `CCCC...`) rồi mới quay lại thẻ, mất thêm vài giây trước dòng SPL. Không ảnh hưởng T1/T2 (mốc 0 là SPL)
nhưng ảnh hưởng thời gian thật từ lúc cấp nguồn.

```mermaid
%%{init: {
  'theme': 'base',
  'themeVariables': {
    'fontFamily': '-apple-system, BlinkMacSystemFont, Segoe UI, Roboto, monospace',
    'fontSize': '12px',
    'titleColor': '#0969da',
    'gridColor': '#d0d7de',
    'lineColor': '#0969da',
    'sectionBkgColor': '#f6f8fa',
    'altSectionBkgColor': '#ffffff',
    'sectionBkgColor2': '#f6f8fa',
    'doneTaskBkgColor': '#2da44e',
    'doneTaskBorderColor': '#1a7f37',
    'doneTaskTextColor': '#ffffff',
    'critBkgColor': '#cf222e',
    'critBorderColor': '#a40e26',
    'critTextColor': '#ffffff',
    'activeTaskBkgColor': '#0969da',
    'activeTaskBorderColor': '#0550ae',
    'activeTaskTextColor': '#ffffff',
    'milestoneBkgColor': '#d4a72c',
    'milestoneBorderColor': '#9a6700'
  }
}}%%
gantt
    title Boot Chain Optimization 3b (0.00s ➔ 8.20s)
    dateFormat x
    axisFormat %S s
    tickInterval 1second

    section 1+2. SPL + Decompress (1.88s)
    SPL loads args, zImage; LZ4 decompress [0.00s → 1.88s] (1.88s) :done, e1, 0, 1879

    section 3. Kernel (2.92s)
    Kernel to /sbin/init [1.88s → 4.80s] (2.92s)      :active, e2, 1879, 4803
    T1 fbcon ready @ 4.40s                            :milestone, 4396, 4396

    section 4. Userspace (3.40s)
    Init to first-frame [4.80s → 8.20s] (3.40s)       :crit, e3, 4803, 8202
    T2 first-frame @ 8.20s                            :milestone, 8202, 8202
```

Phân bổ T2 hiện tại: Userspace 41%, Kernel 36%, SPL + Giải nén 23%.
