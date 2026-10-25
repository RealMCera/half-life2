/*
 * nv2a_overlay_stb.c - stb_truetype's implementation, in a file of its own
 * so the vendored header builds without the kernel's warnings.
 */
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
