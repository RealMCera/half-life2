# Xbox 360 PowerPC binary produced by xenosworks/xsdk.
cmake_minimum_required(VERSION 3.25)
project(hl1_remaster_probe C)
add_executable(hl1_remaster_probe main.c wireframe.c shaded.c)
set_target_properties(hl1_remaster_probe PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(hl1_remaster_probe PRIVATE -Wall -Wextra -Werror)
xsdk_add_xex(hl1_remaster_probe)
# Separate XEX solely for diagnosing Xenia's display-controller support.
# It has NO dependency on original Half-Life game assets.
add_executable(hl1_display_probe graphics_probe.c)
set_target_properties(hl1_display_probe PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(hl1_display_probe PRIVATE -Wall -Wextra -Werror)
xsdk_add_xex(hl1_display_probe)

# Only the SECOND cloud compilation uses this flag and a patched
# temporary copy of libxsdk; the first baseline XEX is unchanged.
option(HL1_EXPERIMENTAL_LUT_PROBE "Label isolated Xenia-only LUT diagnostic" OFF)
if(HL1_EXPERIMENTAL_LUT_PROBE)
    target_compile_definitions(hl1_display_probe PRIVATE HL1_EXPERIMENTAL_LUT_PROBE=1)
endif()
