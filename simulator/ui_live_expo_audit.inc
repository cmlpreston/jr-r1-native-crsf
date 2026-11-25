// Included inside runSimulatorUiAudit: exercise the real touch/keypad editor,
// immutable control snapshots and production engine, using disposable storage.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;bindingCount_=0;
channelRoleSettings_.steeringChannel=1;channelRoleSettings_.throttleChannel=2;
channelRoleSettings_.steeringEnabled=channelRoleSettings_.throttleEnabled=true;
const auto expoCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
uint32_t expoKeyTime=millis(),expoRevision=1,expoCycleTime=1000;
auto expoTap=[&](PageControlHandler handler,uint8_t argument){
    lv_obj_update_layout(lv_scr_act());
    for(uint8_t n=0;n<pageFocusCount_;++n){const auto target=pageFocusTargets_[n];
        if(target.handler==handler&&target.argument==argument){
            handleTouch(target.activate.x+target.activate.width/2,target.activate.y+target.activate.height/2);return true;
        }
    }
    return false;
};
auto expoKey=[&](PhysicalControlId key,bool held=false){
    assert(handleKeypadEvent(key,InputEvent::Pressed,expoKeyTime));
    if(held)updateKeypad(expoKeyTime+700);
    assert(handleKeypadEvent(key,InputEvent::Released,expoKeyTime+(held?800:80)));
    expoKeyTime+=held?900:150;
};
auto expoConfiguration=[&](){
    auto configuration=controlConfigurationSnapshot();configuration.revision=expoRevision++;
    // Remove filtering from this fixture so each accepted edit can be compared
    // with the very next real control cycle; UI-owned calibration is untouched.
    for(auto *profile:{&configuration.throttleInput,&configuration.steeringInput}){
        profile->calibration={1000,2000,3000};profile->deadbandAdc=0;
        profile->filterShift=0;profile->validMinimumAdc=100;profile->validMaximumAdc=4090;
    }
    return configuration;
};
auto expoValues=[&](int positive,int negative){
    const auto configuration=controlConfigurationSnapshot();
    const auto &profile=configuration.channelProfiles[selectedChannel_-1];
    assert(profile.positiveExpoPercent==positive&&profile.negativeExpoPercent==negative);
};
auto expoCurve=[&](const ChannelResponseProfile &profile){
    const int graphHeight=portraitUi?150:60;
    for(unsigned point:{0,8,16,24,32}){
        const int raw=-1000+int(point)*2000/32;
        const int output=applyChannelResponse(profile,raw);
        assert(designPoints_[point].y==graphHeight/2-output*(graphHeight-8)/2400);
    }
};
for(unsigned orientation=0;orientation<2;++orientation){
    for(bool reversed:{false,true})for(uint8_t role:{0,1,2,3,4}){
        tuneRole_=role;tuneBank_=0;selectedChannel_=role==0?steeringChannel():role<3?throttleChannel():3;
        for(uint8_t ch=1;ch<=CRSF_CHANNEL_COUNT;++ch){
            channelModel_.parameters[ch-1]=ChannelParameterValues{};
            channelModel_.mixing[ch-1]=ChannelMixSettings{};
            channelModel_.setResponse(ch,ChannelResponseProfile{});
        }
        ChannelResponseProfile original;
        original.positiveExpoPercent=30;original.negativeExpoPercent=-20;
        original.positiveEndpointPercent=90;original.negativeEndpointPercent=70;
        original.reversed=reversed;channelModel_.setResponse(selectedChannel_,original);
        drawDesignPage(UiPage::Tune);armRequested_=true;safetyResetRequested_=false;
        settingsSavePending_=channelParameterSavePending_=false;
        const auto cuts=auditCutRequests;
        openDesignEditor(1,UiPage::Tune);
        assert(liveExpoEditor()&&!expoPreviewActive_);
        expoValues(30,-20);expoCurve(original); // Merely entering must retain asymmetric saved sides.
        assert(expoTap(&TransmitterUi::controlDesign,32)); // Fine changes step, not response.
        assert(!expoPreviewActive_);expoValues(30,-20);

        ControlEngine engine(ControlSafetyPolicy{80,0,0,50});
        ControlCycleInput cycle;cycle.startupComplete=cycle.linkValid=true;cycle.linkQuality=100;
        auto sample=[&](const ControlConfigurationSnapshot &configuration,int raw){
            cycle.nowMs=expoCycleTime;expoCycleTime+=4;
            cycle.throttleAdc=cycle.steeringAdc=2000;cycle.bindings=CrsfChannelOutputModel{};
            if(role==0)cycle.steeringAdc=2000+raw;
            else if(role<3)cycle.throttleAdc=2000+raw;
            else cycle.bindings.configure(selectedChannel_,crsfValueFromPermille(raw),CRSF_CHANNEL_MID,true);
            const auto result=engine.update(configuration,cycle);
            assert(result.safety==OutputSafetyStatus::Armed);
            return result.outputs.requested(selectedChannel_);
        };
        auto baseline=expoConfiguration();sample(baseline,0);
        const int half=(role==2||role==4)?-500:500;
        const auto centre=sample(baseline,0),left=sample(baseline,-1000),right=sample(baseline,1000);
        const auto halfBefore=sample(baseline,half);
        const int draft=editorDraft_;
        assert(expoTap(&TransmitterUi::controlDesign,31));
        assert(editorDraft_==draft+1&&expoPreviewActive_);
        expoValues(role==2||role==4?30:draft+1,role==0||role==2||role==4?draft+1:-20);
        auto preview=expoConfiguration();
        expoCurve(preview.channelProfiles[selectedChannel_-1]);
        assert(sample(preview,half)!=halfBefore);
        assert(sample(preview,0)==centre&&sample(preview,-1000)==left&&sample(preview,1000)==right);
        assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==cuts);
        assert(profileForChannel(selectedChannel_).positiveExpoPercent==30&&profileForChannel(selectedChannel_).negativeExpoPercent==-20);

        expoKey(PhysicalControlId::KeyMinus);assert(editorDraft_==draft);
        expoValues(role==2||role==4?30:draft,role==0||role==2||role==4?draft:-20);
        assert(expoTap(&TransmitterUi::controlDesign,33));assert(editorDraft_==0&&expoPreviewActive_);
        expoValues(role==2||role==4?30:0,role==0||role==2||role==4?0:-20);
        // The keypad's confirmed Reset must activate the same live preview.
        expoKey(PhysicalControlId::KeyPlus);expoKey(PhysicalControlId::KeyClear,true);
        assert(activePage_==UiPage::FieldResetConfirmation);
        assert(expoTap(&TransmitterUi::controlKeypadMenu,1));
        assert(activePage_==UiPage::ValueEditor&&editorDraft_==0&&expoPreviewActive_);
        expoValues(role==2||role==4?30:0,role==0||role==2||role==4?0:-20);
        if(const char *prefix=getenv("JR_R1_LIVE_EXPO_SCREENSHOTS")){
            lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];
            snprintf(path,sizeof(path),"%s-%s-%u-%u.bmp",prefix,portraitUi?"portrait":"landscape",role,unsigned(reversed));
            simulatorSaveScreenshot(path);
        }
        assert(expoTap(&TransmitterUi::controlDesign,34));assert(activePage_==UiPage::Tune);
        expoValues(30,-20);auto restored=expoConfiguration();assert(sample(restored,half)==halfBefore);
        assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==cuts&&!settingsSavePending_);

        openDesignEditor(1,UiPage::Tune);assert(!expoPreviewActive_);
        expoKey(PhysicalControlId::KeyClear,true);
        assert(activePage_==UiPage::FieldResetConfirmation);
        assert(expoTap(&TransmitterUi::controlKeypadMenu,1));
        assert(expoPreviewActive_&&editorDraft_==0);
        expoValues(role==2||role==4?30:0,role==0||role==2||role==4?0:-20);
        assert(expoTap(&TransmitterUi::controlDesign,34));expoValues(30,-20);

        openDesignEditor(1,UiPage::Tune);expoKey(PhysicalControlId::KeyPlus);
        navigateBack();assert(activePage_==UiPage::Tune);expoValues(30,-20);
        openDesignEditor(1,UiPage::Tune);expoKey(PhysicalControlId::KeyPlus);
        drawDashboard();expoValues(30,-20);assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==cuts);

        // Apply without a first edit cannot silently make steering asymmetric
        // expo symmetric. Deliberate edits may set both steering sides together.
        openDesignEditor(1,UiPage::Tune);
        assert(expoTap(&TransmitterUi::controlDesign,35));expoValues(30,-20);
        openDesignEditor(1,UiPage::Tune);expoKey(PhysicalControlId::KeyPlus);
        const int committed=editorDraft_;
        assert(expoTap(&TransmitterUi::controlDesign,35));assert(activePage_==UiPage::Tune);
        expoValues(role==2||role==4?30:committed,role==0||role==2||role==4?committed:-20);
        assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==cuts&&settingsSavePending_);

        armRequested_=false;openDesignEditor(1,UiPage::Tune);expoKey(PhysicalControlId::KeyMinus);
        assert(!armRequested_&&!controlConfigurationSnapshot().armRequested&&!safetyResetRequested_&&auditCutRequests==cuts);
        assert(expoTap(&TransmitterUi::controlDesign,34));
    }
    // Storage follows the same acknowledged-CUT policy as live steering travel.
    tuneRole_=0;selectedChannel_=steeringChannel();armRequested_=false;
    channelModel_.setResponse(selectedChannel_,ChannelResponseProfile{});assert(persistChannelModel());
    settingsSavePending_=channelParameterSavePending_=false;
    openDesignEditor(1,UiPage::Tune);armRequested_=true;safetyResetRequested_=false;
    const auto saveCuts=auditCutRequests;expoKey(PhysicalControlId::KeyPlus);
    const int savedExpo=editorDraft_;assert(expoTap(&TransmitterUi::controlDesign,35));
    assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==saveCuts);
    settingsSaveDueMs_=0;CrsfTelemetryState telemetry;CrsfChannelOutputModel outputs;
    update(THROTTLE_ADC_CENTER,STEERING_ADC_CENTER,0,0,CRSF_CHANNEL_MID,
        OutputSafetyStatus::Armed,telemetry,99,true,outputs,true);
    ChannelModelController stored;assert(ChannelSettingsRepository::load(activeModelSlot_,stored)==ChannelSettingsLoad::Loaded);
    assert(stored.responses[selectedChannel_-1].positiveExpoPercent==0&&settingsSavePending_);
    requestConfigurationCut();lv_tick_inc(40);
    update(THROTTLE_ADC_CENTER,STEERING_ADC_CENTER,0,0,CRSF_CHANNEL_MID,
        OutputSafetyStatus::Cut,telemetry,99,true,outputs,false);
    assert(!settingsSavePending_&&!channelParameterSavePending_);
    assert(ChannelSettingsRepository::load(activeModelSlot_,stored)==ChannelSettingsLoad::Loaded);
    assert(stored.responses[selectedChannel_-1].positiveExpoPercent==savedExpo&&stored.responses[selectedChannel_-1].negativeExpoPercent==savedExpo);
    assert(requestDisplayRotation());applyDisplayRotation();
}
safetyCutHandler_=expoCutHandler;
Serial.println("Live expo: touch/keypad/reset, all roles/reversal, asymmetric entry, next-cycle response, fixed neutral/endpoints, Cancel/Back/Apply, no CUT/autoarm and CUT-only persistence in both orientations PASS");
return true;
