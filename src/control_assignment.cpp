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

const char *transmitterFunctionName(TransmitterFunction function)
{
    switch (function) {
        case TransmitterFunction::OpenMenu: return "OPEN MENU";
        case TransmitterFunction::Dashboard: return "DASHBOARD";
        case TransmitterFunction::NextPage: return "NEXT PAGE";
        case TransmitterFunction::PreviousPage: return "PREVIOUS PAGE";
        case TransmitterFunction::QuickPanel: return "QUICK PANEL";
        case TransmitterFunction::DisplayOnOff: return "DISPLAY ON/OFF";
        case TransmitterFunction::DisplayWake: return "DISPLAY WAKE";
        case TransmitterFunction::ToggleNightMode: return "NIGHT MODE";
        case TransmitterFunction::TouchLock: return "TOUCH LOCK";
        case TransmitterFunction::ArmCut: return "ARM / CUT";
        case TransmitterFunction::PanicBrake: return "PANIC BRAKE";
        case TransmitterFunction::TimerStartStop: return "TIMER START/STOP";
        case TransmitterFunction::TimerLap: return "TIMER LAP";
        case TransmitterFunction::TimerReset: return "TIMER RESET";
        case TransmitterFunction::ResetMaximums: return "RESET MAXIMUMS";
        case TransmitterFunction::ResetTrip: return "RESET TRIP";
        case TransmitterFunction::ResetConsumedCapacity: return "RESET CAPACITY";
        case TransmitterFunction::LoggingStartStop: return "LOG START/STOP";
        case TransmitterFunction::AlarmAcknowledge: return "ACK ALARM";
        case TransmitterFunction::NextModel: return "NEXT MODEL";
        case TransmitterFunction::PreviousModel: return "PREVIOUS MODEL";
        case TransmitterFunction::Count: break;
    }
    return "UNKNOWN";
}

const char *driveModeSelectionName(DriveModeSelection selection)
{
    switch (selection) {
        case DriveModeSelection::CutArm: return "CUT / ARM";
        case DriveModeSelection::WalkNormal: return "WALK / NORMAL";
        case DriveModeSelection::ProfileAProfileB: return "PROFILE A / B";
        case DriveModeSelection::Count: break;
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
    if (binding.target == AssignmentTarget::TransmitterFunction &&
        binding.targetId >= static_cast<uint16_t>(TransmitterFunction::Count))
        return fail("invalid transmitter function");
    if (binding.target == AssignmentTarget::DriveMode &&
        binding.targetId >= static_cast<uint16_t>(DriveModeSelection::Count))
        return fail("invalid drive mode selection");
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
