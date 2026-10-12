# Portable input mapping fixture

See [the module contract and integration example](../../src/input/input_map.md).
Build this directory directly with CMake and run CTest; no root runtime/SDL
configuration is required. Explicit C checks execute in Release even under
NDEBUG; a C++ consumer verifies extern-C linkage and the actual mapping target.

Coverage: key names/aliases, binding parse/format/clear/duplicates/limits,
keyboard resolution and diagonals, whole-vector merging, mapping/disabled or
swapped sticks, deadzones, triggers, immutable names, long binding text,
malformed counts/IDs and range/roundtrip properties. Fixtures have no device,
game data, save files or guest code dependencies.
