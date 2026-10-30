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

    # Shader-drawn (not CPU BMP) RGB triangle diagnostic. Only build against
    # the disposable emulator-only LUT diagnostic SDK; never ship to hardware.
    add_executable(hl1_triangle_probe triangle_probe.c)
    set_target_properties(hl1_triangle_probe PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
    target_compile_options(hl1_triangle_probe PRIVATE -Wall -Wextra -Werror)
    xsdk_add_shaders(hl1_triangle_probe hl1_triangle.vert hl1_triangle.frag)
    xsdk_add_xex(hl1_triangle_probe)

    # Original c0a0 world geometry: actual native GPU 3D, no copyrighted
    # assets in this repository. Requires user's H360 file beside XEX.
    add_executable(hl1_world_gpu_probe world_gpu_probe.c)
    set_target_properties(hl1_world_gpu_probe PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
    target_compile_options(hl1_world_gpu_probe PRIVATE -Wall -Wextra -Werror)
    xsdk_add_shaders(hl1_world_gpu_probe hl1_world.vert hl1_world.frag)
    xsdk_add_xex(hl1_world_gpu_probe)

    # New EXPERIMENTAL indexed geometry + free-flight camera.
    # Keeps the original visually verified orbit XEX unchanged.
    add_executable(hl1_world_explore world_explore.c)
    set_target_properties(hl1_world_explore PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
    target_compile_options(hl1_world_explore PRIVATE -Wall -Wextra -Werror)
    xsdk_add_shaders(hl1_world_explore hl1_world.vert hl1_world.frag)
    xsdk_add_xex(hl1_world_explore)

    # Isolated large Source-BSP H360 preview: split indexed draws at 65532.
    # This does not modify the working HL1 viewer or collision prototypes.
    add_executable(hl1_bms_world_probe bms_world_probe.c)
    set_target_properties(hl1_bms_world_probe PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
    target_compile_options(hl1_bms_world_probe PRIVATE -Wall -Wextra -Werror)
    xsdk_add_shaders(hl1_bms_world_probe hl1_world.vert hl1_world.frag)
    xsdk_add_xex(hl1_bms_world_probe)

    # Large BMS Alpha 5 world: split draw indices + CPU floor/wall physics.
    # A distinct XEX keeps verified GPU viewer and original HL1 physics tests.
    add_executable(hl1_bms_world_walk bms_world_walk.c)
    set_target_properties(hl1_bms_world_walk PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
    target_compile_options(hl1_bms_world_walk PRIVATE -Wall -Wextra -Werror)
    xsdk_add_shaders(hl1_bms_world_walk hl1_world.vert hl1_world.frag)
    xsdk_add_xex(hl1_bms_world_walk)

    # Separate first-person walking / physics proof-of-concept.
    # Triangulated mesh floor queries + basic wall segment blocking,
    # no original GoldSrc BSP hulls, triggers, or gameplay yet.
    add_executable(hl1_world_walk world_walk.c)
    set_target_properties(hl1_world_walk PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
    target_compile_options(hl1_world_walk PRIVATE -Wall -Wextra -Werror)
    xsdk_add_shaders(hl1_world_walk hl1_world.vert hl1_world.frag)
    xsdk_add_xex(hl1_world_walk)

    # Stable-ground V2 separate XEX; keep prior physics test for comparison.
    add_executable(hl1_world_walk_stable world_walk_stable.c)
    set_target_properties(hl1_world_walk_stable PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
    target_compile_options(hl1_world_walk_stable PRIVATE -Wall -Wextra -Werror)
    xsdk_add_shaders(hl1_world_walk_stable hl1_world.vert hl1_world.frag)
    xsdk_add_xex(hl1_world_walk_stable)
endif()
