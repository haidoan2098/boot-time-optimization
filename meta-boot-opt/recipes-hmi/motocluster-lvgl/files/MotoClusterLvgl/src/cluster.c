/*
 * MotoCluster, LVGL edition - the same cluster face as qml/Main.qml, drawn with LVGL's
 * software renderer straight into /dev/fb0.
 *
 * WHAT IS FAITHFUL TO THE QML, AND WHY
 * ------------------------------------
 * The palette, the geometry ratios and the L-shaped torque scale are copied exactly. They
 * have to be: this program's only purpose is to price Qt on this SoC, and a comparison
 * between two different-looking clusters prices nothing. If it does not look like the
 * screenshot of the Qt one, the measurement is not answering the question that was asked.
 *
 *   TORQUE - an L: up the left side, round the corner, along the top.
 *   A quarter CIRCLE is not an L - a circular quarter is curved everywhere, so the bars
 *   fan continuously and it reads as a swoosh. The reference cluster is a rounded
 *   RECTANGLE CORNER: a long straight run up the left, a corner radius, a long straight
 *   run across the top, and it is the STRAIGHT RUNS that make it read as an L.
 * That paragraph is lifted from Main.qml because the mistake it describes is easy to
 * repeat when reimplementing from a screenshot.
 *
 * WHAT IS DELIBERATELY DIFFERENT
 * ------------------------------
 * 1. Geometry is computed ONCE. See cluster.h - this is the whole point of the port.
 * 2. No MultiEffect, no layer.enabled, no gradients on the segments. The Qt build already
 *    had to drop those (patch 0001) because Qt's software adaptation renders NOTHING for
 *    them - the items vanish rather than merely losing their shadow. LVGL's software
 *    renderer has the same constraint for the same reason, so the two are level here.
 * 3. Segment count follows the Qt build's patch 0002: 29, not the original 58. Keeping the
 *    counts equal is part of keeping the comparison honest.
 */

/*
 * ── 2026-08 RESTYLE ────────────────────────────────────────────────────────────
 * The face above is the ORIGINAL Qt-parity design and the comparison it describes
 * has already been made (Qt 1545 ms vs LVGL 86 ms to first frame). The layout is
 * now redrawn in the segmented-bar instrument idiom instead, which is what this
 * panel is actually for.
 *
 * ⚠️ DRAWN FROM CONVENTION, NOT COPIED. The reference design that prompted this is
 * a third-party project with NO LICENSE file (all rights reserved), and its face is
 * a 320x170 bitmap plus needle sprites. None of its code, art or fonts are used
 * here. What is reused is the vocabulary every production cluster shares - a swept
 * segmented bar, colour zones, telltale pills, a boxed digital readout, a small
 * segmented fuel gauge. All of it is drawn with LVGL primitives, which also means it
 * scales to any panel instead of needing art re-authored per resolution.
 *
 * THE SWEEP IS DIAGONAL-THEN-HORIZONTAL, not an L. A vertical leg plus a horizontal
 * leg reads as a bracket; a raked leg rolling into a horizontal run reads as a
 * gauge sweeping up to speed. That is the single biggest structural difference from
 * the design above, and it is why path_pt() was rewritten rather than retuned.
 */

#include "cluster.h"

#include <math.h>
#include <stdio.h>

#include "lvgl.h"

/* ---- palette ----------------------------------------------------------------
 * Near-black face, and the bar reads through ZONES rather than one continuous hue
 * ramp. A ramp looks smooth in a screenshot but is unreadable in motion: every bar
 * is a slightly different colour, so none of them means anything. Discrete bands
 * give the bar a state recognisable peripherally. */
#define C_BG     0x04060b
#define C_GREEN  0x22c55e
#define C_BLUE   0x3b82f6
#define C_WHITE  0xe9eff8
#define C_RED    0xef4444
#define C_CYAN   0x22d3ee
#define C_MINT   0x34e5b0
#define C_AMBER  0xffb020
#define C_ROSE   0xff2d55
#define C_MUTED  0x66748c
#define C_TRACK  0x171e2d
#define C_PANEL  0x0c121c

