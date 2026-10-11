// Portrait detail pages use the same setting/control callbacks as landscape.
// Geometry is explicit in LCD pixels; labels and values stack within each row.
void TransmitterUi::controlPortraitBank(uint8_t item, PageControlEvent)
{
    portraitResponseBank_ = item ? 1 : 0;
    drawThrottleResponsePage();
}

void TransmitterUi::drawPortraitDetails(UiPage page)
{
    if (page != activePage_ && page == UiPage::ThrottleResponse)
        portraitResponseBank_ = 0;
    activePage_ = page;
    startScreen();
    resetLvglObjects();
    beginPageFocus();
    auto note = [](const char *text, int y, uint32_t ink = cDim) {
        auto label = makeLabel(lv_scr_act(), text, 12, y, 12, ink);
        lv_obj_set_width(label, 296);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        return label;
    };
    auto action = [this](const char *text, int x, int y, int w,
                         PageControlHandler handler, uint8_t id) {
        return makePageActionButton(x, y, w, 42, text, 13,
            cPanel2, cCyan, cLine, handler, id);
    };
    auto row = [this](const char *name, const char *value, int y,
                      PageControlHandler handler, uint8_t id,
                      bool toggle = false, int height = 64) {
        auto panel = makePanel(8, y, 304, height, cBlack, cLine, 0);
        lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_BOTTOM, 0);
        auto label = makeLabel(panel, name, 10, 6, 12, cDim);
        boundedLabel(label, 192);
        auto valueLabel = makeLabel(panel, value, 10, 27, 14, cWhite);
        boundedLabel(valueLabel, 192);
        if (toggle) {
            makeButton(212, y + 10, 92, 42, "Change", 12);
            addPageActionTarget(8, y, 304, height, handler, id);
        } else {
            makeButton(212, y + (height-40)/2, 42, 40, "-", 20);
            makeButton(262, y + (height-40)/2, 42, 40, "+", 20);
            addPageEditorTarget(8, y, 304, height,
                212, y + (height-40)/2, 42, 40,
                262, y + (height-40)/2, 42, 40, handler, id);
        }
        return valueLabel;
    };
    char text[96];
    if (page == UiPage::ChannelRoles) {
        makeHeader("Channel roles", "Back");
        for (int i=0; i<2; ++i) {
            snprintf(text, sizeof(text), "CH%u", i ? channelRoleSettings_.throttleChannel : channelRoleSettings_.steeringChannel);
            row(i ? "Throttle" : "Steering", text, 64+i*76, &TransmitterUi::controlChannelRoles, i,false,76-kPanelGap);
        }
        row("Steering input", channelRoleSettings_.steeringEnabled ? "On" : "Off - safe neutral",
            216, &TransmitterUi::controlChannelRoles, 2, true);
        note("Changes force CUT. With steering off, its output stays at safe neutral. Re-arm with throttle centred.", 300, cAmber);
    } else if (page == UiPage::ChannelPlaceholder) {
        snprintf(text, sizeof(text), "Channel %u", selectedChannel_);
        makeHeader(text, "Back");
        const bool throttle = selectedChannel_ == channelRoleSettings_.throttleChannel;
        const bool proportional = channelHasProportionalControl(selectedChannel_);
        note(throttle ? "THROTTLE" : selectedChannel_ == channelRoleSettings_.steeringChannel ? "STEERING" : "AUXILIARY", 54, cCyan);
        for (uint8_t i=0; i<2; ++i) {
            auto kind = i ? ChannelParameterKind::Subtrim : ChannelParameterKind::Trim;
            int binding = bindingIndexForChannelParameter(selectedChannel_, kind);
            const char *control = binding >= 0 ? physicalControlDescriptor(static_cast<size_t>(bindings_[binding].input)).name : "None";
            snprintf(text, sizeof(text), "%s  %+.1f%%", control, channelParameterValue(selectedChannel_, kind)/10.0f);
            row(i ? "Subtrim control" : "Trim control", text, 82+i*76, &TransmitterUi::controlChannelConfig, i,false,76-kPanelGap);
        }
        action("Physical control / assignments", 8, 232, 304, &TransmitterUi::controlChannelConfig, 4);
        action("Mixing / switched rates",8,278,304,&TransmitterUi::controlChannelConfig,5);
        if (proportional) {
            action(throttle ? "Basic / calibration" : "Calibrate", 8, 324, 304, &TransmitterUi::controlChannelConfig, 2);
        }
        action("Response / ramp", 8, 376, 304, &TransmitterUi::controlChannelConfig, 3);
    } else if (page == UiPage::Channel2) {
        makeHeader("Throttle settings", "Back");
        const char *names[]={"Brake / reverse endpoint", "Profile subtrim", "Forward endpoint", "Direction"};
        for (int i=0; i<4; ++i) {
            if (i==0) snprintf(text,sizeof(text),"%u%%",throttleProfile().negativeEndpointPercent);
            if (i==1) snprintf(text,sizeof(text),"%+d%%",throttleProfile().subtrimPercent);
            if (i==2) snprintf(text,sizeof(text),"%u%%",throttleProfile().positiveEndpointPercent);
            if (i==3) snprintf(text,sizeof(text),"%s",throttleProfile().reversed?"Reverse":"Normal");
            channel2SettingValueLabels_[i]=row(names[i],text,54+i*70,&TransmitterUi::controlThrottleSetting,i,i==3);
        }
        action("Response",8,344,148,&TransmitterUi::controlThrottleHeader,0);
        action("Calibrate",164,344,148,&TransmitterUi::controlThrottleHeader,1);
        note("Release trigger before starting calibration.",388);
        makeLabel(lv_scr_act(),"Throttle input",12,410,12,cCyan);
        channel2ValueLabel_=makeLabel(lv_scr_act(),"0%",248,407,14,cGreen);
        channel2LiveBar_=makeBar(lv_scr_act(),12,434,296,10,-1000,1000,cGreen);
        lv_bar_set_value(channel2LiveBar_,0,LV_ANIM_OFF);
    } else if (page == UiPage::ThrottleResponse) {
        snprintf(text,sizeof(text),"CH%u response",selectedChannel_);
        makeHeader(text,"Back");
        drawThrottleResponseGraph();
        const auto &p=selectedResponseProfile();
        const bool throttle=selectedChannel_==channelRoleSettings_.throttleChannel;
        const bool steering=selectedChannel_==channelRoleSettings_.steeringChannel;
        const char *positive=throttle?"Forward":steering?"Right":"Positive";
        const char *negative=throttle?"Brake":steering?"Left":"Negative";
        const int values[]={p.subtrimPercent,p.positiveEndpointPercent,p.positiveRatePercent,p.positiveExpoPercent,p.negativeEndpointPercent,p.negativeRatePercent,p.negativeExpoPercent};
        for (int i=0; i<4; ++i) {
            int index=portraitResponseBank_*4+i;
            if(index>=7)break;
            char name[32];
            if(index==0)snprintf(name,sizeof(name),"Profile subtrim");
            else snprintf(name,sizeof(name),"%s %s",index<4?positive:negative,index==1||index==4?"endpoint":index==2||index==5?"rate":"expo");
            snprintf(text,sizeof(text),"%d%%",values[index]);
            row(name,text,214+i*49,&TransmitterUi::controlThrottleResponse,index,false,49-kPanelGap);
        }
        action("<",8,414,52,&TransmitterUi::controlPortraitBank,0);
        action("Ramp time",68,414,184,&TransmitterUi::controlThrottleHeader,0);
        action(">",260,414,52,&TransmitterUi::controlPortraitBank,1);
    } else if (page == UiPage::ChannelSpeed) {
        makeHeader("Output ramp", "Back");
        const auto &p=selectedResponseProfile();
        for(int i=0;i<2;++i){
            uint16_t value=i?p.negativeRampMs:p.positiveRampMs;
            snprintf(text,sizeof(text),value?"%u ms":"Direct",value);
            row(i?"Negative / brake side":"Positive / forward side",text,64+i*76,&TransmitterUi::controlChannelSpeed,i,false,76-kPanelGap);
        }
        note("0 ms means direct response. Ramp time is measured from neutral to the endpoint.",234);
    } else if (page == UiPage::DriveModes) {
        makeHeader("Drive mode", "Back");
        action(driveModeSettings_.activeMode==DriveMode::Walk?"WALK  *":"Walk",8,62,148,&TransmitterUi::controlDriveMode,0);
        action(driveModeSettings_.activeMode==DriveMode::Normal?"NORMAL  *":"Normal",164,62,148,&TransmitterUi::controlDriveMode,1);
        snprintf(text,sizeof(text),"%u%%",driveModeSettings_.walkLimitPercent);
        row("Walk forward limit",text,132,&TransmitterUi::controlDriveMode,2);
        note("Walk limits forward drive only. Brake / reverse remains at 100%.\n\nChanging mode forces CUT; release the trigger before arming.",222,cAmber);
    } else if (page == UiPage::Setup) {
        makeHeader("System settings", "Back");
        const char *names[]={"Buzzer", "Key clicks", "Pitch", "Click length"};
        for(int i=0;i<4;++i){
            if(i==0)snprintf(text,sizeof(text),"%s",systemSettings_.buzzerEnabled?"On":"Off");
            if(i==1)snprintf(text,sizeof(text),"%s",systemSettings_.keyClicksEnabled?"On":"Off");
            if(i==2)snprintf(text,sizeof(text),"%u Hz",systemSettings_.buzzerPitchHz);
            if(i==3)snprintf(text,sizeof(text),"%u ms",systemSettings_.keyClickDurationMs);
            row(names[i],text,54+i*62,&TransmitterUi::controlSetup,i,i<2,62-kPanelGap);
        }
        action("Test sound",8,308,148,&TransmitterUi::controlSetup,4);
        action("Display",164,308,148,&TransmitterUi::controlSetup,5);
        action("TX battery",8,357,148,&TransmitterUi::controlSetup,6);
        action("RX battery",164,357,148,&TransmitterUi::controlSetup,7);
        makePageActionButton(8,410,304,42,"Firmware info",12,cPanel2,cCyan,cLine,&TransmitterUi::controlSetup,8);
    } else if (page == UiPage::DisplaySetup) {
        makeHeader("Display", "Back");
        const char *names[]={"Screen timeout","Idle action","Brightness","Colour scheme","Activate now","Orientation"};
        const int first=displaySetupBank_*4;
        for(int i=first;i<min(first+4,6);++i){
            if(i==0){if(systemSettings_.screenTimeoutSeconds)snprintf(text,sizeof(text),"%u seconds",systemSettings_.screenTimeoutSeconds);else snprintf(text,sizeof(text),"Never");}
            if(i==1)snprintf(text,sizeof(text),"%s",systemSettings_.logoScreensaverEnabled?"Logo saver":"Display off");
            if(i==2)snprintf(text,sizeof(text),"%u%%",systemSettings_.displayBrightnessPercent);
            if(i==3)snprintf(text,sizeof(text),"%s",colourSchemeName(systemSettings_.nightMode,systemSettings_.darkColourScheme));
            if(i==5)snprintf(text,sizeof(text),"Portrait / rotate");
            if(i==4)snprintf(text,sizeof(text),"%s",systemSettings_.logoScreensaverEnabled?"Show logo":"Sleep");
            row(names[i],text,54+(i-first)*70,&TransmitterUi::controlDisplay,i,i!=0&&i!=2);
        }
        note("Logo saver dims to 25%. Display off saves the most power.",354);
        action("<",8,414,60,&TransmitterUi::controlDisplay,10);
        note(displaySetupBank_?"Display 2 / 2":"Display 1 / 2",444);
        // Centre pagination text between the two controls.
        lv_obj_t *last=lv_obj_get_child(lv_scr_act(),-1);lv_obj_set_pos(last,96,428);lv_obj_set_width(last,145);
        action(">",252,414,60,&TransmitterUi::controlDisplay,11);
    } else if(page==UiPage::TxBatterySetup || page==UiPage::RxBatterySetup) {
        const bool tx=page==UiPage::TxBatterySetup;
        makeHeader(tx?"TX battery":"RX battery","Back");
        const char *names[]={"Chemistry","Cell count","Low alarm",tx?"Voltage sensing":"Alarm enabled"};
        for(int i=0;i<4;++i){
            if(i==0)snprintf(text,sizeof(text),"%s",batteryChemistryName(tx?txBatterySettings_.chemistry:rxBatterySettings_.chemistry));
            if(i==1)snprintf(text,sizeof(text),"%uS",tx?txBatterySettings_.cellCount:rxBatterySettings_.cellCount);
            if(i==2){auto v=tx?txBatterySettings_.lowAlarmDecivolts:rxBatterySettings_.lowAlarmDecivolts;snprintf(text,sizeof(text),"%u.%u V",v/10,v%10);}
            if(i==3)snprintf(text,sizeof(text),"%s",(tx?txBatterySettings_.voltageSenseEnabled:rxBatterySettings_.alarmEnabled)?"On":"Off");
            row(names[i],text,54+i*70,tx?&TransmitterUi::controlTxBattery:&TransmitterUi::controlRxBattery,i,i==0||i==3);
        }
        note(tx?"Battery voltage uses the board's ADC divider.":"Voltage is supplied by receiver telemetry.",354);
    }
    finishPageFocus();
}
