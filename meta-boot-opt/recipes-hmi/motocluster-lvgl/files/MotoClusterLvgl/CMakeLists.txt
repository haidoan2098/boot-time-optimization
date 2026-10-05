cmake_minimum_required(VERSION 3.16)
project(motocluster-lvgl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# LVGL comes from meta-openembedded's lvgl recipe as a SHARED library plus headers.
# It ships no CMake package config, so find it by hand rather than find_package().
#
# Two include directories, not one, and both are needed:
#   <includedir>/lvgl   - where the lvgl recipe's do_install:append puts lv_conf.h
#   <includedir>        - so that lvgl's own "#include <lvgl/...>" paths resolve
# Getting this wrong shows up as a confusing "lv_conf.h not found" from deep inside
# lvgl.h rather than as a missing-library error.
find_library(LVGL_LIB NAMES lvgl REQUIRED)
find_path(LVGL_INCLUDE_DIR NAMES lvgl.h PATH_SUFFIXES lvgl REQUIRED)

add_executable(motocluster-lvgl
    src/main.c
    src/cluster.c
    src/vehicle.c
)

target_include_directories(motocluster-lvgl PRIVATE
    ${LVGL_INCLUDE_DIR}
    ${LVGL_INCLUDE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -lm is not optional: the vehicle simulation and the L-path geometry both use libm
# (fmod, sin, cos), and on this toolchain they are not inlined at -O2.
# Dynamic link on purpose: this is the baseline. Static linking (to cut ld.so work at
# startup) is a later optimization experiment, measured on its own.
target_link_libraries(motocluster-lvgl PRIVATE ${LVGL_LIB} m)

include(GNUInstallDirs)
install(TARGETS motocluster-lvgl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
