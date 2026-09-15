#pragma once

#include <stddef.h>

#include "physical_input.h"

// Build-selected hardware map: DevKitC prototype or XIAO two-expander PCB.
// Device addresses and port bits must match the selected schematic.
extern const PhysicalInputHardwareBinding kR1InputMap[];
extern const size_t kR1InputMapCount;

const PhysicalInputHardwareBinding *r1HardwareBinding(PhysicalControlId input);
const char *r1InputEndpointLabel(PhysicalControlId input);
