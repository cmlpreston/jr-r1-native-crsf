namespace {
enum ReadinessRow {
    TriggerPosition, ThrottleMix, ReceiverLink, SwitchPositions,
    ModelSettings, ControlInputs, StartupCheck, ThrottleChannel,
    ReadinessRowCount
};

const char *switchPosition(const PhysicalInputState &state)
{
    if (!state.mapped) return "--";
    if (state.faulted) return "fault";
    return state.stablePrimary ? "on" : "off";
}
}

void TransmitterUi::drawOutputReadiness()
{
    auto *screen = lv_scr_act();
    constexpr int inset = UiLayout::PageInset;
    const int width = kScreenWidth - 2 * inset;
    makeLabel(screen,"Output status",inset,44,UiText::Heading,cWhite);
    const int stateX = portraitUi ? inset : kScreenWidth / 2;
    designDetails_[0] = makeLabel(screen,"",stateX,portraitUi?69:49,UiText::Menu,cCyan);
    boundedLabel(designDetails_[0],kScreenWidth-inset-stateX);
    if (!portraitUi) lv_obj_set_style_text_align(designDetails_[0],LV_TEXT_ALIGN_RIGHT,0);
    designDetails_[1] = makeLabel(screen,"",inset,portraitUi?91:73,UiText::Secondary,cWhite);
    lv_obj_set_width(designDetails_[1],width);
    lv_label_set_long_mode(designDetails_[1],LV_LABEL_LONG_WRAP);
    const char *names[] = {
        "Throttle trigger", "Throttle mix", "Receiver link", "Idle Up / Drive",
        "Model settings", "Control inputs", "Startup check", "Throttle channel",
    };
    static_assert(sizeof(names)/sizeof(*names)==ReadinessRowCount,"One label per readiness row");
    const int columnWidth = portraitUi ? width : (width-UiLayout::Gap)/2;
    for (int index = 0; index < ReadinessRowCount; ++index) {
        const int x = inset + (portraitUi ? 0 : (index/4)*(columnWidth+UiLayout::Gap));
        const int y = portraitUi ? 124 + index*14 : 105 + (index%4)*18;
        makeLabel(screen,names[index],x,y,UiText::Caption,cDim);
        auto *value = makeLabel(screen,"--",x+100,y,UiText::Caption,cWhite);
        boundedLabel(value,columnWidth-100);
        lv_obj_set_style_text_align(value,LV_TEXT_ALIGN_RIGHT,0);
        designValues_[index] = value;
    }
}

void TransmitterUi::updateOutputReadiness(OutputSafetyStatus safety, uint32_t now)
{
    const auto &checks = outputReadiness_;
    auto show = [&](ReadinessRow row, const char *value, uint32_t tint) {
        uiSetText(designValues_[row],value);
        uiSetTextColor(designValues_[row],colour(tint),0);
    };
    auto showCheck = [&](ReadinessRow row, bool good, const char *pass, const char *fail) {
        show(row,good?pass:fail,good?cGreen:cAmber);
    };
    if (safety == OutputSafetyStatus::ControlStale || !outputReadinessSampledMs_ ||
        now-outputReadinessSampledMs_ > CONTROL_STALE_TIMEOUT_US/1000) {
        for (int index = 0; index < ReadinessRowCount; ++index) show(ReadinessRow(index),"--",cDim);
        return;
    }

    char value[32];
    const char *trigger = "Input unavailable";
    if (checks.triggerValid) {
        if (checks.triggerCentred) trigger = "Centred";
        else if (!checks.triggerPermille) trigger = "Outside centre";
        else {
            const int position = checks.triggerPermille;
            snprintf(value,sizeof(value),"%s%d.%d%% / centre",position<0?"-":"+",abs(position)/10,abs(position)%10);
            trigger = value;
        }
    }
    show(TriggerPosition,trigger,checks.triggerValid&&checks.triggerCentred?cGreen:cAmber);
    if (!checks.throttleMixValid) show(ThrottleMix,"Input unavailable",cAmber);
    else if (!checks.throttleMixActive) show(ThrottleMix,"Base trigger",cDim);
    else {
        const int position = checks.throttleMixPermille;
        snprintf(value,sizeof(value),"%s%d.%d%% / +/-5%%",position<0?"-":"+",abs(position)/10,abs(position)%10);
        show(ThrottleMix,value,checks.throttleMixNeutral?cGreen:cAmber);
    }
    if (checks.linkValid)
        snprintf(value,sizeof(value),"LQ %u / min %u",unsigned(checks.linkQuality),unsigned(checks.minimumLinkQuality));
    else
        snprintf(value,sizeof(value),"No LQ / min %u",unsigned(checks.minimumLinkQuality));
    show(ReceiverLink,value,checks.linkMeetsMinimum?cGreen:cAmber);

    const auto &idleUp = physicalInputStates_[static_cast<size_t>(PhysicalControlId::IdleUpSwitch)];
    const auto &driveMode = physicalInputStates_[static_cast<size_t>(PhysicalControlId::DriveModeSwitch)];
    if (checks.physicalInputsHealthy) {
        snprintf(value,sizeof(value),"%s / %s",switchPosition(idleUp),switchPosition(driveMode));
        show(SwitchPositions,value,idleUp.faulted||driveMode.faulted?cAmber:cDim);
    } else {
        show(SwitchPositions,"Unavailable",cAmber);
    }
    showCheck(ModelSettings,checks.configurationValid,"Valid","Check model");
    showCheck(ControlInputs,checks.inputsValid,"Valid","Check inputs");
    showCheck(StartupCheck,checks.startupComplete,"Passed","Pending");
    snprintf(value,sizeof(value),"CH%u %s",unsigned(checks.throttleChannel),checks.throttleEnabled?"enabled":"disabled");
    show(ThrottleChannel,value,checks.throttleEnabled?cGreen:cAmber);
}

void TransmitterUi::openOutputInputMonitor(uint8_t, PageControlEvent event)
{
    if (event != PageControlEvent::Activate) return;
    inputMonitorBank_ = 0;
    for (size_t index = 0; index < kPhysicalControlCount; ++index) {
        if (physicalInputStates_[index].faulted) {
            inputMonitorBank_ = index/5;
            break;
        }
    }
    inputMonitorReturnPage_ = UiPage::OutputStatus;
    drawInputMonitorPage();
}
