#pragma once

#include <stddef.h>
#include <stdint.h>

#include "control_assignment.h"

enum class InputSourceKind : uint8_t {
    Unmapped,
    DirectGpio,
    Mcp23017,
    Adc,
};

// sourceIndex is a GPIO number for DirectGpio, an MCP port number 0..15 for
// Mcp23017, and an ADC pin/channel identifier for Adc. device identifies which
// expander to use (normally its 7-bit I2C address) and is zero otherwise.
struct InputEndpoint {
    constexpr InputEndpoint() = default;
    constexpr InputEndpoint(InputSourceKind sourceValue, uint8_t deviceValue,
                            uint8_t indexValue, bool activeLowValue)
        : source(sourceValue), device(deviceValue), sourceIndex(indexValue),
          activeLow(activeLowValue) {}

    InputSourceKind source = InputSourceKind::Unmapped;
    uint8_t device = 0;
    uint8_t sourceIndex = 0;
    bool activeLow = true;
};

struct PhysicalInputHardwareBinding {
    constexpr PhysicalInputHardwareBinding() = default;
    constexpr PhysicalInputHardwareBinding(PhysicalControlId inputValue,
                                           InputEndpoint primaryValue,
                                           InputEndpoint secondaryValue)
        : input(inputValue), primary(primaryValue), secondary(secondaryValue) {}

    PhysicalControlId input = PhysicalControlId::TimerButton;
    InputEndpoint primary;
    InputEndpoint secondary; // positive contact for a spring-centred trim
};

struct PhysicalInputEvent {
    PhysicalControlId input = PhysicalControlId::TimerButton;
    InputEvent event = InputEvent::Pressed;
    int16_t value = 0;
    uint32_t timestampMs = 0;
};

class PhysicalInputEventBuffer {
public:
    static constexpr size_t Capacity = 12;

    bool push(PhysicalControlId input, InputEvent event,
              uint32_t timestampMs, int16_t value = 0);
    void clear() { count_ = 0; overflowed_ = false; }
    size_t size() const { return count_; }
    bool overflowed() const { return overflowed_; }
    const PhysicalInputEvent &operator[](size_t index) const { return events_[index]; }

private:
    PhysicalInputEvent events_[Capacity];
    size_t count_ = 0;
    bool overflowed_ = false;
};

struct DigitalInputTiming {
    uint16_t debounceMs = 20;
    uint16_t longPressMs = 650;
    uint16_t doublePressMs = 275;
    uint16_t repeatDelayMs = 450;
    uint16_t repeatIntervalMs = 100;
};

class DebouncedContact {
public:
    explicit DebouncedContact(uint16_t debounceMs = 20) : debounceMs_(debounceMs) {}
    bool update(bool rawActive, uint32_t nowMs);
    bool active() const { return stableActive_; }
    bool rawActive() const { return rawActive_; }
    bool initialised() const { return initialised_; }

private:
    uint16_t debounceMs_;
    bool initialised_ = false;
    bool rawActive_ = false;
    bool stableActive_ = false;
    uint32_t rawChangedAtMs_ = 0;
};

class DigitalInputProcessor {
public:
    DigitalInputProcessor() = default;
    DigitalInputProcessor(PhysicalControlId input,
                          const DigitalInputTiming &timing = DigitalInputTiming(),
                          bool repeatEnabled = false);

    void update(bool rawActive, uint32_t nowMs, PhysicalInputEventBuffer &events);
    bool active() const { return contact_.active(); }
    bool rawActive() const { return contact_.rawActive(); }

private:
    PhysicalControlId input_;
    DigitalInputTiming timing_;
    DebouncedContact contact_;
    bool repeatEnabled_;
    bool longPressSent_ = false;
    bool pendingShortPress_ = false;
    uint32_t pressedAtMs_ = 0;
    uint32_t pendingShortAtMs_ = 0;
    uint32_t nextRepeatAtMs_ = 0;
};

enum class TrimPosition : int8_t {
    Invalid = -2,
    Negative = -1,
    Neutral = 0,
    Positive = 1,
};

class SpringTrimInputProcessor {
public:
    SpringTrimInputProcessor() = default;
    SpringTrimInputProcessor(PhysicalControlId input,
                             const DigitalInputTiming &timing = DigitalInputTiming());

    void update(bool rawNegativeActive, bool rawPositiveActive,
                uint32_t nowMs, PhysicalInputEventBuffer &events);
    TrimPosition position() const { return position_; }
    bool faulted() const { return position_ == TrimPosition::Invalid; }
    bool rawNegativeActive() const { return negative_.rawActive(); }
    bool rawPositiveActive() const { return positive_.rawActive(); }

private:
    void leavePosition(TrimPosition oldPosition, uint32_t nowMs,
                       PhysicalInputEventBuffer &events);
    void enterPosition(TrimPosition newPosition, uint32_t nowMs,
                       PhysicalInputEventBuffer &events);

    PhysicalControlId input_;
    DigitalInputTiming timing_;
    DebouncedContact negative_;
    DebouncedContact positive_;
    TrimPosition position_ = TrimPosition::Neutral;
    bool initialised_ = false;
    uint32_t nextRepeatAtMs_ = 0;
};

struct PhysicalInputState {
    bool mapped = false;
    bool rawPrimary = false;
    bool rawSecondary = false;
    bool stablePrimary = false;
    bool stableSecondary = false;
    bool faulted = false;
    int16_t value = 0;
    InputEvent lastEvent = InputEvent::Released;
    uint32_t lastEventMs = 0;
};

class PhysicalInputStateStore {
public:
    void setMapped(PhysicalControlId input, bool mapped);
    void setContacts(PhysicalControlId input, bool rawPrimary, bool rawSecondary,
                     bool stablePrimary, bool stableSecondary, bool faulted);
    void setValue(PhysicalControlId input, int16_t value);
    void setFaulted(PhysicalControlId input, bool faulted);
    void apply(const PhysicalInputEvent &event);
    const PhysicalInputState &state(PhysicalControlId input) const;

private:
    PhysicalInputState states_[kPhysicalControlCount];
};