/* ⚠️ DESIGN REFERENCE IS THE PANEL, 1024x600 - not a scaled 1920x1080.
 * It used to be 1920x1080, giving min(1024/1920, 600/1080) = 0.533, so Montserrat 48
 * rendered at ~26 px and every gutter halved. That is why the old face read small and
 * sparse. Scaling is KEPT so another panel still works; the reference is just honest
 * about the display this is drawn for, so g_s == 1.0 here and the numbers below are
 * real pixels. */
#define DES_W 1024.0f
#define DES_H 600.0f
static float g_s;
static int   g_w, g_h;
#define S(v) ((int)((v) * g_s + 0.5f))

#define SEGS  29                    /* main bar */
#define FSEGS 12                    /* fuel gauge */

static lv_obj_t *g_seg[SEGS];
static lv_obj_t *g_fseg[FSEGS];
static lv_obj_t *g_speed, *g_speed_unit, *g_gear, *g_mode;
static lv_obj_t *g_trip, *g_odo, *g_range, *g_coolant, *g_fuel_bar;
static lv_obj_t *g_nav, *g_batt_bar;
static lv_obj_t *g_tt_left, *g_tt_right, *g_tt_beam, *g_tt_abs, *g_tt_oil;

static uint32_t g_seg_col[SEGS];
static uint32_t g_fseg_col[FSEGS];
static int      g_last_gear = -99;

static lv_obj_t *plain(lv_obj_t *parent)
{
	lv_obj_t *o = lv_obj_create(parent);
	lv_obj_remove_style_all(o);
	lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
	return o;
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t col)
{
	lv_obj_t *o = plain(parent);
	lv_obj_set_pos(o, x, y);
	lv_obj_set_size(o, w, h);
	lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
	lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
	return o;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, const char *txt,
		       uint32_t col, const lv_font_t *font)
{
	lv_obj_t *l = lv_label_create(parent);
	lv_obj_set_pos(l, x, y);
	lv_label_set_text(l, txt);
	lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
	lv_obj_set_style_text_font(l, font, 0);
	return l;
}

/* Centred label inside a fixed-width slot. The speed changes width constantly
 * (0 -> 108 -> 0 every ride loop) and a left-anchored readout visibly jitters -
 * the single most obvious "this is a demo" tell on a cluster. */
static lv_obj_t *clabel(lv_obj_t *parent, int x, int y, int w, const char *txt,
			uint32_t col, const lv_font_t *font)
{
	lv_obj_t *l = label(parent, x, y, txt, col, font);
	lv_obj_set_width(l, w);
	lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
	return l;
}

static lv_obj_t *panel(lv_obj_t *parent, int x, int y, int w, int h)
{
	lv_obj_t *o = box(parent, x, y, w, h, C_PANEL);
	lv_obj_set_style_radius(o, S(10), 0);
	lv_obj_set_style_border_width(o, 1, 0);
	lv_obj_set_style_border_color(o, lv_color_hex(C_TRACK), 0);
	lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
	return o;
}

/* A telltale as a FILLED PILL, not a bare word.
 * Text-only telltales differ lit/unlit by colour alone, so the row reads as a caption
 * strip rather than as indicators. A pill has a shape you catch peripherally, which is
 * the entire job. Returns the PILL; the glyph is child 0. */
static lv_obj_t *pill(lv_obj_t *parent, int x, int y, int w, int h, const char *sym)
{
	lv_obj_t *o = box(parent, x, y, w, h, C_TRACK);
	lv_obj_set_style_radius(o, S(7), 0);
	lv_obj_t *l = lv_label_create(o);
	lv_label_set_text(l, sym);
	lv_obj_set_style_text_color(l, lv_color_hex(C_MUTED), 0);
	lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
	lv_obj_center(l);
	return o;
}

