#include "r1_input_map.h"

#include "config.h"

namespace {

constexpr InputEndpoint adc(uint8_t gpio)
{
    return {InputSourceKind::Adc, 0, gpio, false};
}

constexpr InputEndpoint mcp(uint8_t port)
{
    return {InputSourceKind::Mcp23017, MCP23017_I2C_ADDRESS, port, true};
}

constexpr InputEndpoint gpio(uint8_t pin)
{
    return {InputSourceKind::DirectGpio, 0, pin, true};
}

constexpr InputEndpoint none()
{
    return {};
}

constexpr const char *kEndpointLabels[] = {
    "ADC GPIO2", "ADC GPIO1", "MCP PA0", "MCP PA1", "MCP PA2/3",
    "MCP PA4/5", "MCP PA6/7", "MCP PB0/1", "MCP PB2/3", "MCP PB4",
    "MCP PB5", "MCP PB6", "MCP PB7", "GPIO6", "GPIO7", "GPIO14",
    "GPIO15", "GPIO21", "GPIO47",
};

static_assert(sizeof(kEndpointLabels) / sizeof(kEndpointLabels[0]) ==
              kPhysicalControlCount, "endpoint label for every control");

} // namespace

const PhysicalInputHardwareBinding kR1InputMap[] = {
    // Active breadboard/prototype map. The replacement keypad PCB instead uses
    // PA0..PA7 for MODE, UP, PLUS, SELECT, MINUS, DOWN, CLEAR and PAGE. Keep
    // that planned revision as a separate hardware profile; changing this
    // table directly would misinterpret the currently connected controls.
    {PhysicalControlId::SteeringWheel, adc(STEERING_ADC_PIN), none()},
    {PhysicalControlId::ThrottleTrigger, adc(THROTTLE_ADC_PIN), none()},
    {PhysicalControlId::TimerButton, mcp(0), none()},
    {PhysicalControlId::DriveModeSwitch, mcp(1), none()},
    {PhysicalControlId::GripTrimA, mcp(2), mcp(3)},
    {PhysicalControlId::GripTrimB, mcp(4), mcp(5)},
    {PhysicalControlId::ThrottleTrim, mcp(6), mcp(7)},
    {PhysicalControlId::SteeringTrim, mcp(8), mcp(9)},
    {PhysicalControlId::Aux3Trim, mcp(10), mcp(11)},
    {PhysicalControlId::IdleUpSwitch, mcp(12), none()},
    {PhysicalControlId::DisplaySwitch, mcp(13), none()},
    {PhysicalControlId::KeyMode, mcp(14), none()},
    {PhysicalControlId::KeyUp, mcp(15), none()},
    {PhysicalControlId::KeyPlus, gpio(6), none()},
    {PhysicalControlId::KeySelect, gpio(7), none()},
    {PhysicalControlId::KeyPage, gpio(14), none()},
    {PhysicalControlId::KeyClear, gpio(15), none()},
    {PhysicalControlId::KeyDown, gpio(21), none()},
    {PhysicalControlId::KeyMinus, gpio(47), none()},
};

const size_t kR1InputMapCount = sizeof(kR1InputMap) / sizeof(kR1InputMap[0]);
static_assert(kR1InputMapCount == kPhysicalControlCount,
              "hardware mapping for every R1 control");

const PhysicalInputHardwareBinding *r1HardwareBinding(PhysicalControlId input)
{
    const size_t index = static_cast<size_t>(input);
    return index < kR1InputMapCount ? &kR1InputMap[index] : nullptr;
}

const char *r1InputEndpointLabel(PhysicalControlId input)
{
    const size_t index = static_cast<size_t>(input);
    return index < kPhysicalControlCount ? kEndpointLabels[index] : "UNMAPPED";
}
