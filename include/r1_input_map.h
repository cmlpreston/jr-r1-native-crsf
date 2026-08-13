#pragma once

#include <stddef.h>

#include "physical_input.h"

// Provisional production allocation for one Waveshare MCP23017 plus six
// direct ESP32 inputs. Changing wiring later requires editing only this table.
extern const PhysicalInputHardwareBinding kR1InputMap[];
extern const size_t kR1InputMapCount;

const PhysicalInputHardwareBinding *r1HardwareBinding(PhysicalControlId input);
const char *r1InputEndpointLabel(PhysicalControlId input);
