/*
 * MotoCluster-LVGL - the same cluster, no Qt, straight onto /dev/fb0.
 *
 * WHY THIS EXISTS
 * ---------------
 * Measured on this board (AM335x, 1 GHz single-core Cortex-A8, no usable GPU), the Qt
 * build spends 1545 ms between exec and its first frame:
 *
 *     [PHASE] qguiapp          164 ms      platform plugin + linuxfb
 *     [PHASE] qml-loaded      1360 ms      <- engine.load() is 1196 ms of this
 *     [PHASE] first-frame     1545 ms
 *
 * plus ld.so mapping ~30 MB of Qt libraries before main() is even entered. The 1196 ms is
 * QML instantiation - pure CPU, untouchable by any bootloader or kernel change, and only
 * 279 ms better than a GPU board with 1.8 GHz A55s once the QML was cut down.
 *
 * This program is the control experiment: same simulation, same face, same phase marks,
 * no QML engine and no 30 MB of shared libraries. The difference between its KPI line and
 * the Qt one is the price of Qt on this SoC, measured rather than argued.
 *
 * ⚠️ IT IS A MEASUREMENT, NOT YET A DECISION. Do not delete the Qt build on the strength
 * of it. The imx95 sibling's whole story is "the same Qt app on two boards", and replacing
 * the app on one of them ends that comparison. Get the number first, then choose.
 *
 * PHASE MARKS ARE DELIBERATELY THE SAME SHAPE AS main.cpp's
 * --------------------------------------------------------
 *     qguiapp    -> lv-init     platform + display up, nothing drawn
 *     qml-loaded -> ui-built    the scene exists, not yet rendered
 *     first-frame                identical meaning: pixels are on the panel
 * so the two KPI files can be diffed line for line. Keep them aligned if either changes.
 *
 * NOTE ON "since POR": always 0.000 here, exactly as in main.cpp's non-aarch64 branch.
 * The Cortex-A8 has no EL0-readable architected counter, so a process cannot see the
 * ~2.4 s pre-kernel segment at all. kernel+userspace is the real number on this board;
 * add the pre-kernel figure by hand and say which milestone you mean.
 */
#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "lvgl.h"

#include "cluster.h"
#include "vehicle.h"

#define FBDEV_DEFAULT "/dev/fb0"
#define READY_FILE    "/run/hmi-ready"

static double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double g_t0;

#define PHASE(name) \
	fprintf(stderr, "[PHASE] %-14s %5d ms\n", (name), (int)((now_s() - g_t0) * 1000.0))

/*
 * lv-conf.inc wires LV_TICK_CUSTOM_SYS_TIME_EXPR to custom_tick_get(), so LVGL calls
 * THIS for its millisecond clock and nothing has to pump lv_tick_inc() from a thread or a
 * signal. Defined unconditionally: if a future lv_conf.h stops using it the symbol is
 * simply unreferenced, which is cheaper than a build that breaks on an #if.
 */
uint32_t custom_tick_get(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

/*
 * Put the virtual terminal into GRAPHICS mode so the console stops drawing.
 *
 * ⚠️ WITHOUT THIS THE CLUSTER SHARES THE SCREEN WITH THE LOGIN PROMPT. fbcon
 * (CONFIG_FRAMEBUFFER_CONSOLE=y) owns tty1 and renders into the SAME /dev/fb0 this
 * program draws into. LVGL only re-flushes the regions it invalidated, so once getty
 * paints "beaglebone login:" over the top, nothing ever repaints it away and the text
 * sits on the cluster indefinitely.
 *
 * Qt's linuxfb plugin does exactly this internally, which is why the Qt build never
 * showed the problem and this one did - the bug was in what my program did NOT do.
 *
 * KD_GRAPHICS tells the VT layer that userspace owns the display, so fbcon stops
 * emitting. It does NOT disable fbcon's early modeset, which is deliberate: that modeset
 * at ~0.93 s of kernel time is what starts HDMI link training long before the app draws,
 * and losing it would push back the moment the panel can show anything.
 *
 * Best-effort: if /dev/tty0 is missing (no VT at all) the cluster still runs, it just
 * shares the framebuffer.
 */
static void vt_graphics_mode(void)
{
	int fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fd = open("/dev/console", O_RDWR | O_CLOEXEC);
		if (fd < 0)
			return;
	}
	if (ioctl(fd, KDSETMODE, KD_GRAPHICS) < 0)
		fprintf(stderr, "motocluster-lvgl: KDSETMODE failed (console may "
				"draw over the cluster)\n");
	close(fd);
}

