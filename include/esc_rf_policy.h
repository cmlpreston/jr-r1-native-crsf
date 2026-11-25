#pragma once
#include "output_safety.h"
// Single-receiver requests do not depend on a selector channel. Keep the
// independent last-moment control-age and output-safety veto in the RF task.
inline bool escRfRequestAllowed(bool stale,const OutputSafetyDecision &safety){
 return !stale&&!safety.transmitRequested&&safety.status==OutputSafetyStatus::Cut;
}
