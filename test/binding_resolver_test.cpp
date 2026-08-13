#include "binding_resolver.h"

#include <assert.h>
#include <stdio.h>

namespace {

PhysicalInputEvent event(PhysicalControlId input, InputEvent kind,
                         uint32_t timestampMs, int16_t value = 0)
{
    PhysicalInputEvent result;
    result.input = input;
    result.event = kind;
    result.timestampMs = timestampMs;
    result.value = value;
    return result;
}

ControlBinding channelBinding(ControlBehaviour behaviour, InputEvent trigger,
                              uint8_t channel)
{
    ControlBinding binding;
    binding.input = PhysicalControlId::TimerButton;
    binding.event = trigger;
    binding.target = AssignmentTarget::CrsfChannel;
    binding.behaviour = behaviour;
    binding.crsfChannel = channel;
    return binding;
}

void testMapping()
{
    assert(crsfValueFromPermille(-1000) == CRSF_CHANNEL_MIN);
    assert(crsfValueFromPermille(0) == CRSF_CHANNEL_MID);
    assert(crsfValueFromPermille(1000) == CRSF_CHANNEL_MAX);
    assert(crsfValueFromPermille(-2000) == CRSF_CHANNEL_MIN);
    assert(crsfValueFromPermille(2000) == CRSF_CHANNEL_MAX);
}

void testRejectsConflictingChannelWriters()
{
    ControlBinding bindings[2] = {
        channelBinding(ControlBehaviour::Toggle, InputEvent::ShortPress, 5),
        channelBinding(ControlBehaviour::Momentary, InputEvent::Pressed, 5),
    };
    BindingResolver resolver;
    BindingLoadResult result = resolver.setBindings(bindings, 2);
    assert(result.error == BindingLoadError::ConflictingChannelWriter);
    assert(result.bindingIndex == 1);
    assert(result.conflictingIndex == 0);

    bindings[1].crsfChannel = 6;
    result = resolver.setBindings(bindings, 2);
    assert(result.ok());
}

void testMomentaryToggleAndReservedChannel()
{
    ControlBinding bindings[2] = {
        channelBinding(ControlBehaviour::Momentary, InputEvent::Pressed, 5),
        channelBinding(ControlBehaviour::Toggle, InputEvent::ShortPress, 6),
    };
    BindingResolver resolver;
    assert(resolver.setBindings(bindings, 2).ok());
    ResolvedBindingActionBuffer actions;

    resolver.processEvent(event(PhysicalControlId::TimerButton,
                                InputEvent::Pressed, 10),
                          ContextDashboard, actions);
    assert(resolver.value(0) == 1000);
    resolver.processEvent(event(PhysicalControlId::TimerButton,
                                InputEvent::Released, 20),
                          ContextMenu, actions); // release survives context change
    assert(resolver.value(0) == 0);

    resolver.processEvent(event(PhysicalControlId::TimerButton,
                                InputEvent::ShortPress, 30),
                          ContextDashboard, actions);
    assert(resolver.value(1) == 1000);

    CrsfChannelOutputModel model;
    resolver.applyChannelOutputs(model, static_cast<uint16_t>(1U << 5));
    assert(model.enabled(5));
    assert(model.requested(5) == CRSF_CHANNEL_MID);
    assert(!model.enabled(6));
}

void testSteppedTrimAndThreePosition()
{
    ControlBinding bindings[2] = {
        channelBinding(ControlBehaviour::Stepped,
                       InputEvent::PositivePress, 7),
        channelBinding(ControlBehaviour::SpringReturnThreePosition,
                       InputEvent::PositivePress, 8),
    };
    bindings[0].input = PhysicalControlId::GripTrimA;
    bindings[0].minimum = -200;
    bindings[0].maximum = 200;
    bindings[0].step = 50;
    bindings[1].input = PhysicalControlId::Aux3Trim;

    BindingResolver resolver;
    assert(resolver.setBindings(bindings, 2).ok());
    ResolvedBindingActionBuffer actions;
    resolver.processEvent(event(PhysicalControlId::GripTrimA,
                                InputEvent::PositivePress, 1),
                          ContextDashboard, actions);
    resolver.processEvent(event(PhysicalControlId::GripTrimA,
                                InputEvent::PositiveRepeat, 2),
                          ContextDashboard, actions);
    assert(resolver.value(0) == 100);
    resolver.processEvent(event(PhysicalControlId::GripTrimA,
                                InputEvent::NegativePress, 3),
                          ContextDashboard, actions);
    assert(resolver.value(0) == 50);

    resolver.processEvent(event(PhysicalControlId::Aux3Trim,
                                InputEvent::NegativePress, 4),
                          ContextDashboard, actions);
    assert(resolver.value(1) == -1000);
    resolver.processEvent(event(PhysicalControlId::Aux3Trim,
                                InputEvent::NegativeRelease, 5),
                          ContextDashboard, actions);
    assert(resolver.value(1) == 0);
}

void testChannelParameterActionsAndConflicts()
{
    ControlBinding trim;
    trim.input = PhysicalControlId::GripTrimA;
    trim.event = InputEvent::PositivePress;
    trim.target = AssignmentTarget::ModelParameter;
    trim.targetId = encodeChannelParameterTarget(2, ChannelParameterKind::Subtrim);
    trim.behaviour = ControlBehaviour::Stepped;
    trim.crsfChannel = 0;
    trim.minimum = -200;
    trim.maximum = 200;
    trim.step = 10;
    trim.initial = 30;
    trim.failsafe = 30;

    BindingResolver resolver;
    assert(resolver.setBindings(&trim, 1).ok());
    ResolvedBindingActionBuffer actions;
    resolver.processEvent(event(PhysicalControlId::GripTrimA,
                                InputEvent::PositivePress, 1),
                          ContextDashboard, actions);
    assert(actions.size() == 1);
    assert(actions[0].target == AssignmentTarget::ModelParameter);
    assert(channelParameterTargetChannel(actions[0].targetId) == 2);
    assert(channelParameterTargetKind(actions[0].targetId) ==
           ChannelParameterKind::Subtrim);
    assert(actions[0].value == 40);

    ControlBinding duplicate = trim;
    duplicate.input = PhysicalControlId::GripTrimB;
    ControlBinding bindings[] = {trim, duplicate};
    BindingLoadResult result = resolver.setBindings(bindings, 2);
    assert(result.error == BindingLoadError::ConflictingParameterWriter);

    duplicate.targetId = encodeChannelParameterTarget(1,
                                                       ChannelParameterKind::Subtrim);
    assert(resolver.setBindings(&duplicate, 1).ok());
    duplicate.targetId = encodeChannelParameterTarget(17,
                                                       ChannelParameterKind::Trim);
    result = resolver.setBindings(&duplicate, 1);
    assert(result.error == BindingLoadError::InvalidBinding);
}

void testPulseExpiryAndLocalAction()
{
    ControlBinding pulse = channelBinding(ControlBehaviour::Pulse,
                                           InputEvent::ShortPress, 9);
    pulse.pulseMs = 100;
    ControlBinding local;
    local.input = PhysicalControlId::KeyMode;
    local.event = InputEvent::ShortPress;
    local.target = AssignmentTarget::TransmitterFunction;
    local.targetId = static_cast<uint16_t>(TransmitterFunction::OpenMenu);
    local.behaviour = ControlBehaviour::Toggle;
    ControlBinding bindings[] = {pulse, local};

    BindingResolver resolver;
    assert(resolver.setBindings(bindings, 2).ok());
    ResolvedBindingActionBuffer actions;
    resolver.processEvent(event(PhysicalControlId::TimerButton,
                                InputEvent::ShortPress, 0xfffffff0U),
                          ContextDashboard, actions);
    assert(resolver.value(0) == 1000);
    resolver.update(0x20U, actions);
    assert(resolver.value(0) == 1000);
    resolver.update(0x60U, actions);
    assert(resolver.value(0) == 0);

    resolver.processEvent(event(PhysicalControlId::KeyMode,
                                InputEvent::ShortPress, 200),
                          ContextDashboard, actions);
    assert(actions.size() == 1);
    assert(actions[0].target == AssignmentTarget::TransmitterFunction);
    assert(actions[0].targetId ==
           static_cast<uint16_t>(TransmitterFunction::OpenMenu));
    assert(actions[0].value == 1000);
}

void testProportionalAndFailsafe()
{
    ControlBinding binding = channelBinding(ControlBehaviour::Proportional,
                                             InputEvent::ValueChanged, 10);
    binding.minimum = -500;
    binding.maximum = 500;
    binding.failsafe = -500;
    BindingResolver resolver;
    assert(resolver.setBindings(&binding, 1).ok());
    ResolvedBindingActionBuffer actions;
    resolver.processEvent(event(PhysicalControlId::TimerButton,
                                InputEvent::ValueChanged, 1, 1000),
                          ContextArmed, actions);
    assert(resolver.value(0) == 500);

    CrsfChannelOutputModel model;
    resolver.applyChannelOutputs(model);
    assert(model.requested(10) == crsfValueFromPermille(500));
    assert(model.safe(10) == crsfValueFromPermille(-500));
}

} // namespace

int main()
{
    testMapping();
    testRejectsConflictingChannelWriters();
    testMomentaryToggleAndReservedChannel();
    testSteppedTrimAndThreePosition();
    testChannelParameterActionsAndConflicts();
    testPulseExpiryAndLocalAction();
    testProportionalAndFailsafe();
    puts("binding_resolver_test: PASS");
    return 0;
}
