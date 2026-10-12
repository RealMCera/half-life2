# Portable input mapping (opt-in)

`xbox_input_map` is a C11 utility for bindings, controller remapping, stick
and trigger thresholds. It is independent of SDL, XInput, DirectInput and
kernel/USB code. It originates in the
[Def Jam project](https://github.com/vyanhursky/DefJamFFNY-recomp).

Existing `xbox_input` readers and toolkit defaults are unchanged. The target is
excluded from the default build and from the `xboxrecomp` umbrella. A port opts
in by linking `xbox_input_map`; building this utility alone does not need an
SDL package or the toolkit's runtime prerequisites. It uses the system math
library (`m` on non-MSVC toolchains).

## What it does

The module parses/formats up to four named inputs per Xbox control, resolves
held keys to an Xbox pad state, remaps normalized controller controls, applies
radial deadzones and trigger thresholds, and combines two pad states. Names
are ASCII case-insensitive. Unknown/oversized tokens and bindings beyond four
are reported; duplicate keys are ignored. Long text is processed completely,
not truncated. `none`, empty tokens and NULL text add no binding; parsing
replaces the selected control's bindings.

Key/source name strings are immutable. States belong to the caller: independent
states can be processed concurrently without device initialization or global
mapping state. Synchronize concurrent access to the same writable state.
Required pointers must be non-NULL unless a function explicitly documents
otherwise. `InputPad` contains semantic values, not a packed USB wire report.

## Stable key and controller contract

`InputKey` contains explicitly numbered library IDs. **These are not Windows
virtual keys, SDL keycodes/scancodes or DirectInput scan codes.** Do not index
the held-key array with native event codes. Translate them to named constants
such as `INPUT_KEY_SPACE` or use `xbox_InputMap_KeyFromName("space")`. Numeric
IDs from the original Def Jam module have deliberately not been retained.
Persist binding names rather than host codes. `INPUT_KEY_NONE` is reserved.

Letters, digits and named punctuation describe logical keys (for example `A`
and `semicolon`), not Unicode text or physical scan positions. The adapter
owns keyboard-layout translation; this module does not implement IME input or
a layout database. A port offering physical-key controls should translate its
chosen physical keys explicitly and document that policy. Generic modifier
IDs (`SHIFT`, `CTRL`, `ALT`) are separate from left/right IDs: an adapter wishing
to support both kinds of binding updates both the side-specific and aggregate
held state. Wheel inputs are short pulses; the adapter defines their duration.

`InputRaw.src[]` contains normalized 0..32767 values (digital pressed=32767),
missing controls are zero, and negative source values are treated as zero.
Face names are positional (`south`, `east`, `west`, `north`). Signed stick
axes use -32768..32767 with positive Y upwards; adapters invert host Y when
necessary. An adapter must populate directional `src[]` entries separately
when using stick directions as button sources; axes are not implicitly copied
into those entries. Invalid source/stick selections resolve to neutral.

Stick/trigger threshold percentages are clamped to 0..95, digital-button
thresholds to 0..100 (zero still requires a nonzero source). Stick output is
radially limited to full deflection. Xbox face/shoulder controls are emitted
as 0/255; mapped triggers retain analog travel. Merging ORs digital buttons,
takes the maximum analog pressure, and selects the entire stick vector with
the greater magnitude (the destination wins ties).

`xbox_InputMap_PadMapDefaults` explicitly selects the conventional face/shoulder
mapping, 15% stick deadzones, 5% trigger thresholds and a 50% digital threshold.
It is a convenience initializer, not an automatic change to existing ports.
An adopter should deliberately choose its mapping and thresholds.

## Minimal keyboard example

```c
#include "input_map.h"

InputBindings bindings = {0};
uint8_t held[INPUT_KEY_COUNT] = {0};
InputPad pad;

xbox_InputMap_BindingsParse(&bindings, INPUT_A, "space, mouse1");
/* The host event adapter translates its Space-down event to this library ID. */
held[INPUT_KEY_SPACE] = 1;
xbox_InputMap_KeysResolve(&bindings, held, &pad);
/* pad.analog[INPUT_ANALOG_A] == 255. The port supplies its normal report path. */
```

Link using `target_link_libraries(your_host PRIVATE xbox_input_map)` after the
normal toolkit `add_subdirectory`. A standalone utility consumer may instead
add `src/input/mapping` directly. Keyboard and controller examples are tested
in `tests/input_map`. No device reader, rumble/hotplug/focus behavior, settings
file migration or guest report submission is added by this module.

## Testing

```sh
cmake -S tests/input_map -B build/input-map -DCMAKE_BUILD_TYPE=Release
cmake --build build/input-map --config Release
ctest --test-dir build/input-map -C Release --output-on-failure
```

The C checks are explicit and execute under `NDEBUG`; the C++ consumer also
links the actual C library. Tests require no controller, game data or SDL.
Real device adoption separately needs adapter range/polarity, layout and
hardware tests. Arithmetic success does not prove device discovery or latency.
