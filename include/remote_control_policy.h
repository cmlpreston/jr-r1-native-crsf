#pragma once
#include "output_safety.h"
// Calibration/TX configuration also deliberately inhibits live outputs. Keep
// remote Back/Cancel usable there, without allowing pending arming or faults.
constexpr bool remoteControlAllowed(bool armRequested,OutputSafetyStatus status) {
    return !armRequested&&(status==OutputSafetyStatus::Cut||status==OutputSafetyStatus::Calibration||status==OutputSafetyStatus::ConfigurationFault);
}

// Joining home Wi-Fi must not wait for the startup warning to be dismissed.
constexpr bool wifiConnectionAllowed(bool armRequested,OutputSafetyStatus status) {
    return !armRequested && status!=OutputSafetyStatus::Armed;
}
