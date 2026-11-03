// Exercise the native callbacks, real telemetry formatting and V2 persistence.
#include <cassert>
#include <cstdlib>
bool TransmitterUi::runSimulatorV2Audit()
{
    const auto activate=PageControlEvent::Activate;
    bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
    armRequested_=true;safetyResetRequested_=false;v2Overview_=true;
    const auto layout=dashboardLayouts_[activeModelSlot_];
    auto shot=[&](const char *name){if(const char *prefix=getenv("JR_R1_V2_IMAGES")){
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];snprintf(path,sizeof(path),"%s-%s.bmp",prefix,name);simulatorSaveScreenshot(path);}};
    auto tap=[&](uint8_t id){for(uint8_t i=0;i<pageFocusCount_;++i){const auto t=pageFocusTargets_[i];if(t.handler==&TransmitterUi::controlV2&&t.argument==id){const auto r=t.activate;assert(dispatchPageControlTouch(r.x+r.width/2,r.y+r.height/2));return;}}assert(false);};
    CrsfTelemetryState t{};const uint32_t now=millis();
    t.gpsSpeedValid=true;t.gpsSpeedUpdatedMs=now;t.groundSpeedDeciKmh=640;
    t.gpsSatellitesValid=true;t.gpsSatellitesUpdatedMs=now;t.satellites=12;
    t.rpmValid=true;t.rpmUpdatedMs=now;t.rpm=32000;
    t.vehicleCellsValid=true;t.vehicleCellsUpdatedMs=now;t.vehicleCellCount=4;t.vehiclePackMv=15800;t.vehicleMinimumCellMv=3900;
    t.linkValid=true;t.linkUpdatedMs=now;t.uplinkLinkQuality=99;t.uplinkRssiDbm=-70;t.rfMode=27;t.txPowerCode=7;
    designTelemetry_=t;drawDashboard();updateDesign(t,OutputSafetyStatus::Cut,99,true);lv_obj_update_layout(lv_scr_act());
    if(!portraitUi){
        assert(v2Values_[0]&&strcmp(lv_label_get_text(v2Values_[0]),"64.0")==0);
        assert(strcmp(lv_label_get_text(v2Values_[2]),"32000")==0);
        assert(strstr(lv_label_get_text(v2Details_[3]),"250 mW RF"));
        for(uint8_t i=0;i<pageFocusCount_;++i){const auto &target=pageFocusTargets_[i];assert(!(target.handler==&TransmitterUi::controlDesign&&target.argument==4));
            assert(target.activate.y+target.activate.height<=kScreenHeight);}
        shot("drive");tap(1);assert(!v2Overview_);assert(widgetViews_[0].panel);controlV2(1,activate);assert(v2Overview_);
        tap(10);assert(activePage_==UiPage::SensorDetail&&sensorSelected_==0);
        controlV2(30,activate);assert(activePage_==UiPage::WidgetSelect);
        sensorSelected_=6;drawDesignPage(UiPage::SensorDetail);tap(30);assert(activePage_==UiPage::DriveBatterySetup);
        sensorSelected_=2;nextSensorSampleMs_=millis()+100000;
        for(unsigned i=0;i<30;++i){sensorHistory_[2][i]=7000+i*200;sensorHistoryValid_[2][i]=true;}
        drawDesignPage(UiPage::SensorDetail);lv_obj_update_layout(lv_scr_act());tap(20);assert(v2GraphPaused_);
        const auto frozen=v2FrozenHistory_[29];sensorHistory_[2][29]=50000;
        updateDesign(t,OutputSafetyStatus::Cut,99,true);assert(v2FrozenHistory_[29]==frozen);
        tap(21);assert(v2GraphCursor_==28);assert(strstr(lv_label_get_text(designDetails_[6]),"-1s:"));shot("graph");
        tap(20);assert(!v2GraphPaused_);assert(armRequested_&&!safetyResetRequested_);
    }
    controlDesign(61,activate);assert(activePage_==UiPage::Gps&&designMarker_&&designCurve_);shot("gps");
    loadV2Quick();uint8_t slots[8];memcpy(slots,v2QuickSlots_,8);
    keypadReturnPage_=UiPage::Dashboard;drawDesignPage(UiPage::QuickNavigation);shot("quick");
    v2QuickEditing_=true;controlKeypadMenu(0,activate);const auto chosen=v2QuickSlots_[0];assert(chosen!=slots[0]);
    v2QuickModel_=255;loadV2Quick();assert(v2QuickSlots_[0]==chosen);
    const uint8_t slot=activeModelSlot_;activeModelSlot_=(slot+1)%kModelMemoryCount;loadV2Quick();assert(v2QuickSlots_[0]==1);
    activeModelSlot_=slot;v2QuickModel_=255;loadV2Quick();assert(v2QuickSlots_[0]==chosen);
    memcpy(v2QuickSlots_,slots,8);saveV2Quick();v2QuickEditing_=false;
    drawDesignPage(UiPage::QuickNavigation);controlKeypadMenu(5,activate);assert(activePage_==UiPage::Gps);
    selectedChannel_=steeringChannel();openV2Shortcut(7);assert(activePage_==UiPage::MixingOverview);
    controlV2(2,activate);assert(activePage_==UiPage::RunReview);shot("review");
    controlV2(50,activate);assert(v2ReviewConfirm_);controlV2(51,activate);assert(!v2ReviewConfirm_);
    assert(armRequested_&&!safetyResetRequested_);
    assert(memcmp(&layout,&dashboardLayouts_[activeModelSlot_],sizeof(layout))==0);
    armRequested_=false;designTelemetry_=CrsfTelemetryState{};v2GraphPaused_=false;v2Overview_=false;
    nextSensorSampleMs_=0;memset(sensorHistoryValid_,0,sizeof(sensorHistoryValid_));
    drawDashboard();
    Serial.println("V2 native: real overview, untouched V1 widgets, GPS route, sensor shortcuts, graph freeze/cursor, per-model Quick persistence, run review and arm/CUT invariants PASS");
    return true;
}