/*
 * Stop fbcon drawing on our framebuffer, WITHOUT touching the display pipeline.
 *
 * The problem: ui-init forks this program and THEN execs /sbin/init, so init's
 * "Please wait: booting..." banner and fbcon's blinking cursor are painted AFTER our
 * first frame. A full-screen background cannot help - the text arrives later - and LVGL
 * only re-flushes what it invalidated, so the cursor keeps being redrawn forever.
 *
 * ⚠️ NOT KD_GRAPHICS. That was tried and it BLANKS THE PANEL on this board: the VT
 * graphics-mode path DPMS-es the CRTC off through drm_fb_helper, so the framebuffer keeps
 * its pixels and the display receives nothing. See vt_graphics_mode() above, kept as a
 * warning.
 *
 * Unbinding vtcon instead only detaches the CONSOLE TEXT RENDERER. Verified live on the
 * board - dmesg prints "Console: switching to colour dummy device 80x30" and the CRTC,
 * the mode and our pixels are all untouched.
 *
 * ✅ AND IT KEEPS THE HDMI WARM-UP. fbcon's modeset at ~0.83 s of kernel time is what
 * starts HDMI link training long before this program draws; that has already happened by
 * the time we unbind, and unbinding does not undo the mode. So we get the early link
 * training AND a clean screen.
 *
 * Best-effort: any failure here is cosmetic, so it is never fatal.
 */
static void fbcon_detach(void)
{
	for (int i = 0; i < 4; i++) {
		char path[64];
		snprintf(path, sizeof path,
			 "/sys/class/vtconsole/vtcon%d/bind", i);
		int fd = open(path, O_WRONLY | O_CLOEXEC);
		if (fd < 0)
			continue;
		/* Writing 0 to a vtcon that is not fbcon is harmless - it is either
		 * already 0 or it is the dummy console, which has nothing to draw. */
		if (write(fd, "0\n", 2) < 0)
			; /* cosmetic only */
		close(fd);
	}
}

int main(void)
{
	vehicle_t veh;
	const char *fbdev = getenv("MOTOCLUSTER_FB");

	g_t0 = now_s();
	setvbuf(stderr, NULL, _IOLBF, 0);

	if (!fbdev)
		fbdev = FBDEV_DEFAULT;

	/* ❌ DISABLED - vt_graphics_mode() BLANKS THE DISPLAY on this board.
	 *
	 * KD_GRAPHICS makes the VT layer hand the display to userspace, and on tilcdc's
	 * DRM fbdev emulation that path DPMS-es the CRTC off: the framebuffer keeps its
	 * pixels (verified - /dev/fb0 read back non-zero and kept changing) while the
	 * panel receives nothing. Symptom is a totally dark screen with DPMS reporting
	 * "On", which is as misleading as it sounds.
	 *
	 * The console bleed it was meant to fix is solved instead by removing the tty1
	 * getty from the image, so nothing ever draws on the framebuffer console. That
	 * fixes the cause rather than fighting the display pipeline.
	 *
	 * Left compiled (see vt_graphics_mode above) but NOT CALLED, so the reasoning
	 * survives with the code. Do not re-enable without re-testing on the panel.
	 */
	/* vt_graphics_mode(); */

	lv_init();

	lv_display_t *disp = lv_linux_fbdev_create();
	if (!disp) {
		fprintf(stderr, "motocluster-lvgl: lv_linux_fbdev_create failed\n");
		return 1;
	}
	lv_linux_fbdev_set_file(disp, fbdev);

	PHASE("lv-init");            /* == qguiapp: display up, nothing drawn yet */

	vehicle_init(&veh);
	cluster_build();
	cluster_update(&veh, 0.0, false);

	PHASE("ui-built");           /* == qml-loaded: scene exists, not yet rendered */

	/*
	 * Force the first refresh synchronously rather than waiting for the timer loop to
	 * get round to it. lv_refr_now() returns after the fbdev flush has copied into the
	 * mmap'd framebuffer, so when it returns the pixels really are on the scanout
	 * buffer - which is the same contract as Qt's frameSwapped, and the reason the two
	 * first-frame marks are comparable at all.
	 */
	lv_refr_now(disp);

	PHASE("first-frame");

	/* AFTER the first frame, so the unbind can never delay the pixels reaching the
	 * panel - it only has to beat init's banner, which is still seconds away. */
	fbcon_detach();

	/* The first-frame contract. boot-defer orders the deferred services against
	 * /run/hmi-ready, and firstframe writes the same file - so whichever draws the
	 * first frame, the rest of the boot sees exactly one signal. */
	int rfd = open(READY_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (rfd >= 0)
		close(rfd);

	fprintf(stderr,
		"[KPI] First frame is shown at %d ms after exec | %.3f s since POR "
		"(pre-kernel %.3f s, kernel+userspace %.3f s)\n",
		(int)((now_s() - g_t0) * 1000.0), 0.0, -now_s(), now_s());

	/*
	 * Ignition sweep, then live data - same 1.6 s ramp as the QML's `intro`.
	 * 30 Hz, matching VehicleModel's QTimer(33): the eye cannot tell on a needle
	 * sweep, and on this CPU every wakeup is time the cluster is not idle in.
	 */
	const double t_start = now_s();
	for (;;) {
		double el = now_s() - t_start;
		bool live = el > 1.6;
		double intro = live ? 0.0 : (el < 0.8 ? el / 0.8 : (1.6 - el) / 0.8);

		if (live)
			vehicle_tick(&veh);
		cluster_update(&veh, intro, live);

		uint32_t wait = lv_timer_handler();
		if (wait == LV_NO_TIMER_READY || wait > 33)
			wait = 33;
		usleep(wait * 1000);
	}

	return 0;
}