/* ---- the swept path ---------------------------------------------------------
 * A raked leg, a corner arc, then a horizontal run. Walked by ARC LENGTH so the
 * segments are evenly spaced along the path rather than evenly spaced in angle.
 *
 * `rot` is the OUTWARD NORMAL in degrees - both the direction a segment grows and
 * the angle it is rotated to. On the raked leg that is (tangent - 90); on the arc it
 * is exactly the point's own angle about the arc centre, which is why the segments
 * fan correctly round the corner without a special case. */
struct pt { float x, y, rot; };

#define RAKE_DEG (-50.0f)           /* raked leg, up and to the right */

static float q_sx, q_sy, q_L1, q_R, q_L2;
static float q_cx, q_cy, q_bx, q_by, q_arc, q_total;

static void path_init(float sx, float sy, float L1, float R, float x_end)
{
	const float a1 = RAKE_DEG * (float)M_PI / 180.0f;
	q_sx = sx; q_sy = sy; q_L1 = L1; q_R = R;

	float ax = sx + L1 * cosf(a1);              /* end of the raked leg */
	float ay = sy + L1 * sinf(a1);

	/* Arc centre sits perpendicular to the leg, on the inside of the turn. */
	float na = a1 + (float)M_PI / 2.0f;
	q_cx = ax + R * cosf(na);
	q_cy = ay + R * sinf(na);

	q_bx = q_cx;                                /* leaving the arc heading east */
	q_by = q_cy - R;

	q_arc = R * (0.0f - a1);                    /* sweep from a1 up to horizontal */
	q_L2  = x_end - q_bx;
	if (q_L2 < 0.0f) q_L2 = 0.0f;
	q_total = q_L1 + q_arc + q_L2;
}

static struct pt path_pt(float f)
{
	struct pt r;
	const float a1 = RAKE_DEG * (float)M_PI / 180.0f;
	if (f < 0.0f) f = 0.0f;
	if (f > 1.0f) f = 1.0f;
	float d = f * q_total;

	if (d <= q_L1) {                            /* up the raked leg */
		r.x = q_sx + d * cosf(a1);
		r.y = q_sy + d * sinf(a1);
		r.rot = RAKE_DEG - 90.0f;
		return r;
	}
	d -= q_L1;
	if (d <= q_arc) {                           /* round the corner */
		float p = (a1 - (float)M_PI / 2.0f) + (d / q_R);
		r.x = q_cx + q_R * cosf(p);
		r.y = q_cy + q_R * sinf(p);
		r.rot = p * 180.0f / (float)M_PI;
		return r;
	}
	d -= q_arc;                                 /* along the horizontal run */
	r.x = q_bx + d;
	r.y = q_by;
	r.rot = -90.0f;
	return r;
}

/* Zone colour at fraction f. Boundaries are deliberately uneven: green is widest
 * because that is where the vehicle spends its time, and red is narrow so it stays
 * meaningful. */
static uint32_t zone(float f)
{
	if (f < 0.34f) return C_GREEN;
	if (f < 0.56f) return C_BLUE;
	if (f < 0.86f) return C_WHITE;
	return C_RED;
}

/* One parallelogram segment.
 * The skew is what separates a gauge from a progress bar: shearing gives the bar an
 * implied direction of travel, so it reads as sweeping rather than filling. Both the
 * rotation and the skew are STYLES, set once here - the renderer still sees one
 * opaque rectangle, so there is no per-frame cost. */
static lv_obj_t *segment(lv_obj_t *parent, struct pt p, int bw, int bh)
{
	lv_obj_t *o = plain(parent);
	lv_obj_set_size(o, bw, bh);
	lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
	lv_obj_set_style_bg_color(o, lv_color_hex(C_TRACK), 0);

	float rad = p.rot * (float)M_PI / 180.0f;
	lv_obj_set_pos(o, (int)(p.x + cosf(rad) * bw / 2.0f - bw / 2.0f),
			  (int)(p.y + sinf(rad) * bw / 2.0f - bh / 2.0f));

	lv_obj_set_style_transform_pivot_x(o, bw / 2, 0);
	lv_obj_set_style_transform_pivot_y(o, bh / 2, 0);
	lv_obj_set_style_transform_rotation(o, (int32_t)(p.rot * 10.0f), 0);
	lv_obj_set_style_transform_skew_x(o, -16 * 10, 0);
	return o;
}

