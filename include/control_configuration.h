#pragma once

#include <stdint.h>

#include "channel_response.h"
#include "channel_mixing.h"
#include "crsf_channel_output.h"
#include "proportional_input.h"

struct ChannelRoleSettings {
    uint8_t steeringChannel = 1;
    uint8_t throttleChannel = 2;
    bool steeringEnabled = true;
    bool throttleEnabled = true;
};

struct ChannelParameterValues {
    int16_t trimPermille = 0;
    int16_t subtrimPermille = 0;
};

enum class DriveMode : uint8_t {
    Walk,
    Normal,
};

struct DriveModeSettings {
    // WALK limits physical forward drive only. Brake/reverse retains its full
    // configured authority. Arm/CUT is an independent safety state.
    DriveMode activeMode = DriveMode::Normal;
    uint8_t walkLimitPercent = 35;
};

// One immutable, self-contained copy of every UI/model value consumed by the
// 250 Hz control task. The UI builds this on its own thread and main.cpp
// publishes the whole value atomically; the control task never follows a
// pointer back into mutable UI state.
struct ControlConfigurationSnapshot {
    uint32_t revision = 0;
    ProportionalInputProfile throttleInput;
    ProportionalInputProfile steeringInput;
    ChannelResponseProfile channelProfiles[CRSF_CHANNEL_COUNT] = {};
    ChannelParameterValues channelParameters[CRSF_CHANNEL_COUNT] = {};
    ChannelMixSettings mixing[CRSF_CHANNEL_COUNT] = {};
    uint16_t proportionalOutputMask = 0;
    uint8_t steeringChannel = 1;
    uint8_t throttleChannel = 2;
    bool steeringEnabled = true;
    bool throttleEnabled = true;
    bool modelStorageValid = true;
    bool armRequested = false;
    bool calibrationActive = false;
    DriveMode driveMode = DriveMode::Normal;
    uint8_t walkLimitPercent = 35;
};

// Revision is publication metadata, not configuration identity.
bool equivalentControlConfiguration(const ControlConfigurationSnapshot &left,
                                    const ControlConfigurationSnapshot &right);
// Live trim is deliberate. Structural response/input/role/mix edits require CUT.
bool controlConfigurationRequiresRearm(const ControlConfigurationSnapshot &before,
                                      const ControlConfigurationSnapshot &after);

uint16_t configuredRoleOwnedChannels(
    const ControlConfigurationSnapshot &configuration);
int16_t applyConfiguredResponse(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille);
int16_t applyConfiguredDriveMode(
    const ControlConfigurationSnapshot &configuration,
    int16_t configuredPermille, int16_t rawInputPermille);
int16_t applyConfiguredChannelAdjustments(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille);
int16_t configuredChannelNeutral(
    const ControlConfigurationSnapshot &configuration, uint8_t channel);

// Rapidly changing ramp state belongs exclusively to the control task. It is
// deliberately absent from the published configuration snapshot.
class ChannelSlewRuntime {
public:
    int16_t apply(const ControlConfigurationSnapshot &configuration,
                  uint8_t channel, int16_t targetPermille, uint32_t nowMs);
    void reset(uint32_t nowMs);

private:
    int16_t slewedPermille_[CRSF_CHANNEL_COUNT] = {};
    uint32_t lastUpdateMs_[CRSF_CHANNEL_COUNT] = {};
};

struct ProcessedProportionalChannel {
    int16_t requestedPermille = 0;
    int16_t safePermille = 0;
};

// Run one proportional source through the common per-channel pipeline:
// response profile, role modifier, channel adjustments and optional slew.
// Safe values use the same static shaping stages but never slew. Callers can
// bypass slew while output safety is CUT, leaving the runtime at neutral for
// the next deliberate live transition.
ProcessedProportionalChannel processConfiguredProportionalChannel(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille, int16_t safeInputPermille,
    ChannelSlewRuntime &slew, uint32_t nowMs, bool advanceSlew,
    int16_t switchedRatePermille = 1000);

void processConfiguredProportionalOutputs(
    const ControlConfigurationSnapshot &configuration,
    CrsfChannelOutputModel &outputs, ChannelSlewRuntime &slew,
    uint32_t nowMs, bool advanceSlew, uint16_t reservedChannels = 0);


// Compiled on configuration revision changes, never by the UI paint loop.
struct CompiledMixPlan {
    uint8_t order[CRSF_CHANNEL_COUNT] = {};
    uint8_t count = 0;
    uint16_t requiredMask = 0;
    bool valid = false;
};
CompiledMixPlan compileConfiguredMixing(const ControlConfigurationSnapshot &configuration);
struct ConfiguredMixInspection {
    bool inputsValid = false;
    bool throttleNeutral = false;
};
ConfiguredMixInspection inspectConfiguredMixInputs(
    const ControlConfigurationSnapshot &configuration, const MixInputSnapshot &inputs,
    const CrsfChannelOutputModel &bindings, const CompiledMixPlan &plan);

// Shared firmware/simulator pipeline: raw bindings and physical sources enter
// once; shaping and safety values are applied exactly once to mixed channels.
void processConfiguredMixedOutputs(const ControlConfigurationSnapshot &configuration,
    CrsfChannelOutputModel &outputs, const MixInputSnapshot &inputs,
    ChannelMixRuntime &mix, ChannelSlewRuntime &slew, uint32_t nowMs, bool live, const CompiledMixPlan *plan = nullptr);
bool validConfiguredMixing(const ControlConfigurationSnapshot &configuration);
bool configuredMixInputsValid(const ControlConfigurationSnapshot &configuration,
                             const MixInputSnapshot &inputs, const CrsfChannelOutputModel *bindings=nullptr);

// Additional arming veto only; never replaces the physical ADC neutral check.
bool configuredMixedThrottleNeutral(const ControlConfigurationSnapshot &configuration,
                                    const MixInputSnapshot &inputs, const CrsfChannelOutputModel *bindings=nullptr);

// Non-transmitting bench preview. Source samples are at the contribution input
// (after any source channel/input shaping); switch/rate transitions are settled.
struct ChannelMixPreview {
    int16_t contributions[2] = {};
    int16_t mixed = 0, output = 0;
};
ChannelMixPreview previewChannelMix(const ControlConfigurationSnapshot &configuration,
    uint8_t channel,const ChannelMixSettings &settings,int16_t base,
    const int16_t sources[2],bool switchHigh,bool walk);

// Sweep one preview input: 0=existing input, 1/2=mix source.
// Other inputs stay fixed; matching source IDs move together.
constexpr unsigned kMixCurvePoints=41;
void previewChannelMixCurve(const ControlConfigurationSnapshot &configuration,
    uint8_t channel,const ChannelMixSettings &settings,int16_t base,
    const int16_t sources[2],bool switchHigh,bool walk,uint8_t sweep,
    int16_t output[kMixCurvePoints]);
