#pragma once

#include <Arduino.h>

// A physical R1 control can either operate the transmitter UI itself or feed
// one of the sixteen CRSF channels.  These definitions intentionally contain
// no GPIO numbers: the interface-PCB input map and the user's assignments are
// separate concerns.
enum class PhysicalControlKind : uint8_t {
    Button,
    Toggle,
    Proportional,
};

enum class ControlBehaviour : uint8_t {
    Momentary,
    Toggle,
    ThreePosition,
    Proportional,
};

enum class AssignmentTarget : uint8_t {
    None,
    TransmitterFunction,
    CrsfChannel,
};

enum class TransmitterFunction : uint8_t {
    OpenMenu,
    NextPage,
    PreviousPage,
    ResetMaximums,
    ResetTrip,
    TimerStartStop,
    TrimIncrease,
    TrimDecrease,
};

struct ControlAssignment {
    uint8_t physicalInput = 0;
    PhysicalControlKind inputKind = PhysicalControlKind::Button;
    ControlBehaviour behaviour = ControlBehaviour::Momentary;
    AssignmentTarget target = AssignmentTarget::None;
    TransmitterFunction function = TransmitterFunction::OpenMenu;
    uint8_t crsfChannel = 0; // 1..16 when target == CrsfChannel
    bool inverted = false;
};
