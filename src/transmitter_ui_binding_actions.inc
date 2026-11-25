namespace {
enum class BindingEditorAction : uint8_t {
    Enable, Target, TargetValue, Mode, Context, Cancel, Remove, Apply,
    AssignTab, ModeTab, RangeTab, StateTab, Minimum, Maximum, Step, Initial, Failsafe, Inverted,
    ReadOnly = 255
};
}

void TransmitterUi::controlBindingEditor(uint8_t field, PageControlEvent event)
{
    const auto action = static_cast<BindingEditorAction>(field);
    const int8_t direction = event == PageControlEvent::Decrease ? -1 : 1;
    if (action == BindingEditorAction::Cancel) {
        playKeyClick();
        drawControlAssignmentsPage();
        return;
    }
    if (action == BindingEditorAction::Remove) {
        if (bindingIndexForInput(selectedBindingInput_) >= 0 && removeSelectedBinding()) {
            playBuzzer(80);
            drawControlAssignmentsPage();
        }
        return;
    }
    if (action == BindingEditorAction::Apply) {
        if (saveBindingDraft()) {
            playBuzzer(80);
            drawControlAssignmentsPage();
        } else {
            playBuzzer(180);
            drawBindingEditorPage();
        }
        return;
    }
    if (action >= BindingEditorAction::AssignTab && action <= BindingEditorAction::StateTab) {
        bindingEditorBank_ = field - static_cast<uint8_t>(BindingEditorAction::AssignTab);
        playKeyClick();
        drawBindingEditorPage();
        return;
    }

    const bool editableMode = bindingDraft_.target == AssignmentTarget::CrsfChannel ||
                              bindingDraft_.target == AssignmentTarget::ModelParameter;
    if (action == BindingEditorAction::Mode && !editableMode) return;
    if (action >= BindingEditorAction::Minimum && action <= BindingEditorAction::Inverted) {
        if (!editableMode) return;
        const bool modelParameter = bindingDraft_.target == AssignmentTarget::ModelParameter;
        const int limit = modelParameter ? 200 : 1000;
        const int delta = direction * 10;
        switch (action) {
            case BindingEditorAction::Minimum:
                bindingDraft_.minimum = constrain(int(bindingDraft_.minimum) + delta, -limit, int(bindingDraft_.maximum));
                break;
            case BindingEditorAction::Maximum:
                bindingDraft_.maximum = constrain(int(bindingDraft_.maximum) + delta, int(bindingDraft_.minimum), limit);
                break;
            case BindingEditorAction::Step:
                bindingDraft_.step = constrain(int(bindingDraft_.step) + delta, 1, 2 * limit);
                break;
            case BindingEditorAction::Initial:
                if (!modelParameter) bindingDraft_.initial = constrain(int(bindingDraft_.initial) + delta,
                    int(bindingDraft_.minimum), int(bindingDraft_.maximum));
                break;
            case BindingEditorAction::Failsafe:
                if (!modelParameter) bindingDraft_.failsafe = constrain(int(bindingDraft_.failsafe) + delta,
                    int(bindingDraft_.minimum), int(bindingDraft_.maximum));
                break;
            case BindingEditorAction::Inverted:
                bindingDraft_.inverted = !bindingDraft_.inverted;
                break;
            default: return;
        }
        bindingError_[0] = '\0';
        playKeyClick();
        drawBindingEditorPage();
        return;
    }

    const PhysicalControlKind kind = physicalControlDescriptor(
        static_cast<size_t>(selectedBindingInput_)).kind;
    const bool parameter = bindingDraft_.target == AssignmentTarget::ModelParameter &&
        isChannelParameterTarget(bindingDraft_.targetId);
    if (action == BindingEditorAction::Enable) {
        bindingDraft_.enabled = !bindingDraft_.enabled;
    } else if (action == BindingEditorAction::Target) {
        const AssignmentTarget next = adjacentBindingTarget(
            kind, bindingDraft_.target, direction);
        if (next == AssignmentTarget::CrsfChannel) {
            bindingDraft_.target = AssignmentTarget::CrsfChannel;
            bindingDraft_.targetId = 0;
            bindingDraft_.crsfChannel = assignmentChannelContext_ ? assignmentChannelContext_ : adjacentBindingChannel(2, 1);
            bindingDraft_.behaviour = kind == PhysicalControlKind::MaintainedSwitch
                ? ControlBehaviour::TwoPosition
                : kind == PhysicalControlKind::SpringCentredTrim
                ? ControlBehaviour::SpringReturnThreePosition
                : ControlBehaviour::Momentary;
            bindingDraft_.event = kind == PhysicalControlKind::SpringCentredTrim
                ? InputEvent::PositivePress : InputEvent::Pressed;
            bindingDraft_.minimum = -1000;
            bindingDraft_.maximum = 1000;
            bindingDraft_.step = 50;
            bindingDraft_.initial = 0;
            bindingDraft_.failsafe = 0;
            bindingDraft_.persistent = false;
        } else if (next == AssignmentTarget::ModelParameter) {
            const uint8_t channel = selectedBindingInput_ ==
                    PhysicalControlId::ThrottleTrim
                ? channelRoleSettings_.throttleChannel
                : selectedBindingInput_ == PhysicalControlId::Aux3Trim
                ? static_cast<uint8_t>(3)
                : channelRoleSettings_.steeringChannel;
            bindingDraft_.target = AssignmentTarget::ModelParameter;
            bindingDraft_.targetId = encodeChannelParameterTarget(
                channel, ChannelParameterKind::Trim);
            bindingDraft_.crsfChannel = 0;
            bindingDraft_.behaviour = ControlBehaviour::Stepped;
            bindingDraft_.event = InputEvent::PositivePress;
            bindingDraft_.minimum = -kChannelParameterLimitPermille;
            bindingDraft_.maximum = kChannelParameterLimitPermille;
            bindingDraft_.step = 10;
            bindingDraft_.initial = channelParameterValue(
                channel, ChannelParameterKind::Trim);
            bindingDraft_.failsafe = bindingDraft_.initial;
            bindingDraft_.persistent = true;
        } else if (next == AssignmentTarget::TransmitterFunction) {
            bindingDraft_.target = next;
            bindingDraft_.targetId = static_cast<uint16_t>(
                TransmitterFunction::OpenMenu);
            bindingDraft_.crsfChannel = 0;
            bindingDraft_.behaviour = kind == PhysicalControlKind::MaintainedSwitch
                ? ControlBehaviour::TwoPosition : ControlBehaviour::Momentary;
            bindingDraft_.event = InputEvent::Pressed;
            bindingDraft_.minimum = -1000;
            bindingDraft_.maximum = 1000;
            bindingDraft_.step = 50;
            bindingDraft_.initial = -1000;
            bindingDraft_.failsafe = -1000;
            bindingDraft_.persistent = false;
        } else if (next == AssignmentTarget::DriveMode) {
            bindingDraft_.target = next;
            bindingDraft_.targetId = static_cast<uint16_t>(
                DriveModeSelection::WalkNormal);
            bindingDraft_.crsfChannel = 0;
            bindingDraft_.behaviour = ControlBehaviour::TwoPosition;
            bindingDraft_.event = InputEvent::Pressed;
            bindingDraft_.minimum = -1000;
            bindingDraft_.maximum = 1000;
            bindingDraft_.step = 50;
            bindingDraft_.initial = -1000;
            bindingDraft_.failsafe = -1000;
            bindingDraft_.persistent = false;
        }
    } else if (action == BindingEditorAction::TargetValue) {
        if (parameter) {
            uint8_t channel = channelParameterTargetChannel(bindingDraft_.targetId);
            channel = static_cast<uint8_t>(
                ((static_cast<int>(channel) - 1 + direction + 16) % 16) + 1);
            const ChannelParameterKind parameterKind = channelParameterTargetKind(
                bindingDraft_.targetId);
            bindingDraft_.targetId = encodeChannelParameterTarget(channel,
                                                                   parameterKind);
            bindingDraft_.initial = channelParameterValue(channel, parameterKind);
            bindingDraft_.failsafe = bindingDraft_.initial;
        } else if (bindingDraft_.target == AssignmentTarget::TransmitterFunction) {
            size_t selected = 0;
            for (size_t index = 0;
                 index < sizeof(kEditableTransmitterFunctions) /
                             sizeof(kEditableTransmitterFunctions[0]); ++index)
                if (static_cast<uint16_t>(kEditableTransmitterFunctions[index]) ==
                    bindingDraft_.targetId) selected = index;
            const size_t count = sizeof(kEditableTransmitterFunctions) /
                                 sizeof(kEditableTransmitterFunctions[0]);
            selected = static_cast<size_t>((static_cast<int>(selected) + direction +
                                            static_cast<int>(count)) %
                                           static_cast<int>(count));
            bindingDraft_.targetId = static_cast<uint16_t>(
                kEditableTransmitterFunctions[selected]);
            if (kind == PhysicalControlKind::MaintainedSwitch) {
                bindingDraft_.behaviour = ControlBehaviour::TwoPosition;
            } else {
                bindingDraft_.behaviour =
                    (kEditableTransmitterFunctions[selected] ==
                         TransmitterFunction::DisplayOnOff ||
                     kEditableTransmitterFunctions[selected] ==
                         TransmitterFunction::ArmCut)
                    ? ControlBehaviour::Toggle : ControlBehaviour::Momentary;
            }
        } else if (bindingDraft_.target == AssignmentTarget::DriveMode) {
            size_t selected = 0;
            for (size_t index = 0;
                 index < sizeof(kEditableDriveModeSelections) /
                             sizeof(kEditableDriveModeSelections[0]); ++index)
                if (static_cast<uint16_t>(kEditableDriveModeSelections[index]) ==
                    bindingDraft_.targetId) selected = index;
            const size_t count = sizeof(kEditableDriveModeSelections) /
                                 sizeof(kEditableDriveModeSelections[0]);
            selected = static_cast<size_t>((static_cast<int>(selected) + direction +
                                            static_cast<int>(count)) %
                                           static_cast<int>(count));
            bindingDraft_.targetId = static_cast<uint16_t>(
                kEditableDriveModeSelections[selected]);
        } else {
            bindingDraft_.crsfChannel = adjacentBindingChannel(
                bindingDraft_.crsfChannel, direction);
        }
    } else if (action == BindingEditorAction::Mode) {
        if (parameter) {
            const uint8_t channel = channelParameterTargetChannel(
                bindingDraft_.targetId);
            const ChannelParameterKind next =
                channelParameterTargetKind(bindingDraft_.targetId) ==
                        ChannelParameterKind::Trim
                    ? ChannelParameterKind::Subtrim : ChannelParameterKind::Trim;
            bindingDraft_.targetId = encodeChannelParameterTarget(channel, next);
            bindingDraft_.initial = channelParameterValue(channel, next);
            bindingDraft_.failsafe = bindingDraft_.initial;
        } else {
            bindingDraft_.behaviour = adjacentBindingBehaviour(
                kind, bindingDraft_.behaviour, direction);
        }
    } else if (action == BindingEditorAction::Context) {
        bindingDraft_.contexts = adjacentControlContext(
            bindingDraft_.contexts, direction);
    } else return;
    bindingError_[0] = '\0';
    playKeyClick();
    drawBindingEditorPage();
}
