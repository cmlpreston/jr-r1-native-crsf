// Integration checks exercise the actual UI callbacks and persistence. Run
// only with JR_R1_SIM_STATE pointed at a disposable directory.
#include <cassert>
#include <cstdlib>
static uint64_t auditRedrawnPixels = 0;
static void auditRefreshMonitor(lv_disp_drv_t *, uint32_t, uint32_t pixels)
{
    auditRedrawnPixels += pixels;
}
bool TransmitterUi::runSimulatorUiAudit()
{
    if(!getenv("JR_R1_SIM_STATE")){Serial.println("UI audit requires disposable JR_R1_SIM_STATE");return false;}
    const auto activate=PageControlEvent::Activate;
    // Boot PIN cannot be bypassed by bound arm actions or remote taps.
    assert(!bootPinUnlocked());drawBootPin();
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    if(const char *path=getenv("JR_R1_PIN_SCREENSHOT"))simulatorSaveScreenshot(path);
    armRequested_=true;assert(!controlConfigurationSnapshot().armRequested);
    assert(!applyDriveModeAction(static_cast<uint16_t>(DriveModeSelection::CutArm),1));
    assert(!applyTransmitterFunctionAction(static_cast<uint16_t>(TransmitterFunction::OpenMenu),1));
    assert(!portalRemoteInput(0,0,2));
    assert(applyTransmitterFunctionAction(static_cast<uint16_t>(TransmitterFunction::MenuSelect),1));
    assert(strcmp(bootPinDigits_,"1")==0);controlBootPin(10,activate);
    controlBootPin(11,activate);assert(!bootPinUnlocked()&&bootPinError_);
    for(int d:{1,1,1,1})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(!bootPinUnlocked()&&bootPinError_);
    controlBootPin(1,activate);controlBootPin(10,activate);assert(!bootPinDigits_[0]);
    setStartupPreflight(false,1);assert(activePage_==UiPage::BootPin);
    for(int d:{1,2,3,4})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(bootPinUnlocked()&&!armRequested());
    assert(activePage_==UiPage::StartupWarning); // Unlock does not dismiss preflight.
    setStartupPreflight(true,0);assert(activePage_==UiPage::Dashboard);
    // Refined navigation: CUT stays readable, role collisions are refused and
    // entering/changing modes never grants an arm request.
    armRequested_=false;drawDashboard();designTouchDispatch_=true;armGestureValid_=true;
    lastTouchDurationMs_=100;controlDesign(180,activate);assert(activePage_==UiPage::OutputStatus);
    lastTouchDurationMs_=900;controlDesign(180,activate);assert(activePage_==UiPage::WidgetSelect);
    drawDashboard();lastTouchDurationMs_=100;controlDesign(5,activate);assert(activePage_==UiPage::OutputStatus&&!armRequested_);
    controlDesign(181,activate);assert(!armRequested_);
    remoteTouchActive_=true;lastTouchDurationMs_=1000;controlDesign(181,activate);assert(!armRequested_);remoteTouchActive_=false;
    armGestureValid_=false;lastTouchDurationMs_=1000;controlDesign(181,activate);assert(!armRequested_);armGestureValid_=true;
    lastTouchDurationMs_=1000;controlDesign(181,activate);assert(armRequested_&&activePage_==UiPage::Dashboard);
    lastTouchDurationMs_=20;controlDesign(0,activate);assert(armRequested_);
    armGestureValid_=false;lastTouchDurationMs_=1000;controlDesign(0,activate);assert(armRequested_);armGestureValid_=true;
    lastTouchDurationMs_=799;controlDesign(0,activate);assert(armRequested_);
    lastTouchDurationMs_=800;controlDesign(0,activate);assert(!armRequested_);designTouchDispatch_=false;
    drawDashboard();controlDesign(6,activate);assert(activePage_==UiPage::ModeChooser);
    controlDesign(182,activate);assert(activePage_==UiPage::Dashboard&&driveMode()==DriveMode::Walk&&!armRequested_);
    // Leaving Walk requires a fresh PIN, survives restart/profile reload, and
    // cannot arm. Cancel and incorrect PIN preserve Walk.
    loadPinSecurity();loadDriveModeSettings();assert(driveMode()==DriveMode::Walk);
    controlDesign(6,activate);controlDesign(183,activate);
    assert(activePage_==UiPage::BootPin&&driveMode()==DriveMode::Walk&&!armRequested());
    if(const char *prefix=getenv("JR_R1_SECURITY_SCREENSHOTS")){
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        char path[256];snprintf(path,sizeof(path),"%s-walk.bmp",prefix);simulatorSaveScreenshot(path);
    }
    handleTouch(portraitUi?40:110,portraitUi?180:140);
    assert(strcmp(bootPinDigits_,"1")==0);controlBootPin(10,activate);
    for(int d:{9,9,9,9})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(bootPinError_&&driveMode()==DriveMode::Walk);
    controlBootPin(12,activate);assert(driveMode()==DriveMode::Walk&&pinPurpose_==PinPurpose::None);
    const auto walkSelection=static_cast<uint16_t>(DriveModeSelection::WalkNormal);
    assert(!applyDriveModeAction(walkSelection,1));assert(pinPurpose_==PinPurpose::WalkExit);
    controlBootPin(12,activate);
    assert(!applyDriveModeAction(walkSelection,1));assert(activePage_==UiPage::Dashboard);
    assert(applyDriveModeAction(walkSelection,-1));
    assert(!applyDriveModeAction(walkSelection,1));
    for(int d:{1,2,3,4})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(driveMode()==DriveMode::Normal&&!armRequested());
    loadPinSecurity();loadDriveModeSettings();assert(driveMode()==DriveMode::Normal);
    // PIN changes require current PIN and matching confirmation; persisted PIN
    // is also used by USB RF bench authorization. Recovery never unlocks Walk.
    assert(!changePin("9999","5678","5678"));assert(!changePin("1234","5678","5679"));
    drawDesignPage(UiPage::SystemMenu);
    if(const char *prefix=getenv("JR_R1_SECURITY_SCREENSHOTS")){
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        char path[256];snprintf(path,sizeof(path),"%s-system.bmp",prefix);simulatorSaveScreenshot(path);
    }
    controlDesign(237,activate);assert(pinPurpose_==PinPurpose::ChangeCurrent);
    for(int d:{1,2,3,4})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(pinPurpose_==PinPurpose::ChangeNew);
    for(int d:{5,6,7,8})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(pinPurpose_==PinPurpose::ChangeConfirm);
    for(int d:{5,6,7,9})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(bootPinError_&&verifyPin("1234"));
    for(int d:{5,6,7,8})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(pinPurpose_==PinPurpose::None&&verifyPin("5678"));
    loadPinSecurity();assert(verifyPin("5678")&&!verifyPin("1234"));
    requestDriveMode(DriveMode::Walk);assert(recoverPinOverUsb("1234"));
    assert(driveMode()==DriveMode::Walk&&verifyPin("1234")&&!armRequested());
    armRequested_=true;assert(!recoverPinOverUsb("4567"));armRequested_=false;
    requestDriveMode(DriveMode::Normal);
    for(int d:{1,2,3,4})controlBootPin(d,activate);
    controlBootPin(11,activate);assert(driveMode()==DriveMode::Normal);

    selectedChannel_=throttleChannel();drawChannelPlaceholder();controlDesign(186,activate);assert(activePage_==UiPage::ChannelInput);
    controlDesign(189,activate);const auto existingRoles=channelRoleSettings_;controlDesign(193,activate);
    assert(channelRoleConflict_&&channelRoleSettings_.steeringChannel==existingRoles.steeringChannel);
    controlDesign(209,activate);controlDesign(209,activate);assert(activePage_==UiPage::ChannelPlaceholder);
    controlDesign(188,activate);assert(activePage_==UiPage::MixingOverview);
    controlDesign(203,activate);assert(activePage_==UiPage::ChannelMixing&&mixingBank_==5);
    controlChannelMixing(11,activate);assert(mixingBank_==7);controlDesign(218,activate);assert(activePage_==UiPage::MixingOverview);
    controlDesign(209,activate);assert(activePage_==UiPage::ChannelPlaceholder);
    drawMainMenu();controlDesign(141,activate);assert(setupBank_==1&&setupPage_==0);
    controlDesign(208,activate);assert(setupPage_==1);controlDesign(140,activate);assert(setupBank_==0&&setupPage_==0);
    drawThrottleResponsePage();statusReturnPage_=UiPage::ThrottleResponse;drawDesignPage(UiPage::OutputStatus);controlDesign(209,activate);assert(activePage_==UiPage::ThrottleResponse);
    // Detailed response keeps each side independent and leaves both channel
    // offsets alone when editing the separate response-profile offset.
    {
        const auto savedChannel=selectedChannel_;selectedChannel_=16;
        const auto originalProfile=profileForChannel(16);
        const auto trim=channelParameterValue(16,ChannelParameterKind::Trim),subtrim=channelParameterValue(16,ChannelParameterKind::Subtrim);
        drawDesignPage(UiPage::ChannelResponseSetup);controlDesign(196,activate);assert(activePage_==UiPage::ResponseProfile);
        controlDesign(232,activate);controlDesign(235,activate);editorDraft_=65;controlDesign(34,activate);
        assert(profileForChannel(16).negativeEndpointPercent==originalProfile.negativeEndpointPercent);
        controlDesign(235,activate);editorDraft_=70;controlDesign(35,activate);
        assert(profileForChannel(16).negativeEndpointPercent==70&&profileForChannel(16).positiveEndpointPercent==originalProfile.positiveEndpointPercent);
        controlDesign(236,activate);controlDesign(235,activate);assert(editorField_==7);editorDraft_=7;controlDesign(35,activate);
        assert(profileForChannel(16).subtrimPercent==7&&channelParameterValue(16,ChannelParameterKind::Trim)==trim&&channelParameterValue(16,ChannelParameterKind::Subtrim)==subtrim);
        navigateBack();assert(activePage_==UiPage::ChannelResponseSetup);
        profileForChannel(16)=originalProfile;saveSettings();selectedChannel_=savedChannel;
    }
    // Choosing a trim control is a draft. Cancel and occupied controls cannot
    // change saved assignments; Apply updates only the chosen channel parameter.
    {
        const auto savedChannel=selectedChannel_;const auto savedCount=bindingCount_;
        ControlBinding savedBindings[kPhysicalControlCount];memcpy(savedBindings,bindings_,sizeof(savedBindings));
        bindingCount_=0;selectedChannel_=16;
        drawDesignPage(UiPage::ChannelResponseSetup);controlDesign(197,activate);
        assert(activePage_==UiPage::ParameterControlPicker);
        controlDesign(225,activate);assert(parameterControlDraft_==PhysicalControlId::GripTrimA&&bindingCount_==0);
        navigateBack();assert(activePage_==UiPage::ChannelResponseSetup&&bindingCount_==0);
        controlDesign(197,activate);controlDesign(225,activate);armRequested_=true;controlDesign(230,activate);
        assert(activePage_==UiPage::ChannelResponseSetup&&bindingCount_==1&&!armRequested_);
        assert(bindingIndexForChannelParameter(16,ChannelParameterKind::Trim)>=0);
        controlDesign(198,activate);controlDesign(225,activate);
        assert(parameterControlError_&&parameterControlDraft_==PhysicalControlId::Count&&bindingCount_==1);
        controlDesign(226,activate);assert(!parameterControlError_);controlDesign(230,activate);
        assert(bindingCount_==2&&bindingIndexForChannelParameter(16,ChannelParameterKind::Subtrim)>=0);
        memcpy(bindings_,savedBindings,sizeof(savedBindings));bindingCount_=savedCount;saveBindings();selectedChannel_=savedChannel;
        selectedBindingInput_=PhysicalControlId::TimerButton;bindingDraft_=defaultBindingForInput(selectedBindingInput_);bindingEditorBank_=0;
        drawBindingEditorPage();const auto originalDraft=bindingDraft_;
        controlBindingEditor(9,activate);assert(bindingEditorBank_==1);
        controlBindingEditor(8,activate);assert(bindingEditorBank_==0&&bindingDraft_.contexts==originalDraft.contexts);
        bindingDraft_.target=AssignmentTarget::CrsfChannel;
        controlBindingEditor(10,activate);assert(bindingEditorBank_==2);
        bindingDraft_.minimum=0;bindingDraft_.maximum=600;bindingDraft_.initial=bindingDraft_.failsafe=0;
        controlBindingEditor(12,PageControlEvent::Decrease);assert(bindingDraft_.minimum==-10);
        controlBindingEditor(12,PageControlEvent::Increase);assert(bindingDraft_.minimum==0);
        bindingDraft_.step=10;controlBindingEditor(14,PageControlEvent::Decrease);assert(bindingDraft_.step==1);
        controlBindingEditor(11,activate);assert(bindingEditorBank_==3);
        controlBindingEditor(15,PageControlEvent::Decrease);assert(bindingDraft_.initial==0);
        controlBindingEditor(16,PageControlEvent::Increase);assert(bindingDraft_.failsafe==10);
        controlBindingEditor(17,activate);assert(bindingDraft_.inverted);
        bindingDraft_.target=AssignmentTarget::TransmitterFunction;bindingDraft_.behaviour=ControlBehaviour::Momentary;
        controlBindingEditor(3,PageControlEvent::Increase);assert(bindingDraft_.behaviour==ControlBehaviour::Momentary);
        controlBindingEditor(5,activate);assert(activePage_==UiPage::ControlAssignments&&bindingCount_==savedCount);
    }
    if(const char *prefix=getenv("JR_R1_EDITOR_SCREENSHOTS")){
        auto shot=[&](unsigned n){lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];snprintf(path,sizeof(path),"%s-%u.bmp",prefix,n);simulatorSaveScreenshot(path);};
        selectedBindingInput_=PhysicalControlId::TimerButton;bindingDraft_=defaultBindingForInput(selectedBindingInput_);bindingEditorBank_=0;
        drawBindingEditorPage();shot(0);bindingEditorBank_=1;drawBindingEditorPage();shot(1);
        bindingDraft_.target=AssignmentTarget::TransmitterFunction;drawBindingEditorPage();shot(2);
        drawDesignPage(UiPage::ChannelResponseSetup);controlDesign(197,activate);shot(3);
        drawDesignPage(UiPage::ChannelResponseSetup);controlDesign(196,activate);shot(4);controlDesign(234,activate);shot(5);bindingEditorBank_=0;
    }
    if(const char *prefix=getenv("JR_R1_REFINED_SCREENSHOTS")){
        const UiPage capturePages[]={UiPage::Dashboard,UiPage::MainMenu,UiPage::ChannelPlaceholder,UiPage::ChannelInput,UiPage::ChannelResponseSetup,UiPage::MixingOverview,UiPage::ModeChooser,UiPage::OutputStatus,UiPage::NetworkOptions,UiPage::SystemMenu,UiPage::Setup};
        for(unsigned n=0;n<sizeof(capturePages)/sizeof(capturePages[0]);++n){
            showSimulatorPage(capturePages[n]);lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
            char path[256];snprintf(path,sizeof(path),"%s-%u.bmp",prefix,n);simulatorSaveScreenshot(path);
        }
    }
    drawTelemetryPage();controlDesign(223,activate);assert(activePage_==UiPage::InputMonitor);navigateBack();assert(activePage_==UiPage::Telemetry);
    drawDesignPage(UiPage::SystemMenu);controlDesign(48,activate);navigateBack();assert(activePage_==UiPage::SystemMenu);
    drawDashboard();
    // Binary monitor classification is based on assignment, never sampled values.
    const auto savedBinding=bindings_[0];const auto savedCount=bindingCount_;
    const auto savedMix=channelMixing_[14];channelMixing_[14]=ChannelMixSettings{};
    bindings_[0]=ControlBinding{};bindings_[0].input=PhysicalControlId::IdleUpSwitch;
    bindings_[0].target=AssignmentTarget::CrsfChannel;bindings_[0].crsfChannel=15;
    bindings_[0].behaviour=ControlBehaviour::TwoPosition;bindingCount_=1;
    int binaryOff=0,binaryOn=0;assert(dashboardBinaryChannel(14,binaryOff,binaryOn));
    bindings_[0].behaviour=ControlBehaviour::Stepped;assert(!dashboardBinaryChannel(14,binaryOff,binaryOn));
    bindings_[0].behaviour=ControlBehaviour::TwoPosition;bindings_[0].input=PhysicalControlId::GripTrimA;
    assert(!dashboardBinaryChannel(14,binaryOff,binaryOn));
    bindings_[0].input=PhysicalControlId::IdleUpSwitch;channelMixing_[14].rules[0].enabled=1;
    assert(!dashboardBinaryChannel(14,binaryOff,binaryOn));channelMixing_[14]=savedMix;
    bindings_[0]=savedBinding;bindingCount_=savedCount;
    // Appearance persists per model and never changes channel settings.
    latestRawAdc_=triggerInput_.centre();armRequested_=false;
    const auto appearanceSlot=activeModelSlot_;
    const auto appearanceImage=modelImages_[appearanceSlot];
    const auto appearanceColour=modelColours_[appearanceSlot];
    modelImages_[appearanceSlot]=7;modelColours_[appearanceSlot]=0x3050ff;
    const auto oldAccent=modelAccents_[appearanceSlot],oldBase=modelBases_[appearanceSlot];
    modelAccents_[appearanceSlot]=0x00ff00;modelBases_[appearanceSlot]=0xeeeeee;
    saveModelMemorySettings();modelImages_[appearanceSlot]=8;modelColours_[appearanceSlot]=0;
    loadModelMemorySettings();assert(modelImages_[appearanceSlot]==7&&modelColours_[appearanceSlot]==0x3050ff);
    assert(modelAccents_[appearanceSlot]==0x00ff00&&modelBases_[appearanceSlot]==0xeeeeee);
    drawModelSelectPage();controlDesign(160,activate);assert(modelAppearanceEditing_);
    controlDesign(162,activate);assert(modelImages_[appearanceSlot]==8);
    controlDesign(161,activate);assert(modelImages_[appearanceSlot]==7);
    controlDesign(163,activate);assert(validModelColour(modelColours_[appearanceSlot]));
    armRequested_=true;controlDesign(162,activate);assert(modelImages_[appearanceSlot]==7);armRequested_=false;
    controlDesign(160,activate);assert(!modelAppearanceEditing_);
    modelImages_[appearanceSlot]=appearanceImage;modelColours_[appearanceSlot]=appearanceColour;modelAccents_[appearanceSlot]=oldAccent;modelBases_[appearanceSlot]=oldBase;saveModelMemorySettings();
    activePage_=UiPage::Channel2;
    latestRawAdc_=triggerInput_.centre();
    armRequested_=false;
    // Throttle settings show physical input even with neutral output and no RX.
    drawPage();
    const auto oldTimeout = systemSettings_.screenTimeoutSeconds;
    systemSettings_.screenTimeoutSeconds = 0;
    CrsfTelemetryState noReceiver;
    CrsfChannelOutputModel neutralOutputs;
    for (int16_t input : {int16_t(-1000), int16_t(0), int16_t(1000)}) {
        nextDisplayUpdateMs_ = 0;
        update(triggerInput_.centre(), steeringInput_.centre(), input, 0,
               kCrsfMid, OutputSafetyStatus::Cut, noReceiver, 0, false,
               neutralOutputs, false);
        assert(activePage_ == UiPage::Channel2);
        assert(lv_bar_get_value(channel2LiveBar_) == input && !armRequested_);
        assert(strcmp(lv_label_get_text(channel2ValueLabel_),
            input < 0 ? "-100%" : input > 0 ? "+100%" : "+0%") == 0);
        assert(!calibrationActive());
    }
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    if(const char *path=getenv("JR_R1_AUDIT_INPUT_SCREENSHOT")) simulatorSaveScreenshot(path);
    systemSettings_.screenTimeoutSeconds = oldTimeout;
    // Drafts must not affect channel output settings until explicitly applied.
    tuneRole_=1;selectedChannel_=channelRoleSettings_.throttleChannel;
    auto original=profileForChannel(selectedChannel_);
    openDesignEditor(0,UiPage::Tune);editorDraft_=55;
    assert(profileForChannel(selectedChannel_).positiveRatePercent==original.positiveRatePercent);
    controlDesign(34,activate);
    assert(profileForChannel(selectedChannel_).positiveRatePercent==original.positiveRatePercent);
    openDesignEditor(0,UiPage::Tune);editorDraft_=55;controlDesign(35,activate);
    assert(profileForChannel(selectedChannel_).positiveRatePercent==55);
    assert(profileForChannel(selectedChannel_).negativeRatePercent==original.negativeRatePercent);
    tuneRole_=2;openDesignEditor(0,UiPage::Tune);editorDraft_=70;controlDesign(35,activate);
    assert(profileForChannel(selectedChannel_).positiveRatePercent==55 && profileForChannel(selectedChannel_).negativeRatePercent==70);
    profileForChannel(selectedChannel_)=original;saveSettings();
    // Steering selects independent drafts from physical input or a side tap.
    tuneRole_=0;selectedChannel_=channelRoleSettings_.steeringChannel;
    const auto steeringOriginal=profileForChannel(selectedChannel_);
    auto &steering=profileForChannel(selectedChannel_);
    steering.negativeEndpointPercent=81;steering.positiveEndpointPercent=93;
    steering.reversed=true; // Selection follows the wheel, not output reversal.
    latestSteeringRawAdc_=steeringInput_.calibration().minimum;
    openDesignEditor(2,UiPage::Tune);
    assert(steeringEndpointSide_==0 && editorDraft_==81 && editorFine_);
    adjustPageFocus(-1);assert(editorDraft_==80);
    assert(steering.negativeEndpointPercent==81 && steering.positiveEndpointPercent==93);
    latestSteeringRawAdc_=steeringInput_.calibration().maximum;
    updateSteeringEndpointSelection();assert(steeringEndpointSide_==1 && editorDraft_==93);
    adjustPageFocus(1);assert(editorDraft_==94 && steeringEndpointDrafts_[0]==80);
    latestSteeringRawAdc_=steeringInput_.centre()+5;
    updateSteeringEndpointSelection();assert(steeringEndpointSide_==1);
    latestSteeringRawAdc_=steeringInput_.calibration().maximum;
    updateSteeringEndpointSelection();controlDesign(150,activate);
    updateSteeringEndpointSelection();assert(steeringEndpointSide_==0 && editorDraft_==80);
    // A tap wins over a held wheel; centre then right reselects right.
    latestSteeringRawAdc_=steeringInput_.centre();updateSteeringEndpointSelection();
    latestSteeringRawAdc_=steeringInput_.calibration().maximum;updateSteeringEndpointSelection();
    assert(steeringEndpointSide_==1 && editorDraft_==94);
    latestSteeringRawAdc_=65535;updateSteeringEndpointSelection();assert(steeringEndpointSide_==1);
    controlDesign(35,activate);assert(!armRequested_);
    assert(steering.negativeEndpointPercent==80 && steering.positiveEndpointPercent==94 && steering.reversed);
    saveSettings();loadSettings();
    assert(steering.negativeEndpointPercent==80 && steering.positiveEndpointPercent==94);
    latestSteeringRawAdc_=steeringInput_.centre();
    openDesignEditor(2,UiPage::Tune);controlDesign(150,activate);controlDesign(33,activate);
    assert(steeringEndpointDrafts_[0]==100 && steeringEndpointDrafts_[1]==94);
    controlDesign(34,activate);assert(steering.negativeEndpointPercent==80);
    openDesignEditor(2,UiPage::Tune);controlDesign(150,activate);
    for(int i=0;i<70;++i)adjustPageFocus(-1);
    assert(editorDraft_==50 && steeringEndpointDrafts_[1]==94);
    controlDesign(151,activate);for(int i=0;i<20;++i)adjustPageFocus(1);
    assert(editorDraft_==100 && steeringEndpointDrafts_[0]==50);
    controlDesign(34,activate);assert(steering.negativeEndpointPercent==80 && steering.positiveEndpointPercent==94);
    steering=steeringOriginal;saveSettings();
    // Throttle and brake retain their existing independent endpoint edits.
    tuneRole_=1;selectedChannel_=channelRoleSettings_.throttleChannel;
    openDesignEditor(2,UiPage::Tune);editorDraft_=75;controlDesign(35,activate);
    assert(profileForChannel(selectedChannel_).positiveEndpointPercent==75);
    assert(profileForChannel(selectedChannel_).negativeEndpointPercent==original.negativeEndpointPercent);
    tuneRole_=2;openDesignEditor(2,UiPage::Tune);editorDraft_=85;controlDesign(35,activate);
    assert(profileForChannel(selectedChannel_).negativeEndpointPercent==85 && profileForChannel(selectedChannel_).positiveEndpointPercent==75);
    profileForChannel(selectedChannel_)=original;saveSettings();
    // Channel-to-control shortcut preserves drafts until Save and provides a return path.
    {
        const auto savedChannel=selectedChannel_;
        const auto savedCount=bindingCount_;
        ControlBinding savedBindings[kPhysicalControlCount];
        memcpy(savedBindings,bindings_,sizeof(savedBindings));
        selectedChannel_=16;bindingCount_=0;
        drawChannelPlaceholder();controlChannelConfig(4,activate);
        assert(activePage_==UiPage::ControlAssignments && assignmentChannelContext_==16);
        const size_t input=static_cast<size_t>(PhysicalControlId::TimerButton);
        assignmentBank_=input/5;
        controlAssignments(input%5,activate);
        assert(activePage_==UiPage::BindingEditor && bindingDraft_.crsfChannel==16);
        assert(bindingCount_==0);
        controlBindingEditor(5,activate); // Cancel cannot create an assignment.
        assert(activePage_==UiPage::ControlAssignments && bindingCount_==0);
        navigateBack();assert(activePage_==UiPage::ChannelPlaceholder);
        bindings_[0]=defaultBindingForInput(PhysicalControlId::TimerButton);
        bindings_[0].crsfChannel=16;bindingCount_=1;
        controlChannelConfig(4,activate);
        assert(activePage_==UiPage::BindingEditor && selectedBindingInput_==PhysicalControlId::TimerButton);
        assert(bindingDraft_.crsfChannel==16);
        selectedChannel_=channelRoleSettings_.throttleChannel;
        controlChannelConfig(4,activate);assert(activePage_==UiPage::ChannelInput);
        navigateBack();assert(activePage_==UiPage::ChannelPlaceholder);
        drawMainMenu();assert(assignmentChannelContext_==0);
        bindingCount_=savedCount;memcpy(bindings_,savedBindings,sizeof(savedBindings));
        selectedChannel_=savedChannel;
    }
    // Mixer drafts, model isolation, persisted reload and all editor pages.
    {
        const auto saved=channelMixing_[2];
        const auto selected=selectedChannel_;selectedChannel_=3;
        controlChannelConfig(5,activate);
        assert(activePage_==UiPage::ChannelMixing);
        controlChannelMixing(0,activate);controlChannelMixing(2,PageControlEvent::Decrease);
        assert(mixingDraft_.rules[0].enabled&&mixingDraft_.rules[0].weight==95);
        assert(memcmp(&saved,&channelMixing_[2],sizeof(saved))==0);
        navigateBack();assert(activePage_==UiPage::MixingOverview);navigateBack();assert(activePage_==UiPage::ChannelPlaceholder); // cancel draft
        controlChannelConfig(5,activate);
        assert(memcmp(&saved,&mixingDraft_,sizeof(saved))==0);
        mixingDraft_.rules[0].enabled=1;mixingDraft_.rules[0].weight=-70;mixingDraft_.rules[0].replace=1;
        mixingDraft_.rateEnabled=1;mixingDraft_.lowPositive=50;mixingDraft_.lowNegative=100;
        mixingDraft_.constantPercent=60;mixingDraft_.outputMinimum=20;mixingDraft_.outputMaximum=60;mixingDraft_.cutOverride=1;mixingDraft_.cutPercent=60;
        for(uint8_t bank=0;bank<8;++bank){
            mixingBank_=bank;if(bank==5){mixingPreviewValues_[1]=500;mixingPreviewValues_[2]=500;}drawChannelMixing();lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
            if(const char *prefix=getenv("JR_R1_MIX_IMAGES")){
                char path[256];snprintf(path,sizeof(path),"%s-%s-%u.bmp",prefix,designPortrait?"portrait":"landscape",bank);
                simulatorSaveScreenshot(path);
            }
        }
        armRequested_=true;controlChannelMixing(12,activate);
        assert(!armRequested_&&safetyResetRequested_&&!mixingSaveError_);
        loadMixing();assert(channelMixing_[2].rules[0].weight==-70);
        assert(channelMixing_[2].constantPercent==60&&channelMixing_[2].outputMinimum==20&&channelMixing_[2].cutOverride&&channelMixing_[2].cutPercent==60);
        const auto slot=activeModelSlot_;activeModelSlot_=4;loadMixing();
        assert(!channelMixing_[2].rules[0].enabled);
        activeModelSlot_=slot;loadMixing();assert(channelMixing_[2].rules[0].weight==-70);
        // Threshold controls, v4 persistence and model isolation.
        mixingDraft_=ChannelMixSettings{};mixingDraft_.includeBase=0;
        mixingDraft_.rules[0].enabled=1;mixingDraft_.rules[0].source=1;
        mixingBank_=3;drawChannelMixing();
        controlChannelMixing(2,activate);controlChannelMixing(2,activate);
        assert(mixingDraft_.rules[0].sourceMode==2);
        for(int n=0;n<5;++n)controlChannelMixing(3,PageControlEvent::Decrease);
        for(int n=0;n<2;++n)controlChannelMixing(4,PageControlEvent::Decrease);
        assert(mixingDraft_.rules[0].thresholdPercent==-5&&mixingDraft_.rules[0].hysteresisPercent==3);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        if(const char *prefix=getenv("JR_R1_MIX_IMAGES")){
            char path[256];snprintf(path,sizeof(path),"%s-%s-threshold.bmp",prefix,designPortrait?"portrait":"landscape");
            simulatorSaveScreenshot(path);
        }
        controlChannelMixing(12,activate);assert(!mixingSaveError_);
        loadMixing();assert(channelMixing_[2].rules[0].sourceMode==2);
        assert(channelMixing_[2].rules[0].thresholdPercent==-5&&channelMixing_[2].rules[0].hysteresisPercent==3);
        activeModelSlot_=4;loadMixing();assert(channelMixing_[2].rules[0].sourceMode==0);
        activeModelSlot_=slot;loadMixing();assert(channelMixing_[2].rules[0].sourceMode==2);
        assert(!saveMixing(throttleChannel(),channelMixing_[2]));
        assert(saveMixing(3,saved));selectedChannel_=selected;
    }
    // All ten Setup routes must remain reachable, including non-demo features.
    const UiPage routes[]={UiPage::ModelSelect,UiPage::Channels,UiPage::ControlAssignments,UiPage::DriveModes,UiPage::ElrsTxConfig,UiPage::ElrsBinding,UiPage::EscProgramming,UiPage::Setup,UiPage::InputMonitor,UiPage::WidgetSelect};
    for(uint8_t i=0;i<10;++i){drawMainMenu();controlDesign(40+i,activate);assert(activePage_==routes[i]);}
    // Tall widgets use either top anchor; resizing preserves the covered slot.
    for(uint8_t anchor=0;anchor<2;++anchor){
        dashboardLayouts_[activeModelSlot_]=defaultDashboardLayout();
        dashboardLayouts_[activeModelSlot_].cells[0].width=1;
        auto hidden=dashboardLayouts_[activeModelSlot_].cells[anchor+2];
        drawDesignPage(UiPage::WidgetSelect);controlDesign(110+anchor,activate);
        widgetDraft_.kind=DashboardWidgetKind::Channels;widgetDraft_.width=widgetDraft_.height=1;
        do{controlDesign(121,activate);}while(widgetDraft_.width!=1||widgetDraft_.height!=2);
        controlDesign(126,activate);loadModelMemorySettings();drawDashboard();
        assert(dashboardLayouts_[activeModelSlot_].cells[anchor].height==2);
        assert(!widgetViews_[anchor+2].panel);
        assert(memcmp(&hidden,&dashboardLayouts_[activeModelSlot_].cells[anchor+2],sizeof(hidden))==0);
        drawDesignPage(UiPage::WidgetSelect);controlDesign(110+anchor,activate);
        do{controlDesign(121,activate);}while(widgetDraft_.width!=1||widgetDraft_.height!=1);
        controlDesign(126,activate);
        assert(widgetViews_[anchor+2].panel);
    }
    dashboardLayouts_[activeModelSlot_]=defaultDashboardLayout();
    // Two tall channel panels exercise stacked dials and graph legends.
    for(uint8_t anchor=0;anchor<2;++anchor){
        DashboardWidget widget;widget.kind=DashboardWidgetKind::Channels;
        widget.width=1;widget.height=2;widget.source=0;widget.secondSource=1;
        widget.style=anchor?DashboardWidgetStyle::Graph:DashboardWidgetStyle::Dials;
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],anchor,widget);
    }
    drawDashboard();lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    assert(widgetViews_[0].arcs[0]&&widgetViews_[0].arcs[1]);
    assert(lv_obj_get_y(widgetViews_[0].arcs[1])>=lv_obj_get_y(widgetViews_[0].arcs[0])+lv_obj_get_height(widgetViews_[0].arcs[0]));
    if(const char *prefix=getenv("JR_R1_TALL_WIDGET_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s.bmp",prefix,designPortrait?"portrait":"landscape");
        simulatorSaveScreenshot(path);
    }
    dashboardLayouts_[activeModelSlot_]=defaultDashboardLayout();
    // Widget size, overlap and persistence are isolated from control settings.
    const auto retainedLayout=dashboardLayouts_[activeModelSlot_];
    drawDesignPage(UiPage::WidgetSelect);controlDesign(110,activate);
    widgetDraft_.kind=DashboardWidgetKind::Map;widgetDraft_.width=widgetDraft_.height=2;
    auto before=controlConfigurationSnapshot();
    controlDesign(126,activate);
    assert(dashboardWidgetMask(0,dashboardLayouts_[activeModelSlot_].cells[0])==15);
    auto after=controlConfigurationSnapshot();
    assert(before.throttleChannel==after.throttleChannel && before.armRequested==after.armRequested);
    loadModelMemorySettings();assert(dashboardLayouts_[activeModelSlot_].cells[0].kind==DashboardWidgetKind::Map);
    for(int cell=1;cell<4;++cell){
        assert(!widgetViews_[cell].panel);
        assert(memcmp(&dashboardLayouts_[activeModelSlot_].cells[cell],&retainedLayout.cells[cell],sizeof(DashboardWidget))==0);
    }
    if(const char *prefix=getenv("JR_R1_MAP_WIDGET_IMAGES")){
        for(int w=1;w<=2;++w)for(int h=1;h<=2;++h){
            dashboardLayouts_[activeModelSlot_].cells[0].width=w;
            dashboardLayouts_[activeModelSlot_].cells[0].height=h;
            drawDashboard();lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
            char path[256];snprintf(path,sizeof(path),"%s-%s-%dx%d.bmp",prefix,designPortrait?"portrait":"landscape",w,h);
            simulatorSaveScreenshot(path);
        }
    }
    widgetDraft_=dashboardLayouts_[activeModelSlot_].cells[0];widgetDraft_.width=widgetDraft_.height=1;
    controlDesign(126,activate);
    for(int cell=1;cell<4;++cell)if(retainedLayout.cells[cell].kind!=DashboardWidgetKind::Empty)assert(widgetViews_[cell].panel);
    widgetDraft_.width=widgetDraft_.height=2;controlDesign(126,activate);
    toggleRunTimer();assert(runTimerRunning_);drawTelemetryPage();assert(runTimerRunning_);toggleRunTimer();assert(!runTimerRunning_);
    // Model changes remain gated by CUT and neutral throttle.
    armRequested_=true;assert(!selectModelMemory(1));armRequested_=false;
    latestRawAdc_=triggerInput_.centre()+ARM_NEUTRAL_ADC_WINDOW+10;assert(!selectModelMemory(1));
    latestRawAdc_=triggerInput_.centre();assert(selectModelMemory(1));
    assert(dashboardLayouts_[1].cells[0].kind==DashboardWidgetKind::Timer);
    assert(selectModelMemory(0));assert(dashboardLayouts_[0].cells[0].kind==DashboardWidgetKind::Map);
    dashboardLayouts_[0]=defaultDashboardLayout();saveModelMemorySettings();
    // Assigned reset clears only speed statistics, ignores release, and keeps channels/timer intact.
    speedStatistics_.update(120,1000,1000,true);speedStatistics_.update(300,2000,2000,true);
    auto speedResetId=static_cast<uint16_t>(TransmitterFunction::ResetSpeedStatistics);
    assert(applyTransmitterFunctionAction(speedResetId,0) && speedStatistics_.maximum()==300);
    auto beforeReset=controlConfigurationSnapshot();
    assert(applyTransmitterFunctionAction(speedResetId,1) && !speedStatistics_.available());
    assert(controlConfigurationSnapshot().throttleChannel==beforeReset.throttleChannel);
    sensorSelected_=0;drawDesignPage(UiPage::SensorDetail);
    speedStatistics_.update(100,3000,3000,true);controlDesign(148,activate);assert(!speedStatistics_.available());
    for(uint8_t width=1;width<=2;++width){for(uint8_t height=1;height<=2;++height){
        DashboardWidget speed;speed.kind=DashboardWidgetKind::Sensor;speed.source=0;speed.width=width;speed.height=height;
        for(auto style:{DashboardWidgetStyle::Numbers,DashboardWidgetStyle::Graph}){
            speed.style=style;placeDashboardWidget(dashboardLayouts_[0],0,speed);drawDashboard();
            assert(widgetViews_[0].values[1]!=nullptr);
        }
    }}
    dashboardLayouts_[0]=defaultDashboardLayout();
    // Speed emphasis is a per-widget draft, persisted only on Apply.
    DashboardWidget speedChoice;speedChoice.kind=DashboardWidgetKind::Sensor;
    placeDashboardWidget(dashboardLayouts_[0],0,speedChoice);
    controlDesign(110,activate);controlDesign(124,activate);
    assert(widgetDraft_.speedValue==SpeedWidgetValue::Maximum);
    controlDesign(125,activate);
    assert(dashboardLayouts_[0].cells[0].speedValue==SpeedWidgetValue::Current);
    controlDesign(110,activate);controlDesign(124,activate);controlDesign(126,activate);
    loadModelMemorySettings();
    assert(dashboardLayouts_[0].cells[0].speedValue==SpeedWidgetValue::Maximum);
    assert(dashboardLayouts_[1].cells[0].speedValue==SpeedWidgetValue::Current);
    speedStatistics_.reset(0);speedStatistics_.update(100,1,1,true);speedStatistics_.update(300,1001,1001,true);
    CrsfTelemetryState speedFeed{};speedFeed.gpsValid=true;speedFeed.satellites=12;
    speedFeed.groundSpeedDeciKmh=400;speedFeed.gpsUpdatedMs=millis();
    const char *expected[]={"40.0","30.0","20.0"};
    for(uint8_t choice=0;choice<3;++choice){
        dashboardLayouts_[0].cells[0].speedValue=static_cast<SpeedWidgetValue>(choice);
        drawDashboard();updateDesign(speedFeed,OutputSafetyStatus::Cut,99,true);
        assert(strcmp(lv_label_get_text(widgetViews_[0].values[0]),expected[choice])==0);
    }
    speedFeed.gpsValid=false;updateDesign(speedFeed,OutputSafetyStatus::Cut,99,true);
    assert(strcmp(lv_label_get_text(widgetViews_[0].values[0]),"20.0")==0);
    assert(strstr(lv_label_get_text(widgetViews_[0].values[1]),"Now --"));
    dashboardLayouts_[0]=defaultDashboardLayout();saveModelMemorySettings();
    // Merely selecting CUT with the keypad cannot reuse a previous long touch.
    drawDashboard();lastTouchDurationMs_=1000;designTouchDispatch_=false;
    controlDesign(0,activate);assert(!armRequested_);
    designTouchDispatch_=true;armGestureValid_=true;lastTouchDurationMs_=100;controlDesign(0,activate);assert(!armRequested_);
    lastTouchDurationMs_=900;controlDesign(0,activate);assert(armRequested_);
    lastTouchDurationMs_=20;controlDesign(0,activate);assert(armRequested_);
    armGestureValid_=false;lastTouchDurationMs_=1000;controlDesign(0,activate);assert(armRequested_);armGestureValid_=true;
    lastTouchDurationMs_=799;controlDesign(0,activate);assert(armRequested_);
    lastTouchDurationMs_=800;controlDesign(0,activate);assert(!armRequested_);designTouchDispatch_=false;
    // Calibration is stored as one verified record and survives settings reload.
    const auto oldCalibration = triggerInput_.calibration();
    calibratingSteering_ = false;
    triggerInput_.setCalibration({1550, 1880, 2450});
    assert(saveCalibration());
    triggerInput_.setCalibration(oldCalibration);
    loadSettings();
    assert(triggerInput_.calibration().minimum == 1550 &&
           triggerInput_.centre() == 1880 && triggerInput_.calibration().maximum == 2450);
    startCalibration(1880, false);
    assert(strstr(lv_label_get_text(calibrationStatusLabel_), "WAIT"));
    for(uint32_t t=10;t<1600;t+=10) triggerInput_.updateCalibration(1550,t);
    for(uint32_t t=1600;t<3200;t+=10) triggerInput_.updateCalibration(2450,t);
    drawCalibrationValues(2450);
    assert(strstr(lv_label_get_text(calibrationStatusLabel_), "Both ends OK"));
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    if(const char *path=getenv("JR_R1_AUDIT_CAL_SCREENSHOT")) simulatorSaveScreenshot(path);
    cancelCalibration();
    triggerInput_.setCalibration(oldCalibration);assert(saveCalibration());
    // The bench switch persists and cannot retain an armed state.
    channelRoleSettings_.steeringEnabled = true;
    armRequested_ = true;
    controlChannelRoles(2, activate);
    assert(!steeringEnabled() && !armRequested_ && safetyResetRequested_);
    loadChannelRoleSettings();
    assert(!steeringEnabled() && !controlConfigurationSnapshot().steeringEnabled);
    controlChannelRoles(2, activate);
    assert(steeringEnabled() && !armRequested_);
    if(designPortrait){
        drawMainMenu();dispatchPageControlTouch(250,60);assert(setupBank_==1);
        dispatchPageControlTouch(80,110);assert(activePage_==UiPage::DisplaySetup);
        dispatchPageControlTouch(239,22);assert(activePage_==UiPage::MainMenu);
        drawChannelRolesPage();auto oldRole=channelRoleSettings_.steeringChannel;
        dispatchPageControlTouch(282,94);assert(channelRoleSettings_.steeringChannel!=oldRole);
        dispatchPageControlTouch(231,94);assert(channelRoleSettings_.steeringChannel==oldRole);
        drawThrottleResponsePage();dispatchPageControlTouch(282,434);assert(portraitResponseBank_==1);
        dispatchPageControlTouch(33,434);assert(portraitResponseBank_==0);
        drawElrsBindingPage();auto length=strlen(elrsBindPhrase_);
        dispatchPageControlTouch(15,245);assert(strlen(elrsBindPhrase_)==length+1);
        dispatchPageControlTouch(271,434);assert(strlen(elrsBindPhrase_)==length);
    }
    // Colour choices persist without altering control state. Light retains the
    // last dark palette for the existing assignable Night mode action.
    const auto originalSystem = systemSettings_;
    displaySetupBank_ = 0;
    drawDisplayPage();controlDisplay(3,activate);
    assert(activePage_ == UiPage::ColourScheme);
    for(uint8_t choice=0;choice<=darkColourSchemeCount;++choice){
        controlColourScheme(choice,activate);
        assert(activePage_==UiPage::ColourScheme && !armRequested_);
        loadSystemSettings();
        assert(systemSettings_.nightMode==(choice!=0));
        if(choice)assert(systemSettings_.darkColourScheme==choice-1);
        assert(uiDarkColourScheme==systemSettings_.darkColourScheme);
        if(choice)assert(colour(cCyan).full==lv_color_hex(darkColourSchemes[choice-1].accent).full);
    }
    controlColourScheme(0,activate);loadSystemSettings();
    assert(!uiNightMode && uiDarkColourScheme==3);
    applyTransmitterFunctionAction(static_cast<uint16_t>(TransmitterFunction::ToggleNightMode),1);
    assert(uiNightMode && uiDarkColourScheme==3);
    navigateBack();assert(activePage_==UiPage::DisplaySetup);
    // Corrupt or future palette IDs fall back safely; old night preferences work.
    {Preferences p;p.begin("jr-r1-system",false);p.putUChar("dark_scheme",255);p.end();}
    loadSystemSettings();assert(uiNightMode && uiDarkColourScheme==0);
    systemSettings_=originalSystem;saveSystemSettings();loadSystemSettings();
    // Exercise every established menu in the primary hardware orientation.
    const UiPage pages[]={UiPage::Dashboard,UiPage::Tune,UiPage::Telemetry,UiPage::Gps,UiPage::ChannelMonitor,UiPage::MainMenu,UiPage::ModelSelect,UiPage::Channels,UiPage::ChannelRoles,UiPage::Channel2,UiPage::ThrottleResponse,UiPage::ChannelSpeed,UiPage::ControlAssignments,UiPage::BindingEditor,UiPage::InputMonitor,UiPage::DriveModes,UiPage::ChannelPlaceholder,UiPage::Setup,UiPage::TxBatterySetup,UiPage::RxBatterySetup,UiPage::DisplaySetup,UiPage::ColourScheme,UiPage::FirmwareInfo,UiPage::ElrsBinding,UiPage::ElrsTxConfig,UiPage::EscProgramming,UiPage::WidgetSelect,UiPage::WidgetEditor,UiPage::RadioDiagnostics,UiPage::BatteryMenu,UiPage::Calibration,UiPage::OutputStatus,UiPage::ModeChooser,UiPage::ChannelInput,UiPage::ChannelResponseSetup,UiPage::ParameterControlPicker,UiPage::ResponseProfile,UiPage::MixingOverview,UiPage::NetworkOptions,UiPage::SystemMenu};
    for(auto page:pages){
        showSimulatorPage(page);lv_obj_update_layout(lv_scr_act());
        assert(activePage_==page && pageFocusCount_>0);
        if(page==UiPage::RadioDiagnostics){
            if(const char *path=getenv("JR_R1_RADIO_SCREENSHOT")){
                lv_refr_now(nullptr);simulatorSaveScreenshot(path);
            }
        }
        for(uint8_t i=0;i<pageFocusCount_;++i){const auto &r=pageFocusTargets_[i].focus;
            if(r.x<0 || r.y<0 || r.x+r.width>kScreenWidth || r.y+r.height>kScreenHeight){Serial.printf("UI focus outside LCD: %s target %u\n",uiPageName(page),i);return false;}
        }
        lv_mem_monitor_t memory;lv_mem_monitor(&memory);assert(memory.free_size>8192);
    }
    // Startup faults remain non-dismissible; communication faults are visibly
    // different from held controls even if the unsafe-control mask is unchanged.
    PhysicalInputStateStore startupStates;
    uint32_t unsafe = 0;
    for (uint8_t i=3;i<=9;++i) {
        startupStates.setFaulted(static_cast<PhysicalControlId>(i),true);
        unsafe |= uint32_t{1} << i;
    }
    setPhysicalInputStates(startupStates);
    armRequested_=true;
    setStartupPreflight(false,unsafe);
    assert(activePage_==UiPage::StartupWarning && !armRequested_ && startupFaultMask_==unsafe);
    assert(pageFocusCount_==0);
    lv_obj_update_layout(lv_scr_act());
    const auto childCount=lv_obj_get_child_cnt(lv_scr_act());
    for(uint32_t i=0;i<childCount;++i) {
        auto child=lv_obj_get_child(lv_scr_act(),i);
        if(lv_obj_check_type(child,&lv_label_class))
            assert(lv_obj_get_y(child)+lv_obj_get_height(child)<=(portraitUi?460:320));
    }
    lv_refr_now(nullptr);
    if(const char *path=getenv("JR_R1_AUDIT_STARTUP_SCREENSHOT")) simulatorSaveScreenshot(path);
    for (uint8_t i=3;i<=9;++i) startupStates.setFaulted(static_cast<PhysicalControlId>(i),false);
    setPhysicalInputStates(startupStates);setStartupPreflight(false,unsafe);
    assert(startupFaultMask_==0 && activePage_==UiPage::StartupWarning && !armRequested_);
    setStartupPreflight(true,0);assert(activePage_==UiPage::Dashboard && !armRequested_);
    // Measure real LVGL dirty-region rendering, not calls to our update helpers.
    const auto savedLayout = dashboardLayouts_[activeModelSlot_];
    const auto savedSampleTime = nextSensorSampleMs_;
    dashboardLayouts_[activeModelSlot_] = DashboardLayout{};
    auto &powerWidget = dashboardLayouts_[activeModelSlot_].cells[0];
    powerWidget.kind = DashboardWidgetKind::Power;
    auto &channelWidget = dashboardLayouts_[activeModelSlot_].cells[1];
    channelWidget.kind = DashboardWidgetKind::Channels;
    channelWidget.style = DashboardWidgetStyle::Bars;
    CrsfTelemetryState fixedTelemetry{};
    fixedTelemetry.linkValid = true;
    fixedTelemetry.uplinkLinkQuality = 100;
    fixedTelemetry.receiverVoltageValid = true;
    fixedTelemetry.receiverVoltageDecivolts = 66;
    nextSensorSampleMs_ = millis() + 100000;
    drawDashboard();
    updateDesign(fixedTelemetry, OutputSafetyStatus::Cut, 99, true);
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    auto *auditDisplay = lv_disp_get_default();
    auto previousMonitor = auditDisplay->driver->monitor_cb;
    auditDisplay->driver->monitor_cb = auditRefreshMonitor;
    auditRedrawnPixels = 0;
    for (int i = 0; i < 20; ++i) {
        updateDesign(fixedTelemetry, OutputSafetyStatus::Cut, 99, true);
        lv_refr_now(nullptr);
    }
    Serial.printf("UI redraw benchmark: 20 unchanged dashboard updates = %llu pixels\n",
                  static_cast<unsigned long long>(auditRedrawnPixels));
    assert(auditRedrawnPixels == 0);
    // Changing data and status must still repaint immediately on the next tick.
    const auto savedChannelValue = dashboardChannelValues_[0];
    dashboardChannelValues_[0] = kCrsfMaximum;
    fixedTelemetry.receiverVoltageDecivolts = 67;
    fixedTelemetry.uplinkLinkQuality = 87;
    updateDesign(fixedTelemetry, OutputSafetyStatus::Armed, 99, true);
    lv_refr_now(nullptr);
    assert(auditRedrawnPixels > 0);
    assert(strcmp(lv_label_get_text(widgetViews_[0].values[1]), "6.7 V") == 0);
    assert(lv_bar_get_value(widgetViews_[1].bars[0]) == permilleFromCrsfValue(kCrsfMaximum));
    assert(strcmp(lv_label_get_text(designStatus_), "Drive") == 0);
    assert(lv_obj_has_flag(lv_obj_get_child(driveIcon_,0),LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(lv_label_get_text(designLink_), "87 LQ") == 0);
    auditRedrawnPixels = 0;
    fixedTelemetry.receiverVoltageValid = false;
    fixedTelemetry.linkValid = false;
    fixedTelemetry.linkUpdatedMs = 1;
    updateDesign(fixedTelemetry, OutputSafetyStatus::Cut, 99, true);
    lv_refr_now(nullptr);
    assert(auditRedrawnPixels > 0);
    assert(strcmp(lv_label_get_text(widgetViews_[0].values[1]), "--") == 0);
    assert(strcmp(lv_label_get_text(designStatus_), "Drive") == 0);
    assert(!lv_obj_has_flag(lv_obj_get_child(driveIcon_,0),LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(lv_label_get_text(designLink_), "LQ stale") == 0);
    // A blocked enable request remains visible without replacing the Drive label.
    armRequested_=true;
    updateDesign(fixedTelemetry,OutputSafetyStatus::LinkRequired,99,true);
    assert(strcmp(lv_label_get_text(designLink_),"Link wait")==0);
    assert(strcmp(lv_label_get_text(designStatus_),"Drive")==0);
    assert(!lv_obj_has_flag(lv_obj_get_child(driveIcon_,0),LV_OBJ_FLAG_HIDDEN));
    drawDesignPage(UiPage::OutputStatus);
    updateDesign(fixedTelemetry,OutputSafetyStatus::LinkRequired,99,true);
    assert(strcmp(lv_label_get_text(designDetails_[0]),"LINK WAIT")==0);
    assert(strcmp(lv_label_get_text(designDetails_[1]),"Waiting for receiver link.")==0);
    updateDesign(fixedTelemetry,OutputSafetyStatus::NeutralRequired,99,true);
    assert(strcmp(lv_label_get_text(designDetails_[0]),"NEUTRAL WAIT")==0);
    armRequested_=false;drawDashboard();lv_refr_now(nullptr);
    updateDesign(fixedTelemetry,OutputSafetyStatus::Cut,99,true);
    assert(strcmp(lv_label_get_text(designLink_),"LQ stale")==0);

    auditRedrawnPixels = 0;
    updateDesign(fixedTelemetry, OutputSafetyStatus::InputFault, 0, false);
    lv_refr_now(nullptr);
    assert(auditRedrawnPixels > 0);
    assert(strcmp(lv_label_get_text(designStatus_), "Drive") == 0);
    assert(lv_obj_get_style_bg_color(lv_obj_get_child(driveIcon_,1),0).full==colour(cRed).full);
    assert(lv_obj_get_style_text_font(designStatus_, LV_PART_MAIN) == fontFor(16));
    // Low voltage changes the warning colour as well as its text.
    updateDesign(fixedTelemetry, OutputSafetyStatus::Cut, 1, true);
    assert(strcmp(lv_label_get_text(designLink_), "TX LOW") == 0);
    assert(lv_obj_get_style_text_color(widgetViews_[0].values[0], LV_PART_MAIN).full == colour(cRed).full);
    // A stopped timer, unchanged dials and a max-speed widget also stay idle.
    dashboardLayouts_[activeModelSlot_].cells[0].style = DashboardWidgetStyle::Dials;
    dashboardLayouts_[activeModelSlot_].cells[1].kind = DashboardWidgetKind::Sensor;
    dashboardLayouts_[activeModelSlot_].cells[1].style = DashboardWidgetStyle::Numbers;
    dashboardLayouts_[activeModelSlot_].cells[1].speedValue = SpeedWidgetValue::Maximum;
    dashboardLayouts_[activeModelSlot_].cells[2].kind = DashboardWidgetKind::Timer;
    runTimerRunning_ = false;
    speedStatistics_.reset(millis() - 1);
    speedStatistics_.update(123, millis(), millis(), true);
    drawDashboard();
    updateDesign(fixedTelemetry, OutputSafetyStatus::Cut, 99, true);
    lv_refr_now(nullptr);
    auditRedrawnPixels = 0;
    updateDesign(fixedTelemetry, OutputSafetyStatus::Cut, 99, true);
    lv_refr_now(nullptr);
    assert(auditRedrawnPixels == 0);
    assert(strcmp(lv_label_get_text(widgetViews_[1].values[0]), "12.3") == 0);
    // Formatted updates must preserve text beyond the small stack buffer.
    char longText[200];memset(longText, 'x', sizeof(longText)-1);longText[199] = 0;
    uiSetTextFmt(widgetViews_[1].values[0], "%s", longText);
    assert(strcmp(lv_label_get_text(widgetViews_[1].values[0]), longText) == 0);
    auditDisplay->driver->monitor_cb = previousMonitor;
    dashboardChannelValues_[0] = savedChannelValue;
    dashboardLayouts_[activeModelSlot_] = savedLayout;
    nextSensorSampleMs_ = savedSampleTime;
    // Rebuilt menus render identically. Cache must suppress their transfer,
    // while real navigation still sends changed pixels in both orientations.
    drawDesignPage(UiPage::MainMenu);lv_refr_now(nullptr);
    uiMetrics = UiFrameMetrics{};
    drawDesignPage(UiPage::MainMenu);lv_refr_now(nullptr);
    assert(uiMetrics.rendered > 0 && uiMetrics.sent == 0);
    uiMetrics = UiFrameMetrics{};
    drawDesignPage(UiPage::Tune);lv_refr_now(nullptr);
    Serial.printf("UI transition benchmark: Setup -> Tune rendered=%lu sent=%lu pixels build=%luus\n",
        (unsigned long)uiMetrics.rendered, (unsigned long)uiMetrics.sent, (unsigned long)uiMetrics.buildUs);
    assert(uiMetrics.sent > 0 && uiMetrics.sent < uiMetrics.rendered);
    if (const char *prefix = getenv("JR_R1_AUDIT_CACHE_IMAGES")) {
        char path[256];snprintf(path,sizeof(path),"%s-%s-cached.bmp",prefix,designPortrait?"portrait":"landscape");
        simulatorSaveScreenshot(path);
    }
    // Invalidation guarantees a complete transfer after wake/reinitialisation.
    pixelCache.invalidate();uiMetrics = UiFrameMetrics{};
    lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);
    assert(uiMetrics.sent == kScreenWidth * kScreenHeight);
    if (const char *prefix = getenv("JR_R1_AUDIT_CACHE_IMAGES")) {
        char path[256];snprintf(path,sizeof(path),"%s-%s-full.bmp",prefix,designPortrait?"portrait":"landscape");
        simulatorSaveScreenshot(path);
    }
    setDisplayCacheEnabled(false);
    drawDesignPage(UiPage::MainMenu);lv_refr_now(nullptr);
    drawDesignPage(UiPage::Tune);lv_refr_now(nullptr);
    assert(uiMetrics.sent == uiMetrics.rendered && uiMetrics.sent == kScreenWidth * kScreenHeight);
    if (const char *prefix = getenv("JR_R1_AUDIT_CACHE_IMAGES")) {
        char path[256];snprintf(path,sizeof(path),"%s-%s-off.bmp",prefix,designPortrait?"portrait":"landscape");
        simulatorSaveScreenshot(path);
    }
    setDisplayCacheEnabled(true);
    drawDesignPage(UiPage::Tune);lv_refr_now(nullptr);
    assert(uiMetrics.sent == kScreenWidth * kScreenHeight); // Re-enable primes unknown rows.
    drawDesignPage(UiPage::Tune);lv_refr_now(nullptr);
    assert(uiMetrics.sent == 0);
    // Setup must suppress every arm request, including a previously true one.
    SetupPortal::begin(this);controlDesign(152,activate);
    assert(activePage_==UiPage::WifiSetup && SetupPortal::active());
    char initialSetupPassword[17];strcpy(initialSetupPassword,SetupPortal::state().apPassword);
    SetupPortal::stop();SetupPortal::start();
    assert(strcmp(initialSetupPassword,SetupPortal::state().apPassword)==0);
    assert(!SetupPortal::changePassword("short"));
    assert(!SetupPortal::changePassword("contains spaces"));
    assert(!SetupPortal::changePassword("12345678901234567"));
    assert(SetupPortal::state().autoConnect);
    assert(SetupPortal::setAutoConnect(false));SetupPortal::begin(this);
    assert(!SetupPortal::state().autoConnect);
    assert(SetupPortal::setAutoConnect(true));SetupPortal::begin(this);
    assert(SetupPortal::state().autoConnect);
    controlWifi(15,activate);assert(wifiEdit_==4);
    strcpy(wifiSetupPassword_,"bench-test-1234");controlWifi(13,activate);
    assert(wifiEdit_==0 && strcmp(SetupPortal::state().apPassword,"bench-test-1234")==0);
    SetupPortal::forget();SetupPortal::stop();SetupPortal::begin(this);SetupPortal::start();
    assert(strcmp(SetupPortal::state().apPassword,"bench-test-1234")==0);
    assert(SetupPortal::changePassword(initialSetupPassword));SetupPortal::tick(true);
    armRequested_=true;assert(!controlConfigurationSnapshot().armRequested);
    navigateBack();assert(activePage_==UiPage::WifiSetup);
    armRequested_=false;
    assert(!SetupPortal::connect("Home","short"));
    controlWifi(3,activate);assert(wifiEdit_==3);
    controlWifi(70,activate);assert(wifiEdit_==2 && strcmp(wifiSsid_,"Home Wi-Fi")==0);
    controlWifi(20,activate);assert(strcmp(wifiPassword_,"1")==0);
    controlWifi(12,activate);assert(wifiPassword_[0]==0);
    controlWifi(10,activate);controlWifi(10,activate);controlWifi(20,activate);
    assert(strcmp(wifiPassword_,"!")==0);
    controlWifi(13,activate);
    const auto savedProfile=channelProfiles_[0];const auto savedParameters=channelParameters_[0];
    auto edited=savedProfile;edited.negativeEndpointPercent=73;edited.positiveEndpointPercent=92;
    assert(portalSetChannel(1,edited,10,-20));assert(channelProfiles_[0].negativeEndpointPercent==73 && channelProfiles_[0].positiveEndpointPercent==92);
    assert(portalSetChannel(1,savedProfile,savedParameters.trimPermille,savedParameters.subtrimPermille));
    // Web assignments share the resolver, model persistence and CUT gate.
    ControlBinding savedBindings[kPhysicalControlCount];
    const size_t savedBindingCount=bindingCount_;
    memcpy(savedBindings,bindings_,sizeof(savedBindings));
    const auto savedRoles=channelRoleSettings_;
    bindingCount_=0;
    assert(portalSetBinding(2,4,3,1,ContextDashboard,true));
    assert(portalBinding(2).crsfChannel==3 && !armRequested_);
    loadBindings();assert(portalBinding(2).crsfChannel==3 && portalBinding(2).enabled);
    assert(!portalSetBinding(3,4,3,3,ContextAlways,true)); // Duplicate writer.
    assert(!portalSetBinding(2,4,channelRoleSettings_.throttleChannel,1,ContextAlways,true));
    assert(!portalSetBinding(4,1,0,1,ContextAlways,true)); // Trim cannot fire a function.
    assert(!portalSetBinding(2,4,4,4,ContextAlways,true)); // Button cannot be a rocker.
    assert(!portalSetBinding(2,1,65535,1,ContextAlways,true));
    assert(portalSetBinding(2,1,static_cast<unsigned>(TransmitterFunction::ResetSpeedStatistics),1,ContextDashboard,true));
    assert(portalSetBinding(4,2,encodeChannelParameterTarget(3,ChannelParameterKind::Trim),5,ContextAlways,true));
    assert(!portalSetBinding(5,2,encodeChannelParameterTarget(3,ChannelParameterKind::Trim),5,ContextAlways,true));
    ControlBinding gain;gain.minimum=0;gain.maximum=600;gain.step=50;gain.initial=gain.failsafe=0;
    assert(portalSetBinding(9,4,5,3,ContextAlways,true,&gain));
    loadBindings();assert(portalBinding(9).minimum==0&&portalBinding(9).maximum==600&&portalBinding(9).failsafe==0);
    // Invalid drafts never replace a saved assignment.
    gain.initial=700;assert(!portalSetBinding(9,4,5,3,ContextAlways,true,&gain));
    assert(portalBinding(9).initial==0&&portalBinding(9).maximum==600);
    gain.initial=0;gain.minimum=-1001;assert(!portalSetBinding(9,4,5,3,ContextAlways,true,&gain));
    gain.minimum=-100;gain.maximum=100;gain.step=50;
    assert(portalSetBinding(4,2,encodeChannelParameterTarget(3,ChannelParameterKind::Trim),5,ContextAlways,true,&gain));
    assert(portalSetBinding(4,2,encodeChannelParameterTarget(3,ChannelParameterKind::Trim),5,ContextAlways,true));
    loadBindings();assert(portalBinding(4).minimum==-100&&portalBinding(4).maximum==100&&portalBinding(4).step==50);
    assert(portalSetRole(0,4,true));loadChannelRoleSettings();assert(channelRoleSettings_.steeringChannel==4);
    assert(!portalSetRole(1,4,true));
    memcpy(bindings_,savedBindings,sizeof(savedBindings));bindingCount_=savedBindingCount;saveBindings();
    channelRoleSettings_=savedRoles;saveChannelRoleSettings();
    char previousName[kModelNameCapacity];strcpy(previousName,portalModelName());
    bootPinUnlocked_=false;
    assert(!portalRemoteInput(0,0,2));
    assert(portalSetChannel(1,channelProfiles_[0],channelParameters_[0].trimPermille,channelParameters_[0].subtrimPermille));
    assert(!bootPinUnlocked()&&!controlConfigurationSnapshot().armRequested);
    assert(portalRenameModel("BENCH TEST"));assert(strcmp(portalModelName(),"BENCH TEST")==0);
    assert(!portalRenameModel("TOO LONG MODEL NAME"));assert(portalRenameModel(previousName));
    assert(!bootPinUnlocked());
    drawBootPin();
    const auto remoteKey=[&](int index){
        const int w=portraitUi?272:288,cell=(w-12)/3,h=portraitUi?56:34;
        return portalRemoteInput((kScreenWidth-w)/2+(index%3)*(cell+6)+cell/2,
                                 (portraitUi?168:126)+(index/3)*(h+6)+h/2,0);
    };
    assert(!portalRemoteInput(0,0,1)&&!portalRemoteInput(0,0,2));
    for(int i=0;i<4;++i)assert(remoteKey(0));
    assert(remoteKey(11));assert(!bootPinUnlocked()&&bootPinError_);
    assert(remoteKey(0));assert(remoteKey(9));assert(!bootPinDigits_[0]);
    for(int i=0;i<4;++i)assert(remoteKey(i));
    assert(remoteKey(11));assert(bootPinUnlocked()&&!armRequested());
    assert(!controlConfigurationSnapshot().armRequested);
    drawWifiPage();
    lv_refr_now(nullptr);
    if(const char *prefix=getenv("JR_R1_AUDIT_WIFI_IMAGES")) {
        char path[256];snprintf(path,sizeof(path),"%s-%s.bmp",prefix,designPortrait?"portrait":"landscape");simulatorSaveScreenshot(path);
        controlWifi(5,activate);lv_refr_now(nullptr);
        snprintf(path,sizeof(path),"%s-%s-keyboard.bmp",prefix,designPortrait?"portrait":"landscape");simulatorSaveScreenshot(path);
        updateWifiKeyPreview(10,designPortrait?140:120);
        assert(wifiKeyPreview_ && !lv_obj_has_flag(wifiKeyPreview_,LV_OBJ_FLAG_HIDDEN));
        assert(strcmp(lv_label_get_text(wifiKeyPreview_),"1")==0);
        lv_refr_now(nullptr);
        snprintf(path,sizeof(path),"%s-%s-key-preview.bmp",prefix,designPortrait?"portrait":"landscape");simulatorSaveScreenshot(path);
        updateWifiKeyPreview(-1,-1);
        assert(lv_obj_has_flag(wifiKeyPreview_,LV_OBJ_FLAG_HIDDEN));
    }
    wifiEdit_=0;drawWifiPage();
    assert(wifiStatusIcon);assert(strcmp(lv_label_get_text(wifiStatusIcon),LV_SYMBOL_WIFI)==0);
    assert(SetupPortal::connect("Home Wi-Fi","test-password"));updateWifiStatusIcon();assert(wifiIconState==3);
    SetupPortal::begin(this);assert(SetupPortal::hasSavedNetwork("Home Wi-Fi"));
    assert(SetupPortal::connect("Home Wi-Fi",""));
    {Preferences stored;assert(stored.begin("r1-wifi",true));assert(strcmp(stored.getString("password","").c_str(),"test-password")==0);stored.end();}
    controlWifi(3,activate);controlWifi(70,activate);assert(wifiEdit_==0);

    assert(portalEnableScreen());assert(!portalRemoteInput(-1,0,0));
    assert(portalRemoteInput(20,20,0));assert(activePage_==UiPage::WifiSetup&&SetupPortal::active());
    assert(portalRemoteInput(kScreenWidth-18,20,0));
    assert(activePage_==UiPage::Dashboard&&SetupPortal::active()&&!armRequested_);
    assert(portalRemoteInput(0,0,2));

    assert(portalRemoteInput(0,0,1));assert(activePage_==UiPage::Dashboard);
    assert(SetupPortal::active()&&!controlConfigurationSnapshot().armRequested);
    designTouchDispatch_=true;armGestureValid_=true;lastTouchDurationMs_=2000;controlDesign(5,activate);
    designTouchDispatch_=false;assert(!armRequested_);
    drawMainMenu();controlNavigation(1,activate);assert(activePage_==UiPage::Dashboard);
    assert(portalRemoteInput(0,0,2));assert(activePage_==UiPage::WifiSetup);
    wifiEdit_=1;assert(!portalRemoteInput(0,0,1));wifiEdit_=0;
    touchWasDown_=true;assert(!portalRemoteInput(100,100,0));touchWasDown_=false;
    assert(portalRemoteInput(portraitUi?230:375,20,0));
    assert(SetupPortal::active()&&activePage_==UiPage::Dashboard&&!armRequested_);
    assert(portalRemoteInput(0,0,2));
    const int wifiY=portraitUi?162:140;
    assert(!portalRemoteInput(kScreenWidth/2+20,wifiY+45,0)); // Connect
    assert(!portalRemoteInput(30,wifiY+80,0)); // Forget
    assert(portalRemoteInput(30,wifiY+126,0)); // Open web access without reconnecting
    assert(wifiWebAccess_);
    const int credentialsY=portraitUi?328:274;
    assert(!portalRemoteInput(30,credentialsY+10,0)); // Change password
    assert(SetupPortal::active()&&SetupPortal::hasSavedNetwork("Home Wi-Fi"));
    assert(portalRemoteInput(kScreenWidth/2+20,credentialsY+10,0)); // Hostname editor
    assert(wifiEdit_==6&&SetupPortal::active());
    strcpy(wifiHostname_,"bad.local");controlWifi(13,activate);assert(wifiEdit_==6);
    strcpy(wifiHostname_,"My-Handset");controlWifi(13,activate);assert(wifiEdit_==0);
    assert(strcmp(SetupPortal::state().hostname,"my-handset")==0);
    assert(strcmp(SetupPortal::state().activeHostname,"r1evo")==0); // No connection change before restart
    SetupPortal::begin(this);assert(strcmp(SetupPortal::state().activeHostname,"my-handset")==0);
    assert(SetupPortal::setHostname("r1evo"));SetupPortal::begin(this);
    assert(portalRemoteInput(portraitUi?230:375,20,0)); // Header still closes setup
    assert(SetupPortal::active()&&activePage_==UiPage::Dashboard&&!armRequested_);
    controlWifi(2,activate);assert(!SetupPortal::active()&&!armRequested_); // Physical close

    // Background home Wi-Fi serves the actual screen without opening setup.
    SetupPortal::tick(true);assert(!SetupPortal::active()&&SetupPortal::state().running);
    assert(portalEnableScreen());assert(portalRenameModel("WEB CUT"));
    {
        for(UiPage page : {UiPage::Tune,UiPage::Telemetry}) {
            drawDesignPage(page);int x,y,w,h;assert(portalDriveHoldBounds(x,y,w,h));
            assert(portalRemoteInput(x+w/2,y+h/2,0)&&activePage_==UiPage::Dashboard&&!armRequested_);
        }
        drawDashboard();int x,y,w,h;assert(portalDriveHoldBounds(x,y,w,h));
        const int px=x+w/2,py=y+h/2;
        assert(portalRemoteInput(px,py,0)&&!armRequested_); // tap only navigates
        drawDashboard();assert(portalRemoteInput(px,py,3));auto hold=portalRemoteHoldId();
        assert(!portalRemoteInput(px,py,4,hold)&&!armRequested_); // too short
        drawDashboard();assert(portalRemoteInput(px,py,3));hold=portalRemoteHoldId();
        delay(300);assert(portalRemoteInput(px,py,6,hold));
        delay(300);assert(portalRemoteInput(px,py,6,hold));delay(250);
        assert(portalRemoteInput(px,py,4,hold)&&armRequested_);
        assert(controlConfigurationSnapshot().armRequested); // existing safety engine decides LIVE
        armRequested_=false;safetyResetRequested_=true;SetupPortal::tick(true);
        drawDashboard();assert(portalRemoteInput(px,py,3));hold=portalRemoteHoldId();
        assert(portalRemoteInput(px,py,5,hold));delay(850);
        assert(!portalRemoteInput(px,py,4,hold)&&!armRequested_);
        bootPinUnlocked_=false;assert(!portalRemoteInput(px,py,3));bootPinUnlocked_=true;
        touchWasDown_=true;assert(!portalRemoteInput(px,py,3));touchWasDown_=false;
        drawDashboard();assert(portalRemoteInput(px,py,3));hold=portalRemoteHoldId();
        drawMainMenu();delay(850);assert(!portalRemoteInput(px,py,4,hold)&&!armRequested_);
    }

    for(bool armed : {false,true}) {
        armRequested_=armed;SetupPortal::tick(!armed);drawDashboard();
        logoSaverReturnPage_=UiPage::Dashboard;drawLogoSaver();
        assert(portalScreenNeedsWake());int x,y,w,h;assert(!portalDriveHoldBounds(x,y,w,h));
        assert(portalWakeScreen()&&activePage_==UiPage::Dashboard&&armRequested_==armed);
        assert(!portalScreenNeedsWake()&&!portalWakeScreen());
        sleepDisplay();assert(portalWakeScreen()&&!displaySleeping_&&armRequested_==armed);
    }
    SetupPortal::tick(true);
    armRequested_=true;SetupPortal::tick(false);drawMainMenu();
    assert(!portalRemoteInput(100,100,0)); // edits still gated before CUT acknowledgement
    assert(portalRequestRemoteControl()&&!armRequested_&&safetyResetRequested_);
    assert(!SetupPortal::canControl()&&!portalRemoteInput(100,100,0));
    SetupPortal::tick(true); // control loop has acknowledged CUT
    assert(portalRemoteInput(100,100,0)&&!armRequested_);
    SetupPortal::tick(false);assert(!SetupPortal::requestRestart());assert(portalEnableScreen());
    assert(!portalSetChannel(1,edited,0,0));assert(!portalRenameModel("CLOSED"));
    SetupPortal::tick(true);assert(portalRenameModel("HYPER 7"));
    assert(SetupPortal::requestRestart());
    SetupPortal::tick(false); // Cancel a queued restart if outputs cease to be safe.
    SetupPortal::tick(true);
    // Exercise the RGB565 scanline renderer across both display orientations.
    OfflineMap::Package raster{};raster.valid=true;raster.width=raster.height=2;
    raster.south=GpsMap::OriginLat-10000;raster.north=GpsMap::OriginLat+10000;
    raster.west=GpsMap::OriginLon-10000;raster.east=GpsMap::OriginLon+10000;
    strcpy(raster.attribution,"Raster rendering test");
    auto *colours=OfflineMap::allocatePixels(4);assert(colours);
    colours[0]=0xf800;colours[1]=0x07e0;colours[2]=0x001f;colours[3]=0xffff;
    OfflineMap::install(raster,colours);
    GpsMapViewport rasterView;
    auto *rasterObject=createGpsBasemap(lv_scr_act(),0,0,200,200,rasterView);
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    if(const char *path=getenv("JR_R1_AUDIT_RASTER_SCREENSHOT"))simulatorSaveScreenshot(path);
    lv_obj_del(rasterObject);
    gpsCount_=0;gpsZoom_=1;CrsfTelemetryState noGps{};
    drawDesignPage(UiPage::Gps);updateDesign(noGps,OutputSafetyStatus::Cut,99,true);
    auto gpsMap=lv_obj_get_parent(designCurve_);lv_obj_update_layout(lv_scr_act());
    assert(lv_obj_get_height(gpsMap)==designFooterY-88);
    const double firstScale=designMapViewport_.pixelsPerMetre;
    assert(firstScale==gpsBasemapPreviewScale(gpsMap,true));
    controlDesign(83,activate);updateDesign(noGps,OutputSafetyStatus::Cut,99,true);
    assert(designMapViewport_.pixelsPerMetre>firstScale); // Zoom before any GPS fix.
    controlDesign(82,activate);updateDesign(noGps,OutputSafetyStatus::Cut,99,true);
    lastInteractionMs_=millis();updateGpsPresentation(lastInteractionMs_+3999);assert(!gpsFullScreen_);
    touchWasDown_=true;updateGpsPresentation(lastInteractionMs_+4001);assert(!gpsFullScreen_);touchWasDown_=false;
    updateGpsPresentation(lastInteractionMs_+4001);assert(gpsFullScreen_);
    gpsMap=lv_obj_get_parent(designCurve_);lv_obj_update_layout(lv_scr_act());
    assert(lv_obj_get_width(gpsMap)==kScreenWidth&&lv_obj_get_height(gpsMap)==kScreenHeight);
    assert(!driveProgress_&&!designStatus_); // No hidden output-enable targets.
    assert(lv_obj_get_child_cnt(lv_scr_act())==1); // Only the map, including portrait footer area.
    const int oldZoom=gpsZoom_;const bool oldTrail=gpsTrailVisible_;
    handleTouch(20,kScreenHeight-20); // Reveals only, even on the old Drive target.
    assert(!gpsFullScreen_&&gpsZoom_==oldZoom&&gpsTrailVisible_==oldTrail&&!armRequested_);
    OfflineMap::install(OfflineMap::Package{});
    if(const char *prefix=getenv("JR_R1_GPS_LAYOUT_SCREENSHOTS")){
        drawDesignPage(UiPage::Gps);updateDesign(noGps,OutputSafetyStatus::Cut,99,true);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        char path[256];snprintf(path,sizeof(path),"%s-controls.bmp",prefix);simulatorSaveScreenshot(path);
        gpsFullScreen_=true;drawDesignPage(UiPage::Gps);updateDesign(noGps,OutputSafetyStatus::Cut,99,true);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        snprintf(path,sizeof(path),"%s-full.bmp",prefix);simulatorSaveScreenshot(path);
        handleTouch(20,20);
    }

    // Hardware recovery keeps authenticated remote navigation available without
    // claiming startup preflight passed or permitting any arm request.
    enterHardwareRecovery();
    bootPinUnlocked_=false;startupPreflightComplete_=false;
    bootPinDigits_[0]=0;
    drawBootPin();SetupPortal::tick(true,true);
    assert(portalEnableScreen());
    assert(!portalRemoteInput(0,0,2));
    for(int d:{1,2,3,4})controlBootPin(d,activate);
    controlBootPin(11,activate);
    assert(bootPinUnlocked() && activePage_==UiPage::Dashboard);
    assert(!startupPreflightComplete_);
    armRequested_=true;
    assert(!armRequested() && !controlConfigurationSnapshot().armRequested);
    setRfBenchMode(true);
    assert(!armRequested() && !controlConfigurationSnapshot().armRequested);
    hardwareRecovery_=false;
    assert(!armRequested() && !controlConfigurationSnapshot().armRequested);
    hardwareRecovery_=true;
    setRfBenchMode(false); // Only the serial lease can request bench arming.
    armRequested_=false;
    assert(portalRemoteInput(0,0,2));
    assert(activePage_==UiPage::WifiSetup);
    const bool transport=lcdTransportReady;lcdTransportReady=false;
    lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);
    lcdTransportReady=transport;
    assert(!armRequested());
    Serial.println("Hardware recovery: PIN, remote navigation, offscreen rendering and arm inhibition PASS");
    Serial.println("LVGL UI audit: navigation, drafts, widgets, model guards, timer, endpoints, colour schemes, startup fault display, arm hold and bounds PASS");
    dashboardEnabledChannelMask_=0xffff;
    for(int ch=0;ch<16;++ch)dashboardChannelValues_[ch]=crsfValueFromPermille(ch*100-750);
    for(int size=0;size<4;++size){
        dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
        channelWidgetPages_[0]=0; // Each size tests its first-page transition independently.
        auto &widget=dashboardLayouts_[activeModelSlot_].cells[0];
        widget.kind=DashboardWidgetKind::AllChannels;widget.style=DashboardWidgetStyle::Bars;
        widget.width=(size&1)?2:1;widget.height=(size&2)?2:1;
        drawDashboard();lv_obj_update_layout(lv_scr_act());
        updateDashboardWidgets(designTelemetry_,designTxVoltage_,designTxValid_);lv_refr_now(nullptr);
        auto &view=widgetViews_[0];
        const int cap=dashboardChannelCapacity(lv_obj_get_width(view.panel),lv_obj_get_height(view.panel));
        for(int j=0;j<cap;++j){assert(view.values[j]);assert(lv_obj_get_y(view.bars[j])+4<=lv_obj_get_height(view.panel)-16);}
        controlDesign(130,activate);
        assert(channelWidgetPages_[0]==(cap<16?1:0));
        if(size==3)if(const char *path=getenv("JR_R1_ALL_CHANNELS_SCREENSHOT"))simulatorSaveScreenshot(path);
    }
    for(int size=0;size<4;++size){
        dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
        channelWidgetPages_[0]=0;
        auto &widget=dashboardLayouts_[activeModelSlot_].cells[0];
        widget.kind=DashboardWidgetKind::AllChannels;widget.style=DashboardWidgetStyle::VerticalBars;
        widget.width=(size&1)?2:1;widget.height=(size&2)?2:1;
        drawDashboard();lv_obj_update_layout(lv_scr_act());
        updateDashboardWidgets(designTelemetry_,designTxVoltage_,designTxValid_);lv_refr_now(nullptr);
        auto &view=widgetViews_[0];
        const int cap=dashboardChannelCapacity(lv_obj_get_width(view.panel),lv_obj_get_height(view.panel),widget.style);
        for(int j=0;j<cap;++j){
            assert(view.values[j]&&view.bars[j]);
            assert(lv_obj_get_width(view.bars[j])<lv_obj_get_height(view.bars[j]));
            assert(lv_obj_get_y(view.bars[j])+lv_obj_get_height(view.bars[j])<=lv_obj_get_height(view.panel)-16);
            assert(lv_obj_get_y(view.values[j])+lv_obj_get_height(view.values[j])<=lv_obj_get_y(view.bars[j]));
            assert(lv_bar_get_value(view.bars[j])==permilleFromCrsfValue(dashboardChannelValues_[j]));
        }
        if(size==3)if(const char *path=getenv("JR_R1_ALL_CHANNELS_VERTICAL_SCREENSHOT"))simulatorSaveScreenshot(path);
        controlDesign(130,activate);
        assert(channelWidgetPages_[0]==(cap<16?1:0));
    }
    auto &switchWidget=dashboardLayouts_[activeModelSlot_].cells[0];
    switchWidget.source=1;channelWidgetPages_[0]=0;
    auto oldDrive=physicalInputStates_[3];physicalInputStates_[3].mapped=true;physicalInputStates_[3].faulted=false;physicalInputStates_[3].stablePrimary=true;
    drawDashboard();lv_obj_update_layout(lv_scr_act());updateDashboardWidgets(designTelemetry_,designTxVoltage_,designTxValid_);
    assert(widgetViews_[0].switches[0]&&widgetViews_[0].switches[1]&&widgetViews_[0].switches[2]);
    assert(lv_obj_has_state(widgetViews_[0].switches[0],LV_STATE_CHECKED));
    assert(!lv_obj_has_flag(widgetViews_[0].switches[0],LV_OBJ_FLAG_CLICKABLE));
    assert(strstr(lv_label_get_text(widgetViews_[0].values[0]),"ON"));physicalInputStates_[3]=oldDrive;
    auto &grip=physicalInputStates_[4];auto oldGrip=grip;
    grip.mapped=true;grip.faulted=false;grip.stablePrimary=true;grip.stableSecondary=false;
    updateDashboardWidgets(designTelemetry_,designTxVoltage_,designTxValid_);
    assert(widgetViews_[0].bars[3]&&lv_bar_get_value(widgetViews_[0].bars[3])==-1000);
    grip.stablePrimary=false;grip.stableSecondary=true;
    updateDashboardWidgets(designTelemetry_,designTxVoltage_,designTxValid_);
    assert(lv_bar_get_value(widgetViews_[0].bars[3])==1000);
    grip.stableSecondary=false;updateDashboardWidgets(designTelemetry_,designTxVoltage_,designTxValid_);
    assert(lv_bar_get_value(widgetViews_[0].bars[3])==0);
    grip.stablePrimary=grip.stableSecondary=true;
    updateDashboardWidgets(designTelemetry_,designTxVoltage_,designTxValid_);
    assert(strstr(lv_label_get_text(widgetViews_[0].values[3]),"--"));
    grip=oldGrip;
    auto oldTimer=physicalInputStates_[2];auto oldB=physicalInputStates_[5];
    physicalInputStates_[2].mapped=true;physicalInputStates_[2].faulted=false;physicalInputStates_[2].stablePrimary=true;
    physicalInputStates_[5].mapped=true;physicalInputStates_[5].faulted=false;physicalInputStates_[5].stablePrimary=false;physicalInputStates_[5].stableSecondary=true;
    updateDashboardWidgets(designTelemetry_,designTxVoltage_,designTxValid_);lv_refr_now(nullptr);
    assert(lv_obj_has_state(widgetViews_[0].switches[2],LV_STATE_CHECKED));
    assert(lv_bar_get_value(widgetViews_[0].bars[4])==1000);
    if(const char *path=getenv("JR_R1_INPUT_WIDGET_SCREENSHOT"))simulatorSaveScreenshot(path);
    physicalInputStates_[2]=oldTimer;physicalInputStates_[5]=oldB;
    controlDesign(110,activate);
    widgetDraft_.kind=DashboardWidgetKind::AllChannels;widgetDraft_.source=2;
    const auto oldHidden=dashboardHidden_[activeModelSlot_][0];
    controlDesign(169,activate);assert(widgetInputsEditing_);
    widgetInputsBank_=3;drawDesignPage(UiPage::WidgetEditor);lv_refr_now(nullptr);
    if(const char *path=getenv("JR_R1_SELECTION_SCREENSHOT"))simulatorSaveScreenshot(path);
    widgetInputsBank_=0;
    controlDesign(179,activate);assert((widgetHiddenDraft_&0x1fffff)==0x1fffff);
    controlDesign(170,activate);assert(!(widgetHiddenDraft_&1));
    controlDesign(177,activate);controlDesign(125,activate);
    assert(dashboardHidden_[activeModelSlot_][0]==oldHidden); // Cancel discards selections.
    controlDesign(110,activate);widgetDraft_.source=2;
    controlDesign(169,activate);controlDesign(179,activate);controlDesign(170,activate);
    controlDesign(177,activate);controlDesign(126,activate);
    assert((dashboardHidden_[activeModelSlot_][0]&0x1fffff)==0x1ffffe);
    assert(widgetViews_[0].values[0]&&!widgetViews_[0].values[1]);
    loadModelMemorySettings();assert((dashboardHidden_[activeModelSlot_][0]&0x1fffff)==0x1ffffe);
    dashboardHidden_[activeModelSlot_][0]=oldHidden;
    armRequested_=false;bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;touchWasDown_=false;
    const int initialWidth=kScreenWidth;size_t rotationFree=0;
    for(int cycle=0;cycle<8;++cycle){
        displaySetupBank_=1;drawDisplayPage();controlDisplay(5,activate);assert(rotationPending_);
        applyDisplayRotation();assert(kScreenWidth==((cycle%2)?initialWidth:(initialWidth==480?320:480)));
        assert(kScreenWidth*kScreenHeight==153600&&designFooterY==kScreenHeight-(portraitUi?60:40));
        assert(lv_disp_get_hor_res(registeredDisplay)==kScreenWidth&&lv_disp_get_ver_res(registeredDisplay)==kScreenHeight);
        for(UiPage page : pages){
            showSimulatorPage(page);lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
            assert(activePage_==page);
            for(uint8_t i=0;i<pageFocusCount_;++i){const auto &r=pageFocusTargets_[i].focus;
                assert(r.x>=0&&r.y>=0&&r.x+r.width<=kScreenWidth&&r.y+r.height<=kScreenHeight);
            }
        }
        drawDashboard();lv_refr_now(nullptr);
        lv_mem_monitor_t memory;lv_mem_monitor(&memory);assert(memory.free_size>8192);
        if(cycle==1)rotationFree=memory.free_size;
        if(cycle==7)assert(memory.free_size+512>=rotationFree);
        if(const char *prefix=getenv("JR_R1_ROTATION_SCREENSHOTS")){
            if(cycle<2){char path[256];snprintf(path,sizeof(path),"%s-%dx%d.bmp",prefix,kScreenWidth,kScreenHeight);simulatorSaveScreenshot(path);}
        }
    }
    armRequested_=true;assert(!requestDisplayRotation());armRequested_=false;
    remoteArmHold_.begin(1,millis());assert(!requestDisplayRotation());remoteArmHold_.cancel();
    Serial.println("Runtime rotation: all pages, both dimensions, memory stability and arm/hold guards PASS");
    drawDashboard();return true;
}
