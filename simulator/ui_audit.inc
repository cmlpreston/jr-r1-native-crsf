// Integration checks exercise the actual UI callbacks and persistence. Run
// only with JR_R1_SIM_STATE pointed at a disposable directory.
#include <cassert>
#include <cstdlib>
static uint64_t auditRedrawnPixels = 0;
static unsigned auditCutRequests = 0;
static void auditConfigurationCut() { ++auditCutRequests; }
static void auditRefreshMonitor(lv_disp_drv_t *, uint32_t, uint32_t pixels)
{
    auditRedrawnPixels += pixels;
}
static void auditTypography(lv_obj_t *object)
{
    if(lv_obj_check_type(object,&lv_label_class)){
        const auto *font=lv_obj_get_style_text_font(object,LV_PART_MAIN);
        bool allowed=false;
        for(auto style:{UiText::Caption,UiText::Secondary,UiText::Menu,UiText::Heading,UiText::Reading,UiText::Hero})
            allowed|=font==UiText::font(style);
        assert(allowed && "Rendered labels must use the shared six-style scale");
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);++i)auditTypography(lv_obj_get_child(object,i));
}
bool TransmitterUi::runSimulatorUiAudit()
{
    if(!getenv("JR_R1_SIM_STATE")){Serial.println("UI audit requires disposable JR_R1_SIM_STATE");return false;}
    const auto activate=PageControlEvent::Activate;
    const auto touchAction=[&](PageControlHandler handler,uint8_t argument){
        for(uint8_t i=0;i<pageFocusCount_;++i){const auto t=pageFocusTargets_[i];
            if(t.handler==handler && t.argument==argument){
                handleTouch(t.activate.x+t.activate.width/2,t.activate.y+t.activate.height/2);return true;}}
        return false;
    };
    // Boot PIN cannot be bypassed by bound arm actions or remote taps.
    assert(!bootPinUnlocked());drawBootPin();
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    if(const char *path=getenv("JR_R1_PIN_SCREENSHOT"))simulatorSaveScreenshot(path);
    assert(touchAction(&TransmitterUi::controlBootPin,13));
    assert(pinRecoveryHelp_&&activePage_==UiPage::BootPin&&!bootPinUnlocked()&&!armRequested());
    auditTypography(lv_scr_act());
    if(const char *path=getenv("JR_R1_PIN_HELP_SCREENSHOT")){
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
    controlBootPin(1,activate);assert(!bootPinDigits_[0]);
    assert(touchAction(&TransmitterUi::controlBootPin,14));assert(!pinRecoveryHelp_&&!bootPinUnlocked());
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
    for(int d:{1,2,3})controlBootPin(d,activate);
    assert(!bootPinUnlocked()&&strlen(bootPinDigits_)==3);
    controlBootPin(4,activate);assert(bootPinUnlocked()&&!armRequested());
    assert(activePage_==UiPage::StartupWarning); // Unlock does not dismiss preflight.
    setStartupPreflight(true,0);assert(activePage_==UiPage::Dashboard);
    const auto auditDriveHold=[&](){
        SetupPortal::stop();drawDashboard();armRequested_=false;
        touchWasDown_=armGestureValid_=true;touchStartPage_=activePage_;touchDownStartedMs_=1000;
        updateDriveHoldFeedback(1400);lv_obj_update_layout(lv_scr_act());
        assert(driveHoldOverlay_&&!lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_get_width(driveHoldOverlay_)==kScreenWidth-20&&lv_bar_get_value(driveHoldBar_)==400);
        assert(strstr(lv_label_get_text(driveHoldLabel_),"Keep holding"));
        if(const char *prefix=getenv("JR_R1_DRIVE_HOLD_IMAGES")){
            lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];
            snprintf(path,sizeof(path),"%s-%s.bmp",prefix,portraitUi?"portrait":"landscape");simulatorSaveScreenshot(path);
        }
        updateDriveHoldFeedback(1800);assert(!armRequested_&&lv_bar_get_value(driveHoldBar_)==800);
        assert(strstr(lv_label_get_text(driveHoldLabel_),"Release to request"));
        armGestureValid_=false;updateDriveHoldFeedback(1801);assert(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN));
        touchWasDown_=false;remoteArmPage_=static_cast<uint32_t>(activePage_);
        auto hold=remoteArmHold_.begin(remoteArmPage_,2000);updateDriveHoldFeedback(2400);
        assert(lv_bar_get_value(driveHoldBar_)==400&&!armRequested_);
        remoteArmHold_.cancel();updateDriveHoldFeedback(2401);assert(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN));
        hold=remoteArmHold_.begin(remoteArmPage_,3000);updateDriveHoldFeedback(3901);
        assert(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN)&&!armRequested_);
        remoteArmHold_.cancel();(void)hold;
        SetupPortal::start();touchWasDown_=armGestureValid_=true;touchDownStartedMs_=4000;
        updateDriveHoldFeedback(4800);assert(strstr(lv_label_get_text(driveHoldLabel_),"Wi-Fi setup")&&!armRequested_);
        SetupPortal::stop();armRequested_=true;updateDriveHoldFeedback(4800);
        assert(strstr(lv_label_get_text(driveHoldLabel_),"Release to CUT")&&armRequested_);
        touchWasDown_=armGestureValid_=armRequested_=false;drawDashboard();
        assert(!driveHoldOverlay_); // A new page owns no stale overlay objects.
        Serial.println("Drive hold: wide progress, release cue, blockers, cancel/expiry, remote preview and no auto-arm PASS");
    };
    if(getenv("JR_R1_DRIVE_HOLD_AUDIT_ONLY")){auditDriveHold();return true;}
    const auto auditTxBattery=[&](){
        const auto saved=txBatterySettings_;
        txBatterySettings_.chemistry=BatteryChemistry::LiFe;txBatterySettings_.cellCount=3;
        txBatterySettings_.lowAlarmDecivolts=90;txBatterySettings_.voltageSenseEnabled=false;
        drawTxBatteryPage();assert(sectionNavigationVisible_&&!detailContentCompacted_);
        if(const char *prefix=getenv("JR_R1_TX_BATTERY_IMAGES")){
            lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];
            snprintf(path,sizeof(path),"%s-%s.bmp",prefix,portraitUi?"portrait":"landscape");simulatorSaveScreenshot(path);
        }
        const auto adjust=[&](uint8_t row,bool increase){
            for(uint8_t i=0;i<pageFocusCount_;++i){const auto t=pageFocusTargets_[i];
                if(t.handler==&TransmitterUi::controlTxBattery&&t.argument==row){
                    const auto r=increase?t.increase:t.decrease;assert(r.width>0);
                    dispatchPageControlTouch(r.x+r.width/2,r.y+r.height/2);return;}}
            assert(false);
        };
        adjust(1,false);assert(txBatterySettings_.cellCount==2);
        adjust(1,true);assert(txBatterySettings_.cellCount==3);
        adjust(2,false);assert(txBatterySettings_.lowAlarmDecivolts==89);
        adjust(2,true);assert(txBatterySettings_.lowAlarmDecivolts==90);
        assert(touchAction(&TransmitterUi::controlTxBattery,0)&&txBatterySettings_.chemistry==BatteryChemistry::LiPo);
        assert(touchAction(&TransmitterUi::controlTxBattery,3)&&txBatterySettings_.voltageSenseEnabled);
        loadTxBatterySettings();assert(txBatterySettings_.voltageSenseEnabled&&txBatterySettings_.chemistry==BatteryChemistry::LiPo);
        assert(touchAction(&TransmitterUi::controlNavigation,0)&&activePage_==UiPage::MainMenu);
        txBatterySettings_=saved;saveTxBatterySettings();drawDashboard();
        Serial.println("TX battery: shared rows, actual touch targets, chemistry, cells, alarm, sensing, persistence and Back PASS");
    };
    if(getenv("JR_R1_TX_BATTERY_AUDIT_ONLY")){auditTxBattery();return true;}
    const auto auditDisplay=[&](){
        const auto saved=systemSettings_;
        systemSettings_.screenTimeoutSeconds=60;systemSettings_.displayBrightnessPercent=100;
        displaySetupBank_=0;drawDisplayPage();
        assert(sectionNavigationVisible_&&!detailContentCompacted_);
        auto capture=[&](const char *page){if(const char *prefix=getenv("JR_R1_DISPLAY_IMAGES")){
            lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];
            snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",page);simulatorSaveScreenshot(path);
        }};
        capture("1");
        const auto decrease=[&](uint8_t row){
            for(uint8_t i=0;i<pageFocusCount_;++i){const auto t=pageFocusTargets_[i];
                if(t.handler==&TransmitterUi::controlDisplay&&t.argument==row){
                    assert(t.decrease.width>0);dispatchPageControlTouch(t.decrease.x+t.decrease.width/2,t.decrease.y+t.decrease.height/2);return;}}
            assert(false);
        };
        decrease(0);assert(systemSettings_.screenTimeoutSeconds==30);
        decrease(2);assert(systemSettings_.displayBrightnessPercent==90);
        assert(handlePageSwipe(250,150,100,154,400)&&displaySetupBank_==1);
        capture("2");
        assert(touchAction(&TransmitterUi::controlPageDot,0)&&displaySetupBank_==0);
        controlDisplay(3,activate);assert(activePage_==UiPage::ColourScheme);
        navigateBack();assert(activePage_==UiPage::DisplaySetup);
        assert(touchAction(&TransmitterUi::controlNavigation,0)&&activePage_==UiPage::MainMenu);
        systemSettings_=saved;saveSystemSettings();uiBrightnessPercent=saved.displayBrightnessPercent;applyBacklight(uiBrightnessPercent);
        displaySetupBank_=0;drawDashboard();
        Serial.println("Display settings: shared rows, actual +/- targets, swipe/dots, colour chooser and Back PASS");
    };
    if(getenv("JR_R1_DISPLAY_AUDIT_ONLY")){auditDisplay();return true;}
    const auto auditModuleLink=[&](){
        moduleLinkManaged_=false;moduleLink_=ModuleLink{};
        SetupPortal::start();drawDashboard();
        lastTouchDurationMs_=800;armGestureValid_=designTouchDispatch_=true;
        controlDesign(0,activate);designTouchDispatch_=armGestureValid_=false;
        assert(activePage_==UiPage::OutputStatus&&!armRequested_);
        assert(strstr(lv_label_get_text(designDetails_[1]),"Finish Wi-Fi setup"));
        assert(touchAction(&TransmitterUi::controlDesign,238));
        assert(!SetupPortal::active()&&!armRequested_);
        updateModulePower(false,1000);assert(!moduleLinkEnabled());
        controlDesign(239,activate);assert(activePage_==UiPage::ModuleLink);
        controlDesign(240,activate);assert(!moduleLinkEnabled()); // No supply.
        navigateBack();assert(!moduleLinkEnabled());drawDashboard();
        updateModulePower(true,1100);updateModulePower(true,1350);
        CrsfChannelOutputModel outputs;CrsfTelemetryState telemetry;
        update(THROTTLE_ADC_CENTER,STEERING_ADC_CENTER,0,0,CRSF_CHANNEL_MID,
               OutputSafetyStatus::Cut,telemetry,99,true,outputs,false);
        assert(activePage_==UiPage::ModuleLink&&!armRequested_);
        if(const char *prefix=getenv("JR_R1_MODULE_LINK_IMAGES")){
            lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];
            snprintf(path,sizeof(path),"%s-%s.bmp",prefix,portraitUi?"portrait":"landscape");simulatorSaveScreenshot(path);
        }
        remoteTouchActive_=true;controlDesign(240,activate);remoteTouchActive_=false;
        assert(!moduleLinkEnabled()); // A remote browser cannot enable it.
        controlDesign(241,activate);assert(!moduleLinkEnabled()&&!moduleLink_.prompt());
        controlDesign(239,activate);controlDesign(240,activate);
        assert(moduleLinkEnabled()&&!armRequested_&&!controlConfigurationSnapshot().armRequested);
        controlDesign(239,activate);armRequested_=true;controlDesign(242,activate);
        assert(moduleLinkEnabled()&&armRequested_); // Cannot withdraw the link live.
        armRequested_=false;controlDesign(242,activate);
        assert(!moduleLinkEnabled()&&!armRequested_);
        controlDesign(239,activate);controlDesign(240,activate);armRequested_=true;
        controlDesign(239,activate); // Keep the dialog open across a supply change.
        updateModulePower(false,1400);assert(!moduleLinkEnabled()&&!armRequested_);
        assert(discardTouchUntilRelease_); // A redrawn prompt needs a fresh press.
        updateModulePower(true,1500);updateModulePower(true,1750);
        assert(moduleLink_.prompt()&&!moduleLinkEnabled());
        moduleLinkManaged_=false;moduleLink_=ModuleLink{};remoteTouchRejected_=false;
        discardTouchUntilRelease_=touchWasDown_=false;touchIgnoreUntilMs_=0;
        lastTouchDurationMs_=0;drawDashboard();
        Serial.println("Module link UI: setup warning, physical-only approval, deferred prompt, pause and power loss PASS");
    };
    if(getenv("JR_R1_MODULE_LINK_AUDIT_ONLY")){auditModuleLink();return true;}
    const auto auditSportGps=[&](){
        CrsfTelemetryState sportGps{};sportGps.gpsValid=true;sportGps.satellites=0;
        sportGps.latitudeE7=GpsMap::DemoLat;sportGps.longitudeE7=GpsMap::DemoLon;
        sportGps.gpsUpdatedMs=millis();sportGps.groundSpeedDeciKmh=235;
        gpsCount_=0;gpsLastSampleMs_=sportGps.gpsUpdatedMs-1;
        designTelemetry_=sportGps;gpsFullScreen_=false;drawDesignPage(UiPage::Gps);lv_obj_update_layout(lv_scr_act());updateDesign(sportGps,OutputSafetyStatus::Cut,99,true);
        assert(gpsCount_==1 && strstr(lv_label_get_text(designDetails_[0]),"Fix unknown"));
        int32_t speed=0;uint32_t stamp=0;assert(sensorReading(sportGps,0,speed,stamp)&&speed==235);
        assert(!sensorReading(sportGps,5,speed,stamp)); // unknown count is not zero satellites
        sportGps.gpsValid=false;updateDesign(sportGps,OutputSafetyStatus::Cut,99,true);assert(gpsCount_==1);
        gpsCount_=0;assert(!armRequested_);
        Serial.println("S.Port GPS: unknown fix display, speed, map position and stale handling PASS");
    };
    if(getenv("JR_R1_GPS_AUDIT_ONLY")){auditSportGps();return true;}
    auditDriveHold();
    auditTxBattery();
    auditDisplay();
    auditModuleLink();

    // Real handset key timing, quick shortcuts, drafts and safety invariants.
    {
        uint32_t keyTime=millis();
        auto press=[&](PhysicalControlId key,bool held=false){
            assert(handleKeypadEvent(key,InputEvent::Pressed,keyTime));
            if(held)updateKeypad(keyTime+700);
            assert(handleKeypadEvent(key,InputEvent::Released,keyTime+(held?800:80)));
            keyTime+=held?900:150;
        };
        drawDashboard();armRequested_=true;safetyResetRequested_=false;
        press(PhysicalControlId::KeyMode);assert(activePage_==UiPage::QuickNavigation&&armRequested_&&!safetyResetRequested_);
        if(const char *prefix=getenv("JR_R1_KEYPAD_IMAGES")){
            lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];
            snprintf(path,sizeof(path),"%s-%s-quick.bmp",prefix,portraitUi?"portrait":"landscape");simulatorSaveScreenshot(path);
        }
        press(PhysicalControlId::KeyMode);assert(activePage_==UiPage::MainMenu);
        press(PhysicalControlId::KeyPlus);assert(setupBank_==1);
        press(PhysicalControlId::KeyMode,true);assert(activePage_==UiPage::Dashboard&&armRequested_&&!safetyResetRequested_);
        press(PhysicalControlId::KeyMode);press(PhysicalControlId::KeyMinus);assert(activePage_==UiPage::TimerControl);
        const bool wasRunning=runTimerRunning_;controlKeypadMenu(9,activate);assert(runTimerRunning_!=wasRunning&&armRequested_);
        controlKeypadMenu(9,activate);assert(runTimerRunning_==wasRunning);
        press(PhysicalControlId::KeyMode,true);
        press(PhysicalControlId::KeyMode);press(PhysicalControlId::KeyUp);
        assert(activePage_==UiPage::ValueEditor&&editorField_==1&&armRequested_);
        const auto savedProfile=profileForChannel(selectedChannel_);const int draft=editorDraft_;
        press(PhysicalControlId::KeyPlus);assert(editorDraft_>draft);
        const int edited=editorDraft_;press(PhysicalControlId::KeyMode);
        assert(activePage_==UiPage::ValueEditor&&editorDraft_==edited&&pageFocusVisible_);
        press(PhysicalControlId::KeyClear,true);assert(activePage_==UiPage::FieldResetConfirmation);
        controlKeypadMenu(1,activate);assert(activePage_==UiPage::ValueEditor&&editorDraft_==0);
        assert(profileForChannel(selectedChannel_).positiveExpoPercent==savedProfile.positiveExpoPercent);
        press(PhysicalControlId::KeyClear);assert(activePage_==UiPage::Tune);
        assert(profileForChannel(selectedChannel_).positiveExpoPercent==savedProfile.positiveExpoPercent);
        tuneRole_=0;tuneBank_=0;drawDesignPage(UiPage::Tune);
        press(PhysicalControlId::KeyPage);assert(tuneBank_==1);
        press(PhysicalControlId::KeyPage,true);assert(tuneBank_==0);
        press(PhysicalControlId::KeyPlus);assert(tuneRole_==1);
        press(PhysicalControlId::KeyMinus);assert(tuneRole_==0);
        press(PhysicalControlId::KeyMode);assert(activePage_==UiPage::QuickNavigation);
        handleKeypadEvent(PhysicalControlId::KeyClear,InputEvent::Pressed,keyTime);
        handleKeypadEvent(PhysicalControlId::KeyClear,InputEvent::InvalidState,keyTime+10);
        handleKeypadEvent(PhysicalControlId::KeyClear,InputEvent::Released,keyTime+80);
        assert(activePage_==UiPage::QuickNavigation); // cancelled pointer / failed input
        press(PhysicalControlId::KeyClear);assert(activePage_==UiPage::Dashboard);
        bootPinUnlocked_=false;pinPurpose_=PinPurpose::Boot;drawBootPin();
        press(PhysicalControlId::KeyMode,true);assert(activePage_==UiPage::BootPin&&!armRequested());
        controlBootPin(1,activate);press(PhysicalControlId::KeyClear);assert(!bootPinDigits_[0]&&!bootPinUnlocked());
        bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;drawDashboard();
        Serial.println("Handset keypad: short/hold separation, quick menu, paging, tabs, cancelled inputs, draft reset/cancel and PIN/arm guards PASS");
    }
    // Shared navigation works from old detail pages, new detail pages and
    // the web touch path. Ordinary navigation never changes CUT/arm state.
    // Swipes page within a section without activating rows or changing output.
    tuneBank_=0;drawDesignPage(UiPage::Tune);
    armRequested_=true;safetyResetRequested_=false;
    assert(handlePageSwipe(250,150,100,154,400)&&tuneBank_==1);
    assert(armRequested_&&!safetyResetRequested_&&activePage_==UiPage::Tune);
    assert(handlePageSwipe(250,150,100,154,400)&&tuneBank_==1); // no wrap
    assert(handlePageSwipe(100,150,250,154,400)&&tuneBank_==0);
    assert(handlePageSwipe(250,150,100,230,400)&&tuneBank_==0); // diagonal consumed
    assert(handlePageSwipe(250,150,100,154,1400)&&tuneBank_==0); // long drag consumed
    assert(handlePageSwipe(250,50,100,54,400)&&tuneBank_==0); // subsection tabs excluded
    assert(!handlePageSwipe(100,150,104,152,100)&&tuneBank_==0); // tap untouched
    assert(touchAction(&TransmitterUi::controlPageDot,1)&&tuneBank_==1);
    assert(armRequested_&&!safetyResetRequested_);armRequested_=false;
    drawTelemetryPage();sensorBank_=0;
    assert(handlePageSwipe(250,150,100,154,400)&&sensorBank_==1);
    assert(handlePageSwipe(250,150,100,154,400)&&sensorBank_==2);
    assert(handlePageSwipe(250,150,100,154,400)&&sensorBank_==2);
    assert(touchAction(&TransmitterUi::controlPageDot,0)&&sensorBank_==0);
    controlDesign(62,activate);assert(handlePageSwipe(250,150,100,154,400)&&designBank_==1);
    drawMainMenu();setupPage_=0;assert(handlePageSwipe(250,150,100,154,400)&&setupPage_==1);
    inputMonitorBank_=0;drawInputMonitorPage();assert(handlePageSwipe(250,150,100,154,400)&&inputMonitorBank_==1);
    drawDashboard();const auto modeBeforeSwipe=driveMode();
    assert(!handlePageSwipe(250,150,100,154,400)&&driveMode()==modeBeforeSwipe&&!armRequested());
    beginPin(PinPurpose::WalkExit,UiPage::Dashboard);
    assert(!handlePageSwipe(250,150,100,154,400)&&pinPurpose_==PinPurpose::WalkExit);
    controlBootPin(12,activate);
    Serial.println("Page swipes: section paging, dots, bounds, drag rejection and output/PIN guards PASS");
    drawDisplayPage();assert(sectionNavigationVisible_);
    assert(touchAction(&TransmitterUi::controlSectionNavigation,2));assert(activePage_==UiPage::Telemetry);
    assert(touchAction(&TransmitterUi::controlSectionNavigation,0));assert(activePage_==UiPage::Dashboard&&!armRequested_);
    drawChannelsPage();assert(touchAction(&TransmitterUi::controlNavigation,0));assert(activePage_==UiPage::MainMenu);
    drawDisplayPage();assert(touchAction(&TransmitterUi::controlNavigation,0));assert(activePage_==UiPage::MainMenu);
    selectedChannel_=steeringChannel();tuneRole_=0;openDesignEditor(4,UiPage::Tune);
    assert(!sectionNavigationVisible_);const bool savedDirection=profileForChannel(selectedChannel_).reversed;
    editorDraft_=!savedDirection;navigateBack();assert(profileForChannel(selectedChannel_).reversed==savedDirection);
    assert(activePage_==UiPage::Tune);
    drawDesignPage(UiPage::WidgetEditor);assert(!sectionNavigationVisible_);navigateBack();assert(activePage_==UiPage::WidgetSelect);
    drawDesignPage(UiPage::BatteryMenu);navigateBack();assert(activePage_==UiPage::MainMenu);
    modelAppearanceEditing_=true;drawModelSelectPage();navigateBack();assert(activePage_==UiPage::ModelSelect&&!modelAppearanceEditing_);
    widgetInputsEditing_=true;drawDesignPage(UiPage::WidgetEditor);navigateBack();assert(activePage_==UiPage::WidgetEditor&&!widgetInputsEditing_);
    navigateBack();assert(activePage_==UiPage::WidgetSelect);
    drawDisplayPage();lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    if(const char *path=getenv("JR_R1_NAV_SCREENSHOT"))simulatorSaveScreenshot(path);
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
    // Drive mode tabs select directly, without the old chooser. Reselecting
    // the active mode preserves a live request; changing modes always CUTs.
    drawDashboard();armRequested_=true;safetyResetRequested_=false;
    assert(touchAction(&TransmitterUi::controlDesign,183));
    assert(activePage_==UiPage::Dashboard&&driveMode()==DriveMode::Normal&&armRequested_&&!safetyResetRequested_);
    assert(touchAction(&TransmitterUi::controlDesign,182));
    assert(activePage_==UiPage::Dashboard&&driveMode()==DriveMode::Walk&&!armRequested_&&safetyResetRequested_);
    armRequested_=true;safetyResetRequested_=false;
    assert(touchAction(&TransmitterUi::controlDesign,182));
    assert(armRequested_&&!safetyResetRequested_);armRequested_=false;
    if(const char *prefix=getenv("JR_R1_DRIVE_MODE_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-walk.bmp",prefix,designPortrait?"portrait":"landscape");
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
    // Walk exit expires after ten idle seconds, clears entered digits, and
    // preserves Walk/CUT. A keypad action restarts the idle interval.
    beginPin(PinPurpose::WalkExit,UiPage::Dashboard);
    controlBootPin(13,activate);updatePinTimeout(pinLastInputMs_+10000);
    assert(pinRecoveryHelp_&&pinPurpose_==PinPurpose::WalkExit&&driveMode()==DriveMode::Walk&&!armRequested());
    controlBootPin(14,activate);assert(!pinRecoveryHelp_);
    const uint32_t pinStarted=pinLastInputMs_;
    updatePinTimeout(pinStarted+9999);assert(pinPurpose_==PinPurpose::WalkExit);
    controlBootPin(1,activate);assert(strcmp(bootPinDigits_,"1")==0);
    const uint32_t pinTyped=pinLastInputMs_;
    updatePinTimeout(pinTyped+9999);assert(activePage_==UiPage::BootPin);
    updatePinTimeout(pinTyped+10000);
    assert(activePage_==UiPage::Dashboard&&pinPurpose_==PinPurpose::None);
    assert(driveMode()==DriveMode::Walk&&!armRequested()&&!bootPinDigits_[0]);
    loadPinSecurity();loadDriveModeSettings();assert(driveMode()==DriveMode::Walk);
    beginPin(PinPurpose::WalkExit,UiPage::MainMenu);pinLastInputMs_=UINT32_MAX-5000;
    updatePinTimeout(4999);assert(activePage_==UiPage::Dashboard&&driveMode()==DriveMode::Walk);
    beginPin(PinPurpose::Boot,UiPage::Dashboard);
    updatePinTimeout(pinLastInputMs_+10000);assert(pinPurpose_==PinPurpose::Boot);
    pinPurpose_=PinPurpose::None;
    // Leaving Walk requires a fresh PIN, survives restart/profile reload, and
    // cannot arm. Cancel and incorrect PIN preserve Walk.
    loadPinSecurity();loadDriveModeSettings();assert(driveMode()==DriveMode::Walk);
    drawDashboard();assert(touchAction(&TransmitterUi::controlDesign,183));
    assert(activePage_==UiPage::BootPin&&driveMode()==DriveMode::Walk&&!armRequested());
    if(const char *prefix=getenv("JR_R1_SECURITY_SCREENSHOTS")){
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        char path[256];snprintf(path,sizeof(path),"%s-walk.bmp",prefix);simulatorSaveScreenshot(path);
    }
    assert(touchAction(&TransmitterUi::controlBootPin,1));
    assert(strcmp(bootPinDigits_,"1")==0);controlBootPin(10,activate);
    for(int d:{9,9,9,9})controlBootPin(d,activate);
    assert(bootPinError_&&driveMode()==DriveMode::Walk&&!bootPinDigits_[0]);
    controlBootPin(12,activate);assert(driveMode()==DriveMode::Walk&&pinPurpose_==PinPurpose::None);
    const auto walkSelection=static_cast<uint16_t>(DriveModeSelection::WalkNormal);
    assert(!applyDriveModeAction(walkSelection,1));assert(pinPurpose_==PinPurpose::WalkExit);
    controlBootPin(12,activate);
    assert(!applyDriveModeAction(walkSelection,1));assert(activePage_==UiPage::Dashboard);
    assert(applyDriveModeAction(walkSelection,-1));
    assert(!applyDriveModeAction(walkSelection,1));
    for(int d:{1,2,3,4})controlBootPin(d,activate);
    assert(driveMode()==DriveMode::Normal&&!armRequested());
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
    assert(pinPurpose_==PinPurpose::ChangeNew);
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
    assert(driveMode()==DriveMode::Normal);

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
    const auto savedMix=channelModel_.mixing[14];channelModel_.mixing[14]=ChannelMixSettings{};
    bindings_[0]=ControlBinding{};bindings_[0].input=PhysicalControlId::IdleUpSwitch;
    bindings_[0].target=AssignmentTarget::CrsfChannel;bindings_[0].crsfChannel=15;
    bindings_[0].behaviour=ControlBehaviour::TwoPosition;bindingCount_=1;
    int binaryOff=0,binaryOn=0;assert(dashboardBinaryChannel(14,binaryOff,binaryOn));
    bindings_[0].behaviour=ControlBehaviour::Stepped;assert(!dashboardBinaryChannel(14,binaryOff,binaryOn));
    bindings_[0].behaviour=ControlBehaviour::TwoPosition;bindings_[0].input=PhysicalControlId::GripTrimA;
    assert(!dashboardBinaryChannel(14,binaryOff,binaryOn));
    bindings_[0].input=PhysicalControlId::IdleUpSwitch;channelModel_.mixing[14].rules[0].enabled=1;
    assert(!dashboardBinaryChannel(14,binaryOff,binaryOn));channelModel_.mixing[14]=savedMix;
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
    {
        const auto layout=dashboardLayouts_[activeModelSlot_];
        const auto hidden0=dashboardHidden_[activeModelSlot_][0],hidden2=dashboardHidden_[activeModelSlot_][2];
        dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
        auto &channels=dashboardLayouts_[activeModelSlot_].cells[0];
        channels.kind=DashboardWidgetKind::Channels;channels.style=DashboardWidgetStyle::Bars;
        channels.width=2;channels.source=0;channels.secondSource=1;
        auto &all=dashboardLayouts_[activeModelSlot_].cells[2];
        all.kind=DashboardWidgetKind::AllChannels;all.style=DashboardWidgetStyle::Bars;all.width=2;all.source=0;
        dashboardHidden_[activeModelSlot_][0]=dashboardHidden_[activeModelSlot_][2]=0;
        CrsfTelemetryState previewTelemetry;
        CrsfChannelOutputModel preview;
        preview.configure(1,crsfValueFromPermille(650),CRSF_CHANNEL_MID,true);
        preview.configure(2,crsfValueFromPermille(-400),CRSF_CHANNEL_MID,true);
        uint8_t before[CRSF_FRAME_BYTES],after[CRSF_FRAME_BYTES];buildCrsfChannelsFrame(preview,false,before);
        drawDashboard();nextDisplayUpdateMs_=0;nextSensorSampleMs_=0;
        update(triggerInput_.centre(),steeringInput_.centre(),-400,0,CRSF_CHANNEL_MID,
               OutputSafetyStatus::Cut,previewTelemetry,99,true,preview,false);
        lv_obj_update_layout(lv_scr_act());updateDashboardWidgets(previewTelemetry,99,true);
        const int steer=permilleFromCrsfValue(preview.requested(1)),throttle=permilleFromCrsfValue(preview.requested(2));
        assert(lv_bar_get_value(widgetViews_[0].bars[0])==steer);
        assert(lv_bar_get_value(widgetViews_[0].bars[1])==throttle);
        assert(lv_bar_get_value(widgetViews_[2].bars[0])==steer);
        assert(lv_bar_get_value(widgetViews_[2].bars[1])==throttle);
        assert(channelHistory_[0][29]==preview.requested(1)&&channelHistory_[1][29]==preview.requested(2));
        assert(dashboardChannelValues_[0]==CRSF_CHANNEL_MID&&dashboardChannelValues_[1]==CRSF_CHANNEL_MID&&!armRequested());
        buildCrsfChannelsFrame(preview,false,after);assert(memcmp(before,after,sizeof(before))==0);
        if(const char *path=getenv("JR_R1_CUT_PREVIEW_SCREENSHOT")){lv_refr_now(nullptr);simulatorSaveScreenshot(path);}
        designBank_=0;drawDesignPage(UiPage::ChannelMonitor);updateDesign(previewTelemetry,OutputSafetyStatus::Cut,99,true);
        char expected[40];snprintf(expected,sizeof(expected),"%+d / %+d%% ",steer/10,0);
        assert(strcmp(lv_label_get_text(designValues_[0]),expected)==0);
        dashboardLayouts_[activeModelSlot_]=layout;
        dashboardHidden_[activeModelSlot_][0]=hidden0;dashboardHidden_[activeModelSlot_][2]=hidden2;
        activePage_=UiPage::Channel2;drawPage();
        Serial.println("CUT dashboard: requested controls and histories stay live; Data sent values and CRSF safe frame unchanged PASS");
    }
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
    // Steering sides follow output reversal and keep live controls intact.
    tuneRole_=0;selectedChannel_=channelRoleSettings_.steeringChannel;
    const auto steeringOriginal=profileForChannel(selectedChannel_);
    auto &steering=profileForChannel(selectedChannel_);
    for(bool reversed : {false,true}) {
        steering=ChannelResponseProfile{};steering.reversed=reversed;
        setChannelEndpointForOutput(steering,false,81);
        setChannelEndpointForOutput(steering,true,93);
        latestSteeringRawAdc_=reversed?steeringInput_.calibration().maximum:steeringInput_.calibration().minimum;
        openDesignEditor(2,UiPage::Tune);
        assert(steeringEndpointSide_==0 && editorDraft_==81 && editorFine_);
        auto leftButton=steeringEndpointButtons_[0],rightButton=steeringEndpointButtons_[1],curve=designCurve_;
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        const auto renderedBefore=uiMetrics.rendered;
        // Exercise the actual touch dispatch, not just the draft setter.
        assert(dispatchPageControlTouch(20,designPortrait?312:214));
        assert(editorDraft_==80);
        assert(channelEndpointForOutput(steering,false)==81); // still a draft
        latestSteeringRawAdc_=reversed?steeringInput_.calibration().minimum:steeringInput_.calibration().maximum;
        updateSteeringEndpointSelection();assert(steeringEndpointSide_==1 && editorDraft_==93);
        adjustPageFocus(1);assert(editorDraft_==94 && steeringEndpointDrafts_[0]==80);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        assert(steeringEndpointButtons_[0]==leftButton && steeringEndpointButtons_[1]==rightButton && designCurve_==curve);
        assert(uiMetrics.rendered-renderedBefore<uint32_t(kScreenWidth*kScreenHeight));
        latestSteeringRawAdc_=steeringInput_.centre()+5;
        updateSteeringEndpointSelection();assert(steeringEndpointSide_==1);
        latestSteeringRawAdc_=reversed?steeringInput_.calibration().minimum:steeringInput_.calibration().maximum;
        updateSteeringEndpointSelection();controlDesign(150,activate);
        updateSteeringEndpointSelection();assert(steeringEndpointSide_==0 && editorDraft_==80);
        latestSteeringRawAdc_=steeringInput_.centre();updateSteeringEndpointSelection();
        latestSteeringRawAdc_=reversed?steeringInput_.calibration().minimum:steeringInput_.calibration().maximum;
        updateSteeringEndpointSelection();assert(steeringEndpointSide_==1 && editorDraft_==94);
        latestSteeringRawAdc_=65535;updateSteeringEndpointSelection();assert(steeringEndpointSide_==1);
        assert(dispatchPageControlTouch(kScreenWidth-30,designFooterY+10)); // Apply
        assert(activePage_==UiPage::Tune && !armRequested_);
        assert(channelEndpointForOutput(steering,false)==80 && channelEndpointForOutput(steering,true)==94);
        saveSettings();loadSettings();
        assert(channelEndpointForOutput(steering,false)==80 && channelEndpointForOutput(steering,true)==94);
        // UI Apply -> published configuration -> production mixed output -> CRSF.
        auto config=controlConfigurationSnapshot();const auto ch=selectedChannel_;
        config.channelParameters[ch-1]={};config.mixing[ch-1]={};
        MixInputSnapshot inputs;inputs.valid=3;inputs.values[1]=0;
        ChannelMixRuntime mix;ChannelSlewRuntime slew;CrsfChannelOutputModel outputs;
        inputs.values[0]=reversed?1000:-1000;
        processConfiguredMixedOutputs(config,outputs,inputs,mix,slew,100,false);
        assert(outputs.requested(ch)==crsfValueFromPermille(-800));
        inputs.values[0]=reversed?-1000:1000;
        processConfiguredMixedOutputs(config,outputs,inputs,mix,slew,104,false);
        assert(outputs.requested(ch)==crsfValueFromPermille(940));
        latestSteeringRawAdc_=steeringInput_.centre();
        openDesignEditor(2,UiPage::Tune);controlDesign(150,activate);controlDesign(33,activate);
        assert(steeringEndpointDrafts_[0]==100 && steeringEndpointDrafts_[1]==94);
        controlDesign(34,activate);assert(channelEndpointForOutput(steering,false)==80);
        openDesignEditor(2,UiPage::Tune);controlDesign(150,activate);
        for(int i=0;i<70;++i)adjustPageFocus(-1);
        assert(editorDraft_==50 && steeringEndpointDrafts_[1]==94);
        controlDesign(151,activate);for(int i=0;i<20;++i)adjustPageFocus(1);
        assert(editorDraft_==100 && steeringEndpointDrafts_[0]==50);
        controlDesign(34,activate);
        assert(channelEndpointForOutput(steering,false)==80 && channelEndpointForOutput(steering,true)==94);
    }
    steering=steeringOriginal;saveSettings();
    Serial.println("Steering endpoints: reversed sides, real touch Apply, CRSF output, persistence, cancel and partial redraw PASS");
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
        const auto saved=channelModel_.mixing[2];
        const auto selected=selectedChannel_;selectedChannel_=3;
        controlChannelConfig(5,activate);
        assert(activePage_==UiPage::ChannelMixing);
        controlChannelMixing(0,activate);controlChannelMixing(2,PageControlEvent::Decrease);
        assert(mixingDraft_.rules[0].enabled&&mixingDraft_.rules[0].weight==95);
        assert(memcmp(&saved,&channelModel_.mixing[2],sizeof(saved))==0);
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
        loadMixing();assert(channelModel_.mixing[2].rules[0].weight==-70);
        assert(channelModel_.mixing[2].constantPercent==60&&channelModel_.mixing[2].outputMinimum==20&&channelModel_.mixing[2].cutOverride&&channelModel_.mixing[2].cutPercent==60);
        const auto slot=activeModelSlot_;activeModelSlot_=4;loadMixing();
        assert(!channelModel_.mixing[2].rules[0].enabled);
        activeModelSlot_=slot;loadMixing();assert(channelModel_.mixing[2].rules[0].weight==-70);
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
        loadMixing();assert(channelModel_.mixing[2].rules[0].sourceMode==2);
        assert(channelModel_.mixing[2].rules[0].thresholdPercent==-5&&channelModel_.mixing[2].rules[0].hysteresisPercent==3);
        activeModelSlot_=4;loadMixing();assert(channelModel_.mixing[2].rules[0].sourceMode==0);
        activeModelSlot_=slot;loadMixing();assert(channelModel_.mixing[2].rules[0].sourceMode==2);
        assert(!saveMixing(throttleChannel(),channelModel_.mixing[2]));
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
    // Real CRSF cell frame -> parser -> sensor cards, detail and persisted widget IDs.
    {
        const auto savedTelemetry=designTelemetry_;
        const auto savedLayout=dashboardLayouts_[activeModelSlot_];
        const uint8_t savedBank=sensorBank_;
        CrsfTelemetryParser cellsParser;
        uint8_t frame[]={0xc8,11,0x0e,0,0x0f,0x94,0x0f,0xae,0x0f,0xa6,0x0f,0xc0,0};
        frame[12]=crc8DvbS2(frame+2,10);
        for(auto b:frame)cellsParser.feed(b);
        auto cells=cellsParser.state();
        assert(cells.vehicleCellsValid && cells.vehiclePackMv==16040);
        cells.receiverVoltageValid=true;cells.receiverVoltageDecivolts=66;
        designTelemetry_=cells;sensorBank_=0;drawTelemetryPage();controlDesign(81,activate);
        assert(sensorBank_==1 && activePage_==UiPage::Telemetry);
        updateDesign(cells,OutputSafetyStatus::Cut,99,true);
        assert(strcmp(lv_label_get_text(designValues_[0]),"16.040")==0);
        assert(strcmp(lv_label_get_text(designValues_[1]),"3.988")==0);
        assert(strcmp(lv_label_get_text(designValues_[2]),"3.988")==0);
        assert(strcmp(lv_label_get_text(designValues_[5]),"4.032")==0);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        for(unsigned i=0;i<6;++i){
            auto panel=lv_obj_get_parent(designValues_[i]);
            assert(lv_obj_get_y(designDetails_[i])+lv_obj_get_height(designDetails_[i])<=lv_obj_get_height(panel));
        }
        if(const char *prefix=getenv("JR_R1_CELL_SCREENSHOTS")){
            char path[256];snprintf(path,sizeof(path),"%s-%s.bmp",prefix,designPortrait?"portrait":"landscape");simulatorSaveScreenshot(path);
        }
        cells.vehicleCellsValid=false;updateDesign(cells,OutputSafetyStatus::Cut,99,true);
        assert(strcmp(lv_label_get_text(designValues_[0]),"--")==0);
        assert(strcmp(lv_label_get_text(designValues_[2]),"--")==0);
        controlDesign(72,activate);assert(sensorSelected_==8 && activePage_==UiPage::SensorDetail);
        cells.vehicleCellsValid=true;updateDesign(cells,OutputSafetyStatus::Cut,99,true);
        assert(strcmp(lv_label_get_text(designValues_[0]),"3.988")==0);
        for(uint8_t slot=0;slot<4;++slot){
            DashboardWidget widget;widget.kind=DashboardWidgetKind::Sensor;widget.source=slot+6;
            placeDashboardWidget(dashboardLayouts_[activeModelSlot_],slot,widget);
        }
        drawDashboard();updateDashboardWidgets(cells,99,true);
        assert(strcmp(lv_label_get_text(widgetViews_[0].values[0]),"16.040")==0);
        assert(dashboardLayouts_[activeModelSlot_].cells[2].kind==DashboardWidgetKind::BatteryCells);
        assert(strcmp(lv_label_get_text(widgetViews_[2].values[0]),"1: 3.988")==0);
        assert(strstr(lv_label_get_text(widgetViews_[2].detail),"4S"));
        widgetCell_=0;widgetDraft_=dashboardLayouts_[activeModelSlot_].cells[0];
        widgetDraft_.source=13;controlDesign(123,activate);
        assert(widgetDraft_.source==0);controlDesign(123,PageControlEvent::Decrease);
        assert(widgetDraft_.source==13);
        // Group widget: every span, automatic 6S, explicit mismatch, stale values and persisted count.
        for(uint8_t width=1;width<=2;++width)for(uint8_t height=1;height<=2;++height){
            DashboardWidget group;group.kind=DashboardWidgetKind::BatteryCells;group.width=width;group.height=height;
            placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,group);drawDashboard();
            cells.vehicleCellCount=6;cells.vehicleCellsMv[4]=4100;cells.vehicleCellsMv[5]=4200;
            cells.vehiclePackMv=24340;updateDashboardWidgets(cells,99,true);
            assert(strcmp(lv_label_get_text(widgetViews_[0].values[5]),"6: 4.200")==0);
            assert(lv_obj_has_flag(widgetViews_[0].values[6],LV_OBJ_FLAG_HIDDEN));
            assert(strstr(lv_label_get_text(widgetViews_[0].detail),"6S"));
            if(width==1 && height==1)if(const char *prefix=getenv("JR_R1_GROUP_SCREENSHOTS")){char path[256];snprintf(path,sizeof(path),"%s-%s.bmp",prefix,designPortrait?"portrait":"landscape");lv_refr_now(nullptr);simulatorSaveScreenshot(path);}
            widgetCell_=0;widgetDraft_=group;widgetDraft_.source=12;controlDesign(126,activate);
            updateDashboardWidgets(cells,99,true);
            assert(strstr(lv_label_get_text(widgetViews_[0].detail),"Expected 12S"));
            assert(strcmp(lv_label_get_text(widgetViews_[0].values[11]),"12: --")==0);
            lv_obj_update_layout(lv_scr_act());
            uint16_t seenCells=0;
            const auto checkVisibleCells=[&](){
                unsigned visible=0;
                for(unsigned i=0;i<12;++i){
                    auto label=widgetViews_[0].values[i];
                    if(lv_obj_has_flag(label,LV_OBJ_FLAG_HIDDEN))continue;
                    seenCells|=uint16_t{1}<<i;++visible;
                    assert(lv_obj_get_y(label)+lv_obj_get_height(label)<=lv_obj_get_y(widgetViews_[0].detail));
                    assert(lv_obj_get_height(label)>=lv_obj_get_style_text_font(label,LV_PART_MAIN)->line_height);
                }
                assert(visible>0);
            };
            checkVisibleCells();
            designTelemetry_=cells;
            for(unsigned page=0;page<12 && seenCells!=0x0fff;++page){
                controlDesign(130,activate);updateDashboardWidgets(cells,99,true);
                lv_obj_update_layout(lv_scr_act());checkVisibleCells();
            }
            assert(seenCells==0x0fff); // All 12 cells remain readable/reachable in every span.
            cells.vehicleCellsValid=false;updateDashboardWidgets(cells,99,true);
            assert(strcmp(lv_label_get_text(widgetViews_[0].values[0]),"1: --")==0);
            cells.vehicleCellsValid=true;
        }
        dashboardLayouts_[activeModelSlot_]=savedLayout;designTelemetry_=savedTelemetry;sensorBank_=savedBank;
    }
    Serial.println("Vehicle cells: CRSF parser, 4S cards, stale values, sensor detail and dashboard widgets PASS");
    // FAS third page and widgets accept live test readings without touching RX supply.
    {
        const auto savedTelemetry=designTelemetry_;const auto savedLayout=dashboardLayouts_[activeModelSlot_];
        const auto savedBank=sensorBank_;
        CrsfTelemetryState fas{};fas.fasVoltageValid=fas.fasCurrentValid=true;
        fas.fasVoltageMv=16040;fas.fasCurrentDeciA=11559;
        fas.fasVoltageUpdatedMs=fas.fasCurrentUpdatedMs=millis();
        fas.receiverVoltageValid=true;fas.receiverVoltageDecivolts=66;
        designTelemetry_=fas;sensorBank_=2;drawTelemetryPage();updateDesign(fas,OutputSafetyStatus::Cut,99,true);
        assert(strcmp(lv_label_get_text(designValues_[0]),"16.040")==0);
        assert(strcmp(lv_label_get_text(designValues_[1]),"1155.9")==0);
        assert(strcmp(lv_label_get_text(designDetails_[1]),"Over 300A")==0);
        assert(!designValues_[2]);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        if(const char *prefix=getenv("JR_R1_FAS_SCREENSHOTS")){
            char path[256];snprintf(path,sizeof(path),"%s-%s.bmp",prefix,designPortrait?"portrait":"landscape");simulatorSaveScreenshot(path);
        }
        DashboardWidget widget;widget.kind=DashboardWidgetKind::Sensor;widget.source=13;
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,widget);drawDashboard();updateDashboardWidgets(fas,99,true);
        assert(strcmp(lv_label_get_text(widgetViews_[0].values[0]),"1155.9")==0);
        fas.fasCurrentValid=fas.fasVoltageValid=false;updateDashboardWidgets(fas,99,true);
        assert(strcmp(lv_label_get_text(widgetViews_[0].values[0]),"--")==0);
        dashboardLayouts_[activeModelSlot_]=savedLayout;designTelemetry_=savedTelemetry;sensorBank_=savedBank;
    }
    Serial.println("FAS telemetry: separate voltage/current cards, range marker, widgets and stale values PASS");
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
        dispatchPageControlTouch(40,22);assert(activePage_==UiPage::MainMenu);
        drawChannelRolesPage();auto oldRole=channelRoleSettings_.steeringChannel;
        const auto detailY=[&](int y){return 44+(y-44)*(kScreenHeight-60-4-44)/(kScreenHeight-20-44);};
        dispatchPageControlTouch(282,detailY(94));assert(channelRoleSettings_.steeringChannel!=oldRole);
        dispatchPageControlTouch(231,detailY(94));assert(channelRoleSettings_.steeringChannel==oldRole);
        drawThrottleResponsePage();dispatchPageControlTouch(282,detailY(434));assert(portraitResponseBank_==1);
        dispatchPageControlTouch(33,detailY(434));assert(portraitResponseBank_==0);
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
    const UiPage pages[]={UiPage::QuickNavigation,UiPage::FieldResetConfirmation,UiPage::TimerControl,UiPage::Dashboard,UiPage::Tune,UiPage::Telemetry,UiPage::Gps,UiPage::ChannelMonitor,UiPage::MainMenu,UiPage::ModelSelect,UiPage::Channels,UiPage::ChannelRoles,UiPage::Channel2,UiPage::ThrottleResponse,UiPage::ChannelSpeed,UiPage::ControlAssignments,UiPage::BindingEditor,UiPage::InputMonitor,UiPage::DriveModes,UiPage::ChannelPlaceholder,UiPage::Setup,UiPage::TxBatterySetup,UiPage::RxBatterySetup,UiPage::DisplaySetup,UiPage::ColourScheme,UiPage::FirmwareInfo,UiPage::ElrsBinding,UiPage::ElrsTxConfig,UiPage::EscProgramming,UiPage::WidgetSelect,UiPage::WidgetEditor,UiPage::RadioDiagnostics,UiPage::BatteryMenu,UiPage::Calibration,UiPage::OutputStatus,UiPage::ModeChooser,UiPage::ChannelInput,UiPage::ChannelResponseSetup,UiPage::ParameterControlPicker,UiPage::ResponseProfile,UiPage::MixingOverview,UiPage::NetworkOptions,UiPage::SystemMenu};
    for(auto page:pages){
        showSimulatorPage(page);lv_obj_update_layout(lv_scr_act());
        assert(activePage_==page && pageFocusCount_>0);
        if(page==UiPage::Tune || page==UiPage::Telemetry || page==UiPage::MainMenu){
            if(const char *prefix=getenv("JR_R1_TAB_IMAGES")){
                char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,designPortrait?"portrait":"landscape",uiPageName(page));
                lv_refr_now(nullptr);simulatorSaveScreenshot(path);
            }
        }
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
    assert(runSimulatorPerformanceAudit());
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
    navigateBack();assert(activePage_==UiPage::NetworkOptions && SetupPortal::active() && !armRequested_);
    drawWifiPage();
    armRequested_=false;
    assert(!SetupPortal::connect("Home","short"));
    controlWifi(3,activate);assert(wifiEdit_==3);
    controlWifi(70,activate);assert(wifiEdit_==2 && strcmp(wifiSsid_,"Home Wi-Fi")==0);
    controlWifi(20,activate);assert(strcmp(wifiPassword_,"1")==0);
    controlWifi(12,activate);assert(wifiPassword_[0]==0);
    controlWifi(10,activate);controlWifi(10,activate);controlWifi(20,activate);
    assert(strcmp(wifiPassword_,"!")==0);
    controlWifi(13,activate);
    const auto savedProfile=channelModel_.responses[0];const auto savedParameters=channelModel_.parameters[0];
    auto edited=savedProfile;edited.negativeEndpointPercent=73;edited.positiveEndpointPercent=92;
    assert(portalSetChannel(1,edited,10,-20));assert(channelModel_.responses[0].negativeEndpointPercent==73 && channelModel_.responses[0].positiveEndpointPercent==92);
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
    assert(portalSetChannel(1,channelModel_.responses[0],channelModel_.parameters[0].trimPermille,channelModel_.parameters[0].subtrimPermille));
    assert(!bootPinUnlocked()&&!controlConfigurationSnapshot().armRequested);
    assert(portalRenameModel("BENCH TEST"));assert(strcmp(portalModelName(),"BENCH TEST")==0);
    assert(!portalRenameModel("TOO LONG MODEL NAME"));assert(portalRenameModel(previousName));
    assert(!bootPinUnlocked());
    drawBootPin();
    const auto remoteKey=[&](int index){
        const uint8_t action=index==9?10:index==10?0:index==11?11:index+1;
        for(uint8_t i=0;i<pageFocusCount_;++i){const auto target=pageFocusTargets_[i];
            if(target.handler==&TransmitterUi::controlBootPin&&target.argument==action)
                return portalRemoteInput(target.activate.x+target.activate.width/2,
                                         target.activate.y+target.activate.height/2,0);
        }
        return false;
    };
    assert(!portalRemoteInput(0,0,1)&&!portalRemoteInput(0,0,2));
    for(int i=0;i<4;++i)assert(remoteKey(0));
    assert(!bootPinUnlocked()&&bootPinError_);
    assert(remoteKey(0));assert(remoteKey(9));assert(!bootPinDigits_[0]);
    for(int i=0;i<3;++i)assert(remoteKey(i));
    assert(!bootPinUnlocked());assert(remoteKey(3));
    assert(bootPinUnlocked()&&!armRequested());
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
    assert(portalRemoteInput(20,20,0));assert(activePage_==UiPage::NetworkOptions&&SetupPortal::active());
    assert(portalRemoteInput(30,kScreenHeight-(portraitUi?40:20),0));
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
    assert(portalRemoteInput(30,kScreenHeight-(portraitUi?40:20),0));
    assert(SetupPortal::active()&&activePage_==UiPage::Dashboard&&!armRequested_);
    assert(portalRemoteInput(0,0,2));
    const auto remoteWifiAction=[&](uint8_t action){
        for(uint8_t i=0;i<pageFocusCount_;++i){const auto t=pageFocusTargets_[i];
            if(t.handler==&TransmitterUi::controlWifi && t.argument==action)
                return portalRemoteInput(t.activate.x+t.activate.width/2,t.activate.y+t.activate.height/2,0);}
        assert(false && "Wi-Fi action must be visible");return false;
    };
    assert(!remoteWifiAction(6)); // Connect remains blocked remotely.
    assert(!remoteWifiAction(7)); // Forget remains blocked remotely.
    assert(remoteWifiAction(80)); // Credentials opens without reconnecting.
    assert(wifiWebAccess_);
    assert(!remoteWifiAction(15)); // Change password remains blocked remotely.
    assert(SetupPortal::active()&&SetupPortal::hasSavedNetwork("Home Wi-Fi"));
    assert(remoteWifiAction(19)); // Hostname editor
    assert(wifiEdit_==6&&SetupPortal::active());
    strcpy(wifiHostname_,"bad.local");controlWifi(13,activate);assert(wifiEdit_==6);
    strcpy(wifiHostname_,"My-Handset");controlWifi(13,activate);assert(wifiEdit_==0);
    assert(strcmp(SetupPortal::state().hostname,"my-handset")==0);
    assert(strcmp(SetupPortal::state().activeHostname,"r1evo")==0); // No connection change before restart
    SetupPortal::begin(this);assert(strcmp(SetupPortal::state().activeHostname,"my-handset")==0);
    assert(SetupPortal::setHostname("r1evo"));SetupPortal::begin(this);
    assert(remoteWifiAction(2)); // Remote Finish setup leaves its transport connected
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
        if(!armed)if(const char *prefix=getenv("JR_R1_SAVER_IMAGES")){
            for(uint8_t frame=0;frame<2;++frame){
                logoSaverPosition_=frame;positionLogoSaver();lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
                char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",frame?"r1-evo":"jr-propo");
                simulatorSaveScreenshot(path);
            }
        }
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

    auditSportGps();

    // Hardware recovery keeps authenticated remote navigation available without
    // claiming startup preflight passed or permitting any arm request.
    enterHardwareRecovery();
    bootPinUnlocked_=false;startupPreflightComplete_=false;
    bootPinDigits_[0]=0;
    drawBootPin();SetupPortal::tick(true,true);
    assert(portalEnableScreen());
    assert(!portalRemoteInput(0,0,2));
    for(int d:{1,2,3,4})controlBootPin(d,activate);
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
    // Explicit headless mode also covers a physically blank write-only LCD
    // whose SPI driver reports success. Persist only the bench flag, keep the
    // UI/mirror alive, and require a fresh boot to clear the recovery latch.
    assert(setHeadlessTesting(true));
    assert(headlessTesting() && hardwareRecovery() && !lcdTransportReady);
    {
        Preferences bench;
        assert(bench.begin("r1-bench",true));
        assert(bench.getBool("headless",false));bench.end();
    }
    armRequested_=true;
    assert(!armRequested() && !controlConfigurationSnapshot().armRequested);
    const auto sentBefore=uiMetrics.sent, renderedBefore=uiMetrics.rendered;
    lv_obj_invalidate(lv_scr_act());lv_refr_now(nullptr);
    assert(uiMetrics.rendered>renderedBefore && uiMetrics.sent==sentBefore);
    assert(setHeadlessTesting(false));
    {
        Preferences bench;
        assert(bench.begin("r1-bench",true));
        assert(!bench.getBool("headless",true));bench.end();
    }
    assert(headlessTesting() && hardwareRecovery() && !lcdTransportReady);
    assert(!armRequested() && !controlConfigurationSnapshot().armRequested);
    lcdTransportReady=transport; // Restore the simulator sink for later visual audits.
    Serial.println("Hardware recovery: PIN, remote navigation, offscreen rendering and arm inhibition PASS");
    Serial.println("LVGL UI audit: navigation, drafts, widgets, model guards, timer, endpoints, colour schemes, startup fault display, arm hold and bounds PASS");
    dashboardEnabledChannelMask_=0xffff;
    for(int ch=0;ch<16;++ch)designRequested_[ch]=dashboardChannelValues_[ch]=crsfValueFromPermille(ch*100-750);
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
            if(page==UiPage::Telemetry)sensorBank_=cycle%3; // cover all sensor banks in both orientations
            showSimulatorPage(page);lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);auditTypography(lv_scr_act());
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
    Serial.println("Typography: all pages render with the shared six-style scale PASS");
    drawDashboard();return true;
}

