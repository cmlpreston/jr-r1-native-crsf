#include "control_assignment.h"

namespace {

constexpr PhysicalControlDescriptor kControls[] = {
    {PhysicalControlId::SteeringWheel, PhysicalControlKind::Proportional, "STEERING WHEEL", false},
    {PhysicalControlId::ThrottleTrigger, PhysicalControlKind::Proportional, "THROTTLE TRIGGER", true},
    {PhysicalControlId::TimerButton, PhysicalControlKind::MomentaryButton, "TIMER BUTTON", false},
    {PhysicalControlId::DriveModeSwitch, PhysicalControlKind::MaintainedSwitch, "DRIVE MODE", false},
    {PhysicalControlId::GripTrimA, PhysicalControlKind::SpringCentredTrim, "GRIP TRIM A", false},
    {PhysicalControlId::GripTrimB, PhysicalControlKind::SpringCentredTrim, "GRIP TRIM B", false},
    {PhysicalControlId::ThrottleTrim, PhysicalControlKind::SpringCentredTrim, "THROTTLE TRIM", false},
    {PhysicalControlId::SteeringTrim, PhysicalControlKind::SpringCentredTrim, "STEERING TRIM", false},
    {PhysicalControlId::Aux3Trim, PhysicalControlKind::SpringCentredTrim, "AUX 3 TRIM", false},
    {PhysicalControlId::IdleUpSwitch, PhysicalControlKind::MaintainedSwitch, "IDLE UP", false},
    {PhysicalControlId::DisplaySwitch, PhysicalControlKind::MaintainedSwitch, "DISPLAY SWITCH", false},
    {PhysicalControlId::KeyMode, PhysicalControlKind::KeypadButton, "KEY: MODE", false},
    {PhysicalControlId::KeyUp, PhysicalControlKind::KeypadButton, "KEY: UP", false},
    {PhysicalControlId::KeyPlus, PhysicalControlKind::KeypadButton, "KEY: PLUS", false},
    {PhysicalControlId::KeySelect, PhysicalControlKind::KeypadButton, "KEY: SELECT", false},
    {PhysicalControlId::KeyPage, PhysicalControlKind::KeypadButton, "KEY: PAGE", false},
    {PhysicalControlId::KeyClear, PhysicalControlKind::KeypadButton, "KEY: CLEAR", false},
    {PhysicalControlId::KeyDown, PhysicalControlKind::KeypadButton, "KEY: DOWN", false},
    {PhysicalControlId::KeyMinus, PhysicalControlKind::KeypadButton, "KEY: MINUS", false},
};

static_assert(sizeof(kControls) / sizeof(kControls[0]) == kPhysicalControlCount,
              "Every PhysicalControlId needs a descriptor");

} // namespace

const PhysicalControlDescriptor &physicalControlDescriptor(size_t index)
{
    if (index >= kPhysicalControlCount) index = 0;
    return kControls[index];
}

const char *physicalControlKindName(PhysicalControlKind kind)
{
    switch (kind) {
        case PhysicalControlKind::Proportional: return "PROPORTIONAL";
        case PhysicalControlKind::MomentaryButton: return "BUTTON";
        case PhysicalControlKind::MaintainedSwitch: return "2-POS SWITCH";
        case PhysicalControlKind::SpringCentredTrim: return "SPRING TRIM +/-";
        case PhysicalControlKind::KeypadButton: return "KEYPAD";
    }
    return "UNKNOWN";
}

const char *inputEventName(InputEvent event)
{
    switch (event) {
        case InputEvent::ValueChanged: return "VALUE";
        case InputEvent::Pressed: return "PRESS";
        case InputEvent::Released: return "RELEASE";
        case InputEvent::ShortPress: return "SHORT";
        case InputEvent::LongPress: return "LONG";
        case InputEvent::DoublePress: return "DOUBLE";
        case InputEvent::Repeat: return "REPEAT";
        case InputEvent::NegativePress: return "- PRESS";
        case InputEvent::NegativeRelease: return "- RELEASE";
        case InputEvent::PositivePress: return "+ PRESS";
        case InputEvent::PositiveRelease: return "+ RELEASE";
        case InputEvent::NegativeRepeat: return "- REPEAT";
        case InputEvent::PositiveRepeat: return "+ REPEAT";
        case InputEvent::PositionLow: return "LOW";
        case InputEvent::PositionHigh: return "HIGH";
        case InputEvent::InvalidState: return "FAULT";
        case InputEvent::StateRestored: return "RESTORED";
    }
    return "UNKNOWN";
}

