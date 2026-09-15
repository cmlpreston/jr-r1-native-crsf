#include "r1_input_map.h"

#include "config.h"

namespace {

constexpr InputEndpoint adc(uint8_t gpio)
{
    return {InputSourceKind::Adc, 0, gpio, false};
}

#ifdef JR_R1_XIAO_PCB
constexpr InputEndpoint k1(uint8_t port)
{
    return {InputSourceKind::Mcp23017, 0x20, port, true};
}
constexpr InputEndpoint k2(uint8_t port)
{
    return {InputSourceKind::Mcp23017, 0x21, port, true};
}
#else
constexpr InputEndpoint mcp(uint8_t port)
{
    return {InputSourceKind::Mcp23017, MCP23017_I2C_ADDRESS, port, true};
}

constexpr InputEndpoint gpio(uint8_t pin)
{
    return {InputSourceKind::DirectGpio, 0, pin, true};
}

#endif

constexpr InputEndpoint none()
{
    return {};
}

#ifdef JR_R1_XIAO_PCB
constexpr const char *kEndpointLabels[] = {
    "ADC GPIO7", "ADC GPIO8", "K2 PA1", "K2 PA4", "K2 PA0/2",
    "K2 PA3/PB5", "K1 PA2/0", "K1 PA4/PB6", "K2 PB1/2", "K2 PB6",
    "UNMAPPED", "K1 PA1", "K1 PB5", "K1 PB3", "K1 PB4", "K1 PA3",
    "K1 PB0", "K1 PB2", "K1 PB1",
};
#else
constexpr const char *kEndpointLabels[] = {
    "ADC GPIO2", "ADC GPIO1", "MCP PA0", "MCP PA1", "MCP PA2/3",
    "MCP PA4/5", "MCP PA6/7", "MCP PB0/1", "MCP PB2/3", "MCP PB4",
    "MCP PB5", "MCP PB6", "MCP PB7", "GPIO6", "GPIO7", "GPIO14",
    "GPIO15", "GPIO21", "GPIO47",
};

#endif

static_assert(sizeof(kEndpointLabels) / sizeof(kEndpointLabels[0]) ==
              kPhysicalControlCount, "endpoint label for every control");

} // namespace

const PhysicalInputHardwareBinding kR1InputMap[] = {
#ifdef JR_R1_XIAO_PCB
    // Schematic PDF_[No Variations] (5), 2026-09-13. Trim primary is MINUS.
    {PhysicalControlId::SteeringWheel, adc(STEERING_ADC_PIN), none()},
    {PhysicalControlId::ThrottleTrigger, adc(THROTTLE_ADC_PIN), none()},
    {PhysicalControlId::TimerButton, k2(1), none()},
    {PhysicalControlId::DriveModeSwitch, k2(4), none()},
    {PhysicalControlId::GripTrimA, k2(0), k2(2)},
    {PhysicalControlId::GripTrimB, k2(3), k2(13)},
    {PhysicalControlId::ThrottleTrim, k1(2), k1(0)},
    {PhysicalControlId::SteeringTrim, k1(4), k1(14)},
    {PhysicalControlId::Aux3Trim, k2(9), k2(10)},
    {PhysicalControlId::IdleUpSwitch, k2(14), none()},
    {PhysicalControlId::DisplaySwitch, none(), none()},
    {PhysicalControlId::KeyMode, k1(1), none()},
    {PhysicalControlId::KeyUp, k1(13), none()},
    {PhysicalControlId::KeyPlus, k1(11), none()},
    {PhysicalControlId::KeySelect, k1(12), none()},
    {PhysicalControlId::KeyPage, k1(3), none()},
    {PhysicalControlId::KeyClear, k1(8), none()},
    {PhysicalControlId::KeyDown, k1(10), none()},
    {PhysicalControlId::KeyMinus, k1(9), none()},
#else
    // Original breadboard/prototype map. The XIAO PCB allocation above is
    // selected explicitly; never reinterpret the connected prototype wiring.
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
#endif
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