bool TransmitterUi::runSimulatorPerformanceAudit()
{
    bootPinUnlocked_ = true; pinPurpose_ = PinPurpose::None;
    armRequested_ = false; startupPreflightComplete_ = true;
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
    const auto savedChannelValue = designRequested_[0];
    designRequested_[0] = kCrsfMaximum;
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
    assert(lv_obj_get_style_text_font(designStatus_, LV_PART_MAIN) == UiText::font(UiText::Menu));
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
    designRequested_[0] = savedChannelValue;
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
    // Accepted navigation action through the same page controller, then paint.
    // Native SDL timing excludes hardware SPI and physical release debounce.
    const uint8_t actions[] = {1,2,3,0};
    const char *names[] = {"Tune","Data","Setup","Drive"};
    uint32_t samples[4][7];
    for (unsigned round = 0; round < 7; ++round) {
        for (unsigned page = 0; page < 4; ++page) {
            const uint32_t began = micros();
            controlDesign(actions[page], PageControlEvent::Activate);
            lv_obj_update_layout(lv_scr_act()); lv_refr_now(nullptr);
            samples[page][round] = uint32_t(micros()) - began;
        }
    }
    for (unsigned page = 0; page < 4; ++page) {
        for (unsigned i = 1; i < 7; ++i) for (unsigned j = i; j && samples[page][j] < samples[page][j-1]; --j) {
            const auto previous = samples[page][j-1]; samples[page][j-1] = samples[page][j]; samples[page][j] = previous;
        }
        Serial.printf("UI action-to-paint benchmark: %s median=%luus\n",names[page],(unsigned long)samples[page][3]);
    }
    const auto savedModel = channelModel_;
    const auto savedHandler = safetyCutHandler_;
    safetyCutHandler_ = auditConfigurationCut;
    selectedChannel_ = throttleChannel(); tuneRole_ = 1;
    for (uint8_t field : {0,1,2,3,4,7}) {
        editorField_ = field; armRequested_ = true; safetyResetRequested_ = false;
        const auto count = auditCutRequests;
        applyDesignValue(designValue());
        assert(!armRequested_ && safetyResetRequested_ && auditCutRequests == count+1);
    }
    armRequested_ = true; safetyResetRequested_ = false;
    const auto oldTrim = channelModel_.parameters[selectedChannel_-1].trimPermille;
    assert(applyChannelParameterAction(encodeChannelParameterTarget(selectedChannel_,ChannelParameterKind::Trim),oldTrim == 10 ? 20 : 10));
    assert(armRequested_ && !safetyResetRequested_); // deliberate live trim remains live
    const auto oldSubtrim = channelModel_.parameters[selectedChannel_-1].subtrimPermille;
    assert(applyChannelParameterAction(encodeChannelParameterTarget(selectedChannel_,ChannelParameterKind::Subtrim),oldSubtrim == 10 ? 20 : 10));
    assert(!armRequested_ && safetyResetRequested_);
    channelModel_ = savedModel;
    assert(persistChannelModel());
    auto profile = channelModel_.responses[4]; profile.positiveRatePercent = profile.positiveRatePercent == 90 ? 100 : 90;
    channelModel_.setResponse(5,profile);
    settingsSavePending_ = true; settingsSaveDueMs_ = 0;
    armRequested_ = true;
    CrsfChannelOutputModel outputs; CrsfTelemetryState telemetry;
    update(THROTTLE_ADC_CENTER,STEERING_ADC_CENTER,0,0,CRSF_CHANNEL_MID,
           OutputSafetyStatus::Armed,telemetry,99,true,outputs,true);
    assert(settingsSavePending_); // due settings never write with live outputs
    ChannelModelController stored;
    assert(ChannelSettingsRepository::load(activeModelSlot_,stored) == ChannelSettingsLoad::Loaded);
    assert(stored.responses[4].positiveRatePercent == savedModel.responses[4].positiveRatePercent);
    requestConfigurationCut();
    lv_tick_inc(40);
    update(THROTTLE_ADC_CENTER,STEERING_ADC_CENTER,0,0,CRSF_CHANNEL_MID,
           OutputSafetyStatus::Cut,telemetry,99,true,outputs,false);
    assert(!settingsSavePending_ && !channelParameterSavePending_);
    assert(ChannelSettingsRepository::load(activeModelSlot_,stored) == ChannelSettingsLoad::Loaded);
    assert(stored.responses[4].positiveRatePercent == profile.positiveRatePercent);
    channelModel_ = savedModel; assert(persistChannelModel()); safetyCutHandler_ = savedHandler;
    armRequested_ = false; safetyResetRequested_ = false;
    Serial.println("UI edit safety: immediate CUT, live trim and deferred/coalesced persistence PASS");
    Serial.println("UI responsiveness: unchanged draws, changed values and cached navigation PASS");
    return true;
}
