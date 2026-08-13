#pragma once

#include <stddef.h>
#include <stdint.h>

// Stable, model-independent identifiers for every assignable R1 control.
// GPIOs belong in the hardware input layer; bindings refer only to these IDs.
enum class PhysicalControlId : uint8_t {
    SteeringWheel,
    ThrottleTrigger,
    TimerButton,
    DriveModeSwitch,
    GripTrimA,
    GripTrimB,
    ThrottleTrim,
    SteeringTrim,
    Aux3Trim,
    IdleUpSwitch,
    DisplaySwitch,
    KeyMode,
    KeyUp,
    KeyPlus,
    KeySelect,
    KeyPage,
    KeyClear,
    KeyDown,
    KeyMinus,
    Count,
};

enum class PhysicalControlKind : uint8_t {
    Proportional,
    MomentaryButton,
    MaintainedSwitch,
    SpringCentredTrim,
    KeypadButton,
};

// Trims are composite inputs. Their two electrical contacts become negative
// and positive events while the logical control retains one stable ID.
enum class InputEvent : uint8_t {
    ValueChanged,
    Pressed,
    Released,
    ShortPress,
    LongPress,
    DoublePress,
    Repeat,
    NegativePress,
    NegativeRelease,
    PositivePress,
    PositiveRelease,
    NegativeRepeat,
    PositiveRepeat,
    PositionLow,
    PositionHigh,
    InvalidState,
    StateRestored,
};

enum ControlContext : uint8_t {
    ContextAlways = 1U << 0,
    ContextDashboard = 1U << 1,
    ContextMenu = 1U << 2,
    ContextCut = 1U << 3,
    ContextArmed = 1U << 4,
};

enum class ControlBehaviour : uint8_t {
    Proportional,
    Momentary,
    Toggle,
    TwoPosition,
    SpringReturnThreePosition,
    Stepped,
    ThreePositionSelector,
    Pulse,
};

enum class AssignmentTarget : uint8_t {
    None,
    TransmitterFunction,
    ModelParameter,
    DriveMode,
    CrsfChannel,
    Macro,
};

enum class TransmitterFunction : uint8_t {
    OpenMenu,
    Dashboard,
    NextPage,
    PreviousPage,
    QuickPanel,
    DisplayOnOff,
    DisplayWake,
    ToggleNightMode,
    TouchLock,
    ArmCut,
    PanicBrake,
    TimerStartStop,
    TimerLap,
    TimerReset,
    ResetMaximums,
    ResetTrip,
    ResetConsumedCapacity,
    LoggingStartStop,
    AlarmAcknowledge,
    NextModel,
    PreviousModel,
};

enum class ModelParameter : uint8_t {
    SteeringTrim,
    ThrottleTrim,
    SteeringRate,
    ThrottleRate,
    WalkLimit,
    ForwardExpo,
    BrakeExpo,
    BrakeStrength,
    DragBrake,
    AbsDepth,
    AbsPoint,
    AbsFrequency,
    DisplayBrightness,
};

// Per-channel parameters use a tagged targetId so they can coexist with the
// older model-wide ModelParameter IDs above. Values are stored in permille:
// 10 permille is one percent of conventional servo travel.
enum class ChannelParameterKind : uint8_t {
    Trim = 0,
    Subtrim = 1,
};

constexpr uint16_t kChannelParameterTag = 0x8000;
constexpr uint16_t encodeChannelParameterTarget(
    uint8_t channel, ChannelParameterKind kind)
{
    return static_cast<uint16_t>(kChannelParameterTag |
        (static_cast<uint16_t>(channel) << 8) |
        static_cast<uint8_t>(kind));
}

constexpr bool isChannelParameterTarget(uint16_t targetId)
{
    return (targetId & kChannelParameterTag) != 0;
}

constexpr uint8_t channelParameterTargetChannel(uint16_t targetId)
{
    return static_cast<uint8_t>((targetId >> 8) & 0x1f);
}

constexpr ChannelParameterKind channelParameterTargetKind(uint16_t targetId)
{
    return static_cast<ChannelParameterKind>(targetId & 0xff);
}

const char *channelParameterKindName(ChannelParameterKind kind);

enum class DriveModeSelection : uint8_t {
    CutArm,
    WalkNormal,
    ProfileAProfileB,
};

struct PhysicalControlDescriptor {
    PhysicalControlId id;
    PhysicalControlKind kind;
    const char *name;
    bool hardwareMapped;
};

// A model may have multiple bindings for one input. targetId contains the
// selected TransmitterFunction, ModelParameter, DriveModeSelection or macro ID.
// crsfChannel is 1..16 only when target == CrsfChannel.
struct ControlBinding {
    PhysicalControlId input = PhysicalControlId::TimerButton;
    InputEvent event = InputEvent::ShortPress;
    uint8_t contexts = ContextAlways;
    AssignmentTarget target = AssignmentTarget::None;
    ControlBehaviour behaviour = ControlBehaviour::Momentary;
    uint16_t targetId = 0;
    uint8_t crsfChannel = 0;
    int16_t minimum = -1000;
    int16_t maximum = 1000;
    int16_t step = 50;
    int16_t initial = 0;
    int16_t failsafe = 0;
    uint16_t pulseMs = 250;
    bool enabled = true;
    bool persistent = false;
    bool inverted = false;
    bool safetyCritical = false;
};

constexpr size_t kPhysicalControlCount =
    static_cast<size_t>(PhysicalControlId::Count);

const PhysicalControlDescriptor &physicalControlDescriptor(size_t index);
const char *physicalControlKindName(PhysicalControlKind kind);
const char *inputEventName(InputEvent event);
const char *assignmentTargetName(AssignmentTarget target);
const char *controlBehaviourName(ControlBehaviour behaviour);
bool validateControlBinding(const ControlBinding &binding, const char **reason = nullptr);
