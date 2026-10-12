#include "input_map.h"
int main()
{
    InputPadMap map;
    InputRaw raw = {};
    InputPad out;
    xbox_InputMap_PadMapDefaults(&map);
    raw.src[INPUT_SRC_SOUTH] = 32767;
    xbox_InputMap_PadMap(&map, &raw, &out);
    return out.analog[INPUT_ANALOG_A] == 255 ? 0 : 1;
}
