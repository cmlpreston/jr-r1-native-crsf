// Integration checks exercise the actual UI callbacks and persistence. Run
// only with JR_R1_SIM_STATE pointed at a disposable directory.
#include <cassert>
#include <cstdlib>
bool TransmitterUi::runSimulatorUiAudit()
{
    if(!getenv("JR_R1_SIM_STATE")){Serial.println("UI audit requires disposable JR_R1_SIM_STATE");return false;}
    const auto activate=PageControlEvent::Activate;
    latestRawAdc_=triggerInput_.centre();
    armRequested_=false;
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
    // All ten Setup routes must remain reachable, including non-demo features.
    const UiPage routes[]={UiPage::ModelSelect,UiPage::Channels,UiPage::ControlAssignments,UiPage::DriveModes,UiPage::ElrsTxConfig,UiPage::ElrsBinding,UiPage::EscProgramming,UiPage::Setup,UiPage::InputMonitor,UiPage::WidgetSelect};
    for(uint8_t i=0;i<10;++i){drawMainMenu();controlDesign(40+i,activate);assert(activePage_==routes[i]);}
    // Widget size, overlap and persistence are isolated from control settings.
    drawDesignPage(UiPage::WidgetSelect);controlDesign(110,activate);
    widgetDraft_.kind=DashboardWidgetKind::Map;widgetDraft_.width=widgetDraft_.height=2;
    auto before=controlConfigurationSnapshot();
    controlDesign(126,activate);
    assert(dashboardWidgetMask(0,dashboardLayouts_[activeModelSlot_].cells[0])==15);
    auto after=controlConfigurationSnapshot();
    assert(before.throttleChannel==after.throttleChannel && before.armRequested==after.armRequested);
    loadModelMemorySettings();assert(dashboardLayouts_[activeModelSlot_].cells[0].kind==DashboardWidgetKind::Map);
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
    for(uint8_t width=1;width<=2;++width){for(uint8_t height=1;height<=width;++height){
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
    controlDesign(5,activate);assert(!armRequested_);
    designTouchDispatch_=true;lastTouchDurationMs_=100;controlDesign(5,activate);assert(!armRequested_);
    lastTouchDurationMs_=900;controlDesign(5,activate);assert(armRequested_);
    lastTouchDurationMs_=20;controlDesign(5,activate);assert(!armRequested_);designTouchDispatch_=false;
    if(designPortrait){
        drawMainMenu();dispatchPageControlTouch(250,60);assert(setupBank_==1);
        dispatchPageControlTouch(80,110);assert(activePage_==UiPage::DisplaySetup);
        dispatchPageControlTouch(239,22);assert(activePage_==UiPage::Setup);
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
    const UiPage pages[]={UiPage::Dashboard,UiPage::Tune,UiPage::Telemetry,UiPage::Gps,UiPage::ChannelMonitor,UiPage::MainMenu,UiPage::ModelSelect,UiPage::Channels,UiPage::ChannelRoles,UiPage::Channel2,UiPage::ThrottleResponse,UiPage::ChannelSpeed,UiPage::ControlAssignments,UiPage::BindingEditor,UiPage::InputMonitor,UiPage::DriveModes,UiPage::ChannelPlaceholder,UiPage::Setup,UiPage::TxBatterySetup,UiPage::RxBatterySetup,UiPage::DisplaySetup,UiPage::ColourScheme,UiPage::FirmwareInfo,UiPage::ElrsBinding,UiPage::ElrsTxConfig,UiPage::EscProgramming,UiPage::WidgetSelect,UiPage::WidgetEditor,UiPage::RadioDiagnostics,UiPage::BatteryMenu,UiPage::Calibration};
    for(auto page:pages){
        showSimulatorPage(page);lv_obj_update_layout(lv_scr_act());
        assert(activePage_==page && pageFocusCount_>0);
        for(uint8_t i=0;i<pageFocusCount_;++i){const auto &r=pageFocusTargets_[i].focus;
            if(r.x<0 || r.y<0 || r.x+r.width>kScreenWidth || r.y+r.height>kScreenHeight){Serial.printf("UI focus outside LCD: %s target %u\n",uiPageName(page),i);return false;}
        }
        lv_mem_monitor_t memory;lv_mem_monitor(&memory);assert(memory.free_size>8192);
    }
    Serial.println("LVGL UI audit: navigation, drafts, widgets, model guards, timer, endpoints, colour schemes, arm hold and bounds PASS");
    drawDashboard();return true;
}
