// Exercise the native callbacks, real telemetry formatting and V2 persistence.
#include <cassert>
#include <cstdlib>
bool TransmitterUi::runSimulatorV2Audit()
{
    const auto activate=PageControlEvent::Activate;
    bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
    armRequested_=true;safetyResetRequested_=false;
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
    // Drive is always the saved widget layout, with no fixed mode row.
    assert(widgetViews_[0].panel);
    assert(lv_obj_get_y(widgetViews_[0].panel)==kDashboardWidgetTop);
    for(auto kind:{DashboardWidgetKind::Controls,DashboardWidgetKind::Trims,DashboardWidgetKind::RfMargin}){
        for(unsigned span=0;span<4;++span){
            dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
            auto &w=dashboardLayouts_[activeModelSlot_].cells[0];w.kind=kind;w.width=span&1?2:1;w.height=span&2?2:1;
            drawDashboard();updateDesign(t,OutputSafetyStatus::Cut,99,true);lv_obj_update_layout(lv_scr_act());
            auto &v=widgetViews_[0];assert(v.panel);assert(lv_obj_get_y(v.panel)==kDashboardWidgetTop);
            if(kind==DashboardWidgetKind::RfMargin){
                assert(strcmp(lv_label_get_text(v.values[0]),"38")==0);
                assert(strstr(lv_label_get_text(v.detail),"250 mW"));
                t.linkUpdatedMs=millis()-3001;updateDashboardWidgets(t,99,true);
                assert(strcmp(lv_label_get_text(v.values[0]),"--")==0);
                t.linkUpdatedMs=millis();t.rfMode=255;updateDashboardWidgets(t,99,true);
                assert(strcmp(lv_label_get_text(v.values[0]),"--")==0);
                t.rfMode=27;
            }else if(kind==DashboardWidgetKind::Trims){
                assert(strstr(lv_label_get_text(v.values[0]),"ST trim"));
                assert(strstr(lv_label_get_text(v.values[3]),"Brake"));
                for(unsigned n=0;n<4;++n){const auto r=lv_obj_get_parent(v.values[n]);
                    assert(lv_obj_get_y(r)>=kDashboardWidgetTop&&lv_obj_get_y(r)+lv_obj_get_height(r)<=designFooterY);}
                tap(40);assert(activePage_==UiPage::ValueEditor&&editorField_==5);
                controlDesign(34,activate);assert(activePage_==UiPage::Dashboard);
            }else{
                for(unsigned mode=0;mode<2;++mode)assert(v.switches[mode]);
            }
            if(span==3)shot(kind==DashboardWidgetKind::Controls?"controls":kind==DashboardWidgetKind::Trims?"trims":"rf-margin");
        }
    }
    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
    const DashboardWidgetKind demoKinds[]={DashboardWidgetKind::Controls,DashboardWidgetKind::Trims,DashboardWidgetKind::RfMargin,DashboardWidgetKind::Sensor};
    for(unsigned n=0;n<4;++n)dashboardLayouts_[activeModelSlot_].cells[n].kind=demoKinds[n];
    drawDashboard();updateDashboardWidgets(t,99,true);shot("new-widgets");
    dashboardLayouts_[activeModelSlot_]=layout;drawDashboard();shot("drive");
    if(!portraitUi){
        sensorSelected_=0;drawDesignPage(UiPage::SensorDetail);tap(30);assert(activePage_==UiPage::WidgetSelect);
        sensorSelected_=6;drawDesignPage(UiPage::SensorDetail);tap(30);assert(activePage_==UiPage::DriveBatterySetup);
        sensorSelected_=2;
        auto &history=*telemetryHistory_.get();
        const auto now=millis();
        history.graphs[2].reset(now-31000);
        for(unsigned i=0;i<30;++i){const auto time=now-(29-i)*1000;history.graphs[2].update(7000+i*200,time,true,time);}
        drawDesignPage(UiPage::SensorDetail);lv_obj_update_layout(lv_scr_act());tap(20);assert(v2GraphPaused_);
        assert(history.frozenValid[60] && history.frozen[60]==12800);
        history.graphs[2].reset(now-31000);
        history.graphs[2].update(50000,now,true,now);
        updateDesign(t,OutputSafetyStatus::Cut,99,true);assert(history.frozen[60]==12800);
        tap(21);assert(v2GraphCursor_==59);assert(strstr(lv_label_get_text(designDetails_[6]),"-1s:"));shot("graph");
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
    drawDesignPage(UiPage::Telemetry);tap(2);assert(activePage_==UiPage::RunReview);shot("review");
    controlV2(50,activate);assert(v2ReviewConfirm_);controlV2(51,activate);assert(!v2ReviewConfirm_);
    assert(armRequested_&&!safetyResetRequested_);
    assert(memcmp(&layout,&dashboardLayouts_[activeModelSlot_],sizeof(layout))==0);
    armRequested_=false;designTelemetry_=CrsfTelemetryState{};v2GraphPaused_=false;
    nextChannelSampleMs_=0;
    if (telemetryHistory_) for (auto &history : telemetryHistory_.get()->graphs) history.reset(0);
    drawDashboard();
    Serial.println("V2 native: widget-only Drive, Controls/trim/RF widgets, Data run review, untouched saved layouts, GPS route, sensor shortcuts, graph freeze/cursor, per-model Quick persistence, run review and arm/CUT invariants PASS");
    return true;
}