void cluster_build(void)
{
	lv_obj_t *scr = lv_screen_active();
	lv_display_t *disp = lv_display_get_default();

	g_w = lv_display_get_horizontal_resolution(disp);
	g_h = lv_display_get_vertical_resolution(disp);

	float sw = (float)g_w / DES_W;
	float sh = (float)g_h / DES_H;
	g_s = sw < sh ? sw : sh;

	lv_obj_remove_style_all(scr);
	lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

	/* Explicit full-screen background on top of the screen's own style. Styling
	 * lv_screen_active() alone was not enough - whatever LVGL considered "the screen"
	 * did not cover the whole framebuffer, so anything drawn before us survived in a
	 * strip along the bottom. A real object sized to the framebuffer is unambiguous. */
	box(scr, 0, 0, g_w, g_h, C_BG);

	const int gutter = S(40);

	/* ---------------------------------------------------------- telltale row
	 * Top of the screen, where every production cluster puts warnings and where the
	 * eye goes for them. */
	const int tw = S(132), th = S(38), tgap = S(10);
	const int trow = tw * 5 + tgap * 4;
	int tx = (g_w - trow) / 2;
	const int ty = S(10);
	g_tt_left  = pill(scr, tx, ty, tw, th, "<<");   tx += tw + tgap;
	g_tt_beam  = pill(scr, tx, ty, tw, th, "BEAM"); tx += tw + tgap;
	g_tt_abs   = pill(scr, tx, ty, tw, th, "ABS");  tx += tw + tgap;
	g_tt_oil   = pill(scr, tx, ty, tw, th, "OIL");  tx += tw + tgap;
	g_tt_right = pill(scr, tx, ty, tw, th, ">>");

	/* ---------------------------------------------------------- title strip */
	label(scr, gutter, S(56), "MOTOCLUSTER", C_MUTED, &lv_font_montserrat_16);
	g_nav = clabel(scr, (g_w - S(300)) / 2, S(56), S(300), "> 500 m", C_CYAN,
		       &lv_font_montserrat_16);
	g_mode = label(scr, g_w - gutter - S(16), S(56), "E", C_CYAN,
		       &lv_font_montserrat_16);

	/* ---------------------------------------------------------- the swept bar */
	path_init(S(160), S(392), S(210), S(92), (float)(g_w - S(120)));

	for (int i = 0; i < SEGS; i++) {
		float frac = (float)i / (float)(SEGS - 1);
		bool major = (i % 4) == 0;
		struct pt p = path_pt(frac);

		int bw = S(34) + (int)(S(52) * frac) + (major ? S(14) : 0);
		int bh = major ? S(15) : S(9);
		g_seg[i] = segment(scr, p, bw, bh);
		g_seg_col[i] = C_TRACK;
	}

	/* Scale numbers, placed along the same outward normal the segments grow on, so
	 * they follow the corner instead of sitting in a straight row - which is what
	 * keeps the sweep reading as ONE scale rather than a diagonal plus a bar. */
	for (int k = 0; k <= 6; k++) {
		float frac = (float)k / 6.0f;
		struct pt p = path_pt(frac);
		float rad = p.rot * (float)M_PI / 180.0f;
		int off = S(66) + (int)(S(62) * frac);

		char nb[8];
		snprintf(nb, sizeof nb, "%d", k * 20);
		lv_obj_t *l = label(scr, 0, 0, nb, C_MUTED, &lv_font_montserrat_16);
		lv_obj_update_layout(l);
		lv_obj_set_pos(l, (int)(p.x + cosf(rad) * off) - lv_obj_get_width(l) / 2,
			          (int)(p.y + sinf(rad) * off) - lv_obj_get_height(l) / 2);
	}

	label(scr, g_w - S(104), S(150), "km/h", C_MUTED, &lv_font_montserrat_16);

	/* ---------------------------------------------------------- speed readout
	 * A recessed box low-centre. Without it the number floats with nothing holding
	 * it and the centre of the face has no anchor. */
	const int rw = S(340), rh = S(104);
	const int rx = S(300), ry = S(406);
	panel(scr, rx, ry, rw, rh);
	g_speed = clabel(scr, rx, ry + S(10), rw, "0", C_WHITE, &lv_font_montserrat_48);
	g_speed_unit = clabel(scr, rx, ry + S(72), rw, "km/h", C_MUTED,
			      &lv_font_montserrat_16);

	/* ---------------------------------------------------------- gear */
	const int gx = rx + rw + S(16), gw2 = S(104);
	panel(scr, gx, ry, gw2, rh);
	g_gear = clabel(scr, gx, ry + S(18), gw2, "N", C_MINT, &lv_font_montserrat_48);

	/* ---------------------------------------------------------- fuel gauge
	 * Same parallelogram vocabulary as the main bar, so the two read as one
	 * instrument rather than as a gauge plus a widget. */
	const int fx = gx + gw2 + S(28), fy = ry + S(30);
	const int fw = S(15), fh = S(30), fgap = S(4);
	label(scr, fx - S(20), fy + S(6), "E", C_RED, &lv_font_montserrat_16);
	for (int i = 0; i < FSEGS; i++) {
		lv_obj_t *o = box(scr, fx + i * (fw + fgap), fy, fw, fh, C_TRACK);
		lv_obj_set_style_transform_pivot_x(o, fw / 2, 0);
		lv_obj_set_style_transform_pivot_y(o, fh / 2, 0);
		lv_obj_set_style_transform_skew_x(o, -16 * 10, 0);
		g_fseg[i] = o;
		g_fseg_col[i] = C_TRACK;
	}
	label(scr, fx + FSEGS * (fw + fgap) + S(6), fy + S(6), "F", C_MUTED,
	      &lv_font_montserrat_16);

	/* ---------------------------------------------------------- info row
	 * Reference data - looked at when stopped, not while riding - so it takes the
	 * bottom strip. Reading order down the face is warnings, speed, reference. */
	const int py = g_h - S(74), ph = S(64);
	panel(scr, gutter, py, g_w - 2 * gutter, ph);

	const int cols = 4, colw = (g_w - 2 * gutter) / cols;
	const char *hdr[4] = { "TRIP", "ODO", "RANGE", "COOLANT" };
	lv_obj_t **fld[4] = { &g_trip, &g_odo, &g_range, &g_coolant };
	for (int i = 0; i < cols; i++) {
		label(scr, gutter + i * colw + S(18), py + S(8), hdr[i], C_MUTED,
		      &lv_font_montserrat_16);
		*fld[i] = label(scr, gutter + i * colw + S(18), py + S(28), "0",
				C_CYAN, &lv_font_montserrat_28);
	}

	/* Battery bar tucked under the readout - the only remaining analogue strip. */
	const int bw2 = S(340);
	box(scr, rx, ry - S(18), bw2, S(8), C_TRACK);
	g_batt_bar = box(scr, rx, ry - S(18), bw2, S(8), C_GREEN);
	g_fuel_bar = g_batt_bar;   /* fuel is shown by the segmented gauge above */
}