const char *assignmentTargetName(AssignmentTarget target)
{
    switch (target) {
        case AssignmentTarget::None: return "NONE";
        case AssignmentTarget::TransmitterFunction: return "TX FUNCTION";
        case AssignmentTarget::ModelParameter: return "MODEL PARAMETER";
        case AssignmentTarget::DriveMode: return "DRIVE MODE";
        case AssignmentTarget::CrsfChannel: return "CRSF CHANNEL";
        case AssignmentTarget::Macro: return "MACRO";
    }
    return "UNKNOWN";
}

const char *controlBehaviourName(ControlBehaviour behaviour)
{
    switch (behaviour) {
        case ControlBehaviour::Proportional: return "PROPORTIONAL";
        case ControlBehaviour::Momentary: return "MOMENTARY";
        case ControlBehaviour::Toggle: return "TOGGLE";
        case ControlBehaviour::TwoPosition: return "TWO POSITION";
        case ControlBehaviour::SpringReturnThreePosition: return "JOG -/0/+";
        case ControlBehaviour::Stepped: return "STEPPED";
        case ControlBehaviour::ThreePositionSelector: return "3-POS SELECT";
        case ControlBehaviour::Pulse: return "PULSE";
    }
    return "UNKNOWN";
}

const char *channelParameterKindName(ChannelParameterKind kind)
{
    switch (kind) {
        case ChannelParameterKind::Trim: return "TRIM";
        case ChannelParameterKind::Subtrim: return "SUBTRIM";
    }
    return "UNKNOWN";
}

bool validateControlBinding(const ControlBinding &binding, const char **reason)
{
    auto fail = [reason](const char *message) {
        if (reason != nullptr) *reason = message;
        return false;
    };
    if (static_cast<size_t>(binding.input) >= kPhysicalControlCount)
        return fail("invalid physical input");
    if (binding.contexts == 0) return fail("no active context");
    if (binding.minimum > binding.maximum) return fail("minimum exceeds maximum");
    if (binding.initial < binding.minimum || binding.initial > binding.maximum)
        return fail("initial value outside range");
    if (binding.failsafe < binding.minimum || binding.failsafe > binding.maximum)
        return fail("failsafe value outside range");
    if (binding.target == AssignmentTarget::CrsfChannel &&
        (binding.crsfChannel < 1 || binding.crsfChannel > 16))
        return fail("CRSF channel must be 1..16");
    if (binding.target != AssignmentTarget::CrsfChannel && binding.crsfChannel != 0)
        return fail("non-channel binding has a CRSF channel");
    if (binding.target == AssignmentTarget::ModelParameter &&
        isChannelParameterTarget(binding.targetId)) {
        const uint8_t channel = channelParameterTargetChannel(binding.targetId);
        const ChannelParameterKind kind = channelParameterTargetKind(binding.targetId);
        if (channel < 1 || channel > 16)
            return fail("channel parameter must use channel 1..16");
        if (kind != ChannelParameterKind::Trim &&
            kind != ChannelParameterKind::Subtrim)
            return fail("invalid channel parameter kind");
        if (binding.behaviour != ControlBehaviour::Stepped)
            return fail("channel trim requires stepped behaviour");
    }
    if ((binding.behaviour == ControlBehaviour::Stepped ||
         binding.behaviour == ControlBehaviour::ThreePositionSelector) &&
        binding.step <= 0)
        return fail("stepped binding needs a positive step");
    if (binding.behaviour == ControlBehaviour::Pulse && binding.pulseMs == 0)
        return fail("pulse binding needs a duration");
    if (reason != nullptr) *reason = nullptr;
    return true;
}
