// Header rendering uses synthetic UTC only; no live location or clock changes.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;hardwareRecovery_=false;
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
const auto timeCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto timeCuts=auditCutRequests;
char timeOriginalName[sizeof(modelNames_[0])];memcpy(timeOriginalName,modelNames_[activeModelSlot_],sizeof(timeOriginalName));
memset(modelNames_[activeModelSlot_],'W',sizeof(modelNames_[0])-1);modelNames_[activeModelSlot_][sizeof(modelNames_[0])-1]=0;
const bool timeOriginalAbs=brakeAbs_.enabled,timeOriginalNight=uiNightMode;
const uint8_t timeOriginalScheme=uiDarkColourScheme;
auto *timeDisplay=lv_disp_get_default();const auto timePriorMonitor=timeDisplay->driver->monitor_cb;
timeDisplay->driver->monitor_cb=auditRefreshMonitor;
struct TimeObjectChurn {unsigned created=0;} timeChurn;
auto timeObjectEvent=[](lv_event_t *event){
    if(lv_event_get_code(event)==LV_EVENT_CHILD_CREATED)
        ++static_cast<TimeObjectChurn *>(lv_event_get_user_data(event))->created;
};
lv_obj_add_event_cb(lv_scr_act(),timeObjectEvent,LV_EVENT_ALL,&timeChurn);
auto timeFit=[&]{
    lv_obj_update_layout(lv_scr_act());
    lv_area_t clock,voltage,link;lv_obj_get_coords(headerGpsTimeLabel,&clock);
    lv_obj_get_coords(headerTxVoltageLabel,&voltage);lv_obj_get_coords(designLink_,&link);
    assert(clock.x1>=0&&clock.y1>=0&&clock.x2<kScreenWidth&&clock.y2<44);
    const auto *font=lv_obj_get_style_text_font(headerGpsTimeLabel,0);
    assert(lv_txt_get_width(lv_label_get_text(headerGpsTimeLabel),strlen(lv_label_get_text(headerGpsTimeLabel)),font,0,LV_TEXT_FLAG_NONE)<=lv_obj_get_width(headerGpsTimeLabel));
    assert(clock.y1>voltage.y2);
    assert(clock.x2<link.x1);
    for(uint32_t n=0;n<lv_obj_get_child_cnt(lv_scr_act());++n){
        auto object=lv_obj_get_child(lv_scr_act(),n);
        if(object==headerGpsTimeLabel||!lv_obj_check_type(object,&lv_label_class)||lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))continue;
        lv_area_t area;lv_obj_get_coords(object,&area);
        if(area.y1>=44)continue;
        assert(clock.x2<area.x1||clock.x1>area.x2||clock.y2<area.y1||clock.y1>area.y2);
    }
    auditTypography(lv_scr_act());
};
auto timeShot=[&](const char *name){
    if(const char *prefix=getenv("JR_R1_GPS_TIME_HEADER_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
        lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
};
for(unsigned orientation=0;orientation<2;++orientation){
    for(unsigned theme=0;theme<=darkColourSchemeCount;++theme){
        uiNightMode=theme!=0;uiDarkColourScheme=theme?theme-1:0;
        for(bool abs:{false,true}){
            brakeAbs_.enabled=abs;designTelemetry_=CrsfTelemetryState{};drawDashboard();
            assert(headerGpsTimeLabel&&lv_obj_has_flag(headerGpsTimeLabel,LV_OBJ_FLAG_HIDDEN));
            auto *clock=headerGpsTimeLabel;const auto focusCount=pageFocusCount_;
            CrsfTelemetryState telemetry;telemetry.gpsTimeValid=true;
            telemetry.gpsUtcYear=2026;telemetry.gpsUtcMonth=11;telemetry.gpsUtcDay=15;
            telemetry.gpsUtcHour=23;telemetry.gpsUtcMinute=59;telemetry.gpsTimeUpdatedMs=millis();
            // UTC appears without position, satellites, or a link-status report.
            designTelemetry_=telemetry;updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(!lv_obj_has_flag(clock,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(clock),"23:59Z"));
            assert(pageFocusCount_==focusCount&&clock==headerGpsTimeLabel);timeFit();
            if(theme==0&&abs)timeShot("utc-abs");
            lv_refr_now(nullptr);timeChurn.created=0;auditRedrawnPixels=0;
            lv_mem_monitor_t before;lv_mem_monitor(&before);
            for(unsigned n=0;n<20;++n){updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);lv_refr_now(nullptr);}
            lv_mem_monitor_t after;lv_mem_monitor(&after);
            assert(timeChurn.created==0&&auditRedrawnPixels==0);
            assert(before.used_cnt==after.used_cnt&&before.free_size==after.free_size);
            telemetry.gpsUtcHour=0;telemetry.gpsUtcMinute=0;telemetry.gpsTimeUpdatedMs=millis();
            updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(clock==headerGpsTimeLabel&&!strcmp(lv_label_get_text(clock),"00:00Z"));
            telemetry.gpsTimeUpdatedMs=millis()-3001;updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(clock==headerGpsTimeLabel&&lv_obj_has_flag(clock,LV_OBJ_FLAG_HIDDEN));
            telemetry.gpsTimeUpdatedMs=millis();updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(!lv_obj_has_flag(clock,LV_OBJ_FLAG_HIDDEN)&&pageFocusCount_==focusCount);
            designTelemetry_=telemetry;drawDesignPage(UiPage::BrakeAbs);timeFit();
            if(theme==0&&abs)timeShot("detail-utc-abs");
            // Legacy detail headers clear the clock pointer before deleting it.
            drawWifiPage();assert(!headerGpsTimeLabel);updateHeaderGpsTime(telemetry,millis());
            drawDashboard();assert(headerGpsTimeLabel&&!lv_obj_has_flag(headerGpsTimeLabel,LV_OBJ_FLAG_HIDDEN));
            assert(!armRequested_&&!safetyResetRequested_&&auditCutRequests==timeCuts);
        }
    }
    assert(requestDisplayRotation());applyDisplayRotation();
}
lv_obj_remove_event_cb_with_user_data(lv_scr_act(),timeObjectEvent,&timeChurn);
timeDisplay->driver->monitor_cb=timePriorMonitor;safetyCutHandler_=timeCutHandler;
memcpy(modelNames_[activeModelSlot_],timeOriginalName,sizeof(timeOriginalName));
brakeAbs_.enabled=timeOriginalAbs;uiNightMode=timeOriginalNight;uiDarkColourScheme=timeOriginalScheme;
Serial.println("GPS UTC header: fresh/absent/stale, midnight, both orientations/palettes, ABS/title bounds, stable objects and output guards PASS");
return true;
