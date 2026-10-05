# Configure meta-oe's lvgl for this board. The app draws to /dev/fb0 with the software
# renderer, so the backend and colour depth must match the framebuffer.

# meta-oe defaults to "drm", which sets LV_USE_LINUX_FBDEV 0.
PACKAGECONFIG = "fbdev"

# Must equal the framebuffer's bits per pixel, or the panel shows garbage with no log
# line. 16 is the value measured on the reference board. NOT verified on this one:
# check /sys/class/graphics/fb0/bits_per_pixel after the first boot and change it
# here if it differs.
LVGL_CONFIG_LV_COLOR_DEPTH = "16"

# The app uses these Montserrat sizes. lv-conf.inc does not enable any font, and a
# clean build fails with "'lv_font_montserrat_NN' undeclared" without this.
# Must run after lv-conf.inc's do_configure:append, hence appending again.
do_configure:append() {
    sed -r -i \
        -e "s|^([[:space:]]*#define LV_FONT_MONTSERRAT_16[[:space:]]).*|\11|" \
        -e "s|^([[:space:]]*#define LV_FONT_MONTSERRAT_28[[:space:]]).*|\11|" \
        -e "s|^([[:space:]]*#define LV_FONT_MONTSERRAT_48[[:space:]]).*|\11|" \
        "${S}/lv_conf.h"

    for f in 16 28 48; do
        grep -qE "^#define LV_FONT_MONTSERRAT_${f} 1" "${S}/lv_conf.h" || \
            bbfatal "lvgl: LV_FONT_MONTSERRAT_${f} was not enabled in lv_conf.h"
    done
}
