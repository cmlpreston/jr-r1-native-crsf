#include "output_safety.h"

OutputSafetyDecision arbitrateOutputSafety(
    OutputSafetyStatus controlStatus, bool controlFresh)
{
    OutputSafetyDecision decision;
    decision.status = controlFresh
        ? controlStatus : OutputSafetyStatus::ControlStale;
    decision.transmitRequested =
        decision.status == OutputSafetyStatus::Armed;
    return decision;
}
