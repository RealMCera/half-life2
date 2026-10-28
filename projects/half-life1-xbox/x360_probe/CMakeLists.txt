# Xbox 360 PowerPC binary produced by xenosworks/xsdk.
cmake_minimum_required(VERSION 3.25)
project(hl1_remaster_probe C)
add_executable(hl1_remaster_probe main.c)
set_target_properties(hl1_remaster_probe PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(hl1_remaster_probe PRIVATE -Wall -Wextra -Werror)
xsdk_add_xex(hl1_remaster_probe)