static void set_seg(int i, uint32_t col)
{
	if (g_seg_col[i] == col)
		return;                       /* no invalidate, no redraw */
	g_seg_col[i] = col;
	lv_obj_set_style_bg_color(g_seg[i], lv_color_hex(col), 0);
}

static void set_fseg(int i, uint32_t col)
{
	if (g_fseg_col[i] == col)
		return;
	g_fseg_col[i] = col;
	lv_obj_set_style_bg_color(g_fseg[i], lv_color_hex(col), 0);
}

/* Light a pill: the BACKGROUND carries the colour and the glyph goes dark, which is how
 * an illuminated telltale actually behaves - the lens lights, not the symbol. */
static void set_tt(lv_obj_t *o, bool on, uint32_t col)
{
	lv_obj_set_style_bg_color(o, lv_color_hex(on ? col : C_TRACK), 0);
	lv_obj_t *l = lv_obj_get_child(o, 0);
	if (l)
		lv_obj_set_style_text_color(l, lv_color_hex(on ? C_BG : C_MUTED), 0);
}

void cluster_update(const vehicle_t *v, double intro, bool live)
{
	char buf[64];

	/* Ignition sweep: every segment lights, then hands over to live data. Also a lamp
	 * check - every telltale lights through the sweep and goes out with it, the way a
	 * real cluster proves its lamps at power-on. */
	/* ⚠️ THE BAR IS A SPEEDOMETER, SO IT MUST BE DRIVEN BY SPEED.
	 *
	 * It used to be fmax(power, regen) - a TORQUE gauge, which is what the original
	 * face was (see the header). When the layout was restyled into a speedometer the
	 * scale gained 0..120 km/h numbers but the drive was left on torque, so the bar
	 * swung on ACCELERATION while the readout showed SPEED. Two unrelated quantities
	 * sharing one scale: the bar filled hard pulling away from a stop and emptied at a
	 * steady 100 km/h, which reads as random next to the number.
	 *
	 * SCALE_MAX must match the scale labels drawn in cluster_build() (k * 20, k <= 6).
	 * Change one and you must change the other, or the marks lie again. 120 also suits
	 * the model: gear = min(6, speed/19 + 1), so it tops out near 114. */
	const float SCALE_MAX = 120.0f;
	float val = live ? (float)(v->speed / SCALE_MAX) : (float)intro;
	if (val > 1.0f) val = 1.0f;
	if (val < 0.0f) val = 0.0f;

	for (int i = 0; i < SEGS; i++) {
		float frac = (float)i / (float)(SEGS - 1);
		bool lit = val >= frac - 0.004f;
		set_seg(i, lit ? zone(frac) : C_TRACK);
	}

	/* Fuel: lowest two segments go red, so "nearly empty" is a colour and not a
	 * length you have to estimate. */
	float fv = live ? (float)v->fuel : (float)intro;
	for (int i = 0; i < FSEGS; i++) {
		float frac = (float)(i + 1) / (float)FSEGS;
		bool lit = fv >= frac - 0.004f;
		set_fseg(i, lit ? (i < 2 ? C_RED : C_WHITE) : C_TRACK);
	}

	if (v->gear != g_last_gear) {
		g_last_gear = v->gear;
		if (v->gear == 0)
			lv_label_set_text(g_gear, "N");
		else {
			snprintf(buf, sizeof buf, "%d", v->gear);
			lv_label_set_text(g_gear, buf);
		}
	}

	snprintf(buf, sizeof buf, "%d", (int)(v->speed + 0.5));
	lv_label_set_text(g_speed, buf);

	snprintf(buf, sizeof buf, "%.1f", v->trip);
	lv_label_set_text(g_trip, buf);
	snprintf(buf, sizeof buf, "%d", (int)v->odo);
	lv_label_set_text(g_odo, buf);
	snprintf(buf, sizeof buf, "%d", vehicle_range(v));
	lv_label_set_text(g_range, buf);
	snprintf(buf, sizeof buf, "%d", v->coolant);
	lv_label_set_text(g_coolant, buf);

	snprintf(buf, sizeof buf, "%c", vehicle_ride_mode(v));
	lv_label_set_text(g_mode, buf);

	snprintf(buf, sizeof buf, "> %d m", v->nav_dist);
	lv_label_set_text(g_nav, buf);

	lv_obj_set_width(g_batt_bar, (int)(S(340) * v->battery));

	bool lamp = !live;                 /* lamp check during the sweep */
	set_tt(g_tt_left,  lamp || v->left_ind,  C_MINT);
	set_tt(g_tt_right, lamp || v->right_ind, C_MINT);
	set_tt(g_tt_beam,  lamp || v->high_beam, C_CYAN);
	set_tt(g_tt_abs,   lamp || v->abs,       C_AMBER);
	set_tt(g_tt_oil,   lamp || v->oil,       C_ROSE);
}
