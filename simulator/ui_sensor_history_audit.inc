// Real widgets, chart arrays and LVGL layout, using disposable preferences.
assert(telemetryHistory_&&widgetMetrics_);
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;hardwareRecovery_=false;armRequested_=false;
designSafety_=OutputSafetyStatus::Cut;
const auto original=dashboardLayouts_[activeModelSlot_];
const uint32_t now=100000;
for(uint8_t rotation=0;rotation<2;++rotation){
    for(uint8_t size=0;size<4;++size){
        auto &layout=dashboardLayouts_[activeModelSlot_];layout=DashboardLayout{};
        auto &w=layout.cells[0];w.kind=DashboardWidgetKind::Sensor;w.style=DashboardWidgetStyle::Graph;
        w.source=2;w.width=(size&1)?2:1;w.height=(size&2)?2:1;
        // Page creation samples the simulator's real clock. Seed the explicit
        // audit timeline afterwards so that collection cannot overwrite it.
        drawDashboard();lv_obj_update_layout(lv_scr_act());
        auto &history=telemetryHistory_.get()->graphs[2];history.reset(0);
        for(uint32_t i=0;i<=60;++i)history.update(10000+1000*(i%15),now-60000+i*1000,
            !(i>=25&&i<=30),now-60000+i*1000);
        CrsfTelemetryState t{};t.rpmValid=true;t.rpm=70000;t.rpmUpdatedMs=now;
        updateSensorHistoryWidget(0,w,t,now);
        auto &v=widgetViews_[0];assert(v.plot&&lv_obj_check_type(v.plot,&lv_chart_class));
        assert(lv_chart_get_point_count(v.plot)==61);
        assert(lv_obj_get_x(v.plot)+lv_obj_get_width(v.plot)<=lv_obj_get_width(v.panel));
        assert(lv_obj_get_y(v.plot)+lv_obj_get_height(v.plot)<lv_obj_get_height(v.panel));
        const auto data=widgetMetrics_.get()->sensorValues[0];
        assert(data[0]!=LV_CHART_POINT_NONE&&data[25]==LV_CHART_POINT_NONE&&data[31]!=LV_CHART_POINT_NONE);
        assert(!strcmp(lv_label_get_text(v.values[0]),"70000"));
        t.rpmValid=false;updateSensorHistoryWidget(0,w,t,now+4000);
        assert(!strcmp(lv_label_get_text(v.values[0]),"--"));assert(data[60]==LV_CHART_POINT_NONE);
        assert(data[0]!=LV_CHART_POINT_NONE); // Earlier valid segments survive a gap.
        t.rpmValid=true;updateSensorHistoryWidget(0,w,t,now);
        lv_refr_now(nullptr);auditTypography(lv_scr_act());
        char path[128];snprintf(path,sizeof(path),"/tmp/r1-sensor-history-%s-%u.bmp",portraitUi?"portrait":"landscape",size);
        simulatorSaveScreenshot(path);
        history.reset(t.rpmUpdatedMs);updateSensorHistoryWidget(0,w,t,now);
        for(size_t p=0;p<SensorHistory::Capacity;++p)assert(data[p]==LV_CHART_POINT_NONE);
        assert(!armRequested_);
    }
    if(rotation==0){assert(requestDisplayRotation());applyDisplayRotation();}
}
dashboardLayouts_[activeModelSlot_]=original;
designTelemetry_.rpm=90000;designTelemetry_.rpmValid=true;designTelemetry_.rpmUpdatedMs=now;
auto &modelHistory=telemetryHistory_.get()->graphs[2];modelHistory.reset(0);
modelHistory.update(90000,now,true,now);
latestRawAdc_=triggerInput_.centre();
assert(selectModelMemory((activeModelSlot_+1)%kModelMemoryCount));
int32_t modelValues[SensorHistory::Capacity];bool modelValid[SensorHistory::Capacity];
modelHistory.update(90000,now,true,now+1000);
assert(!modelHistory.project(now+1000,modelValues,modelValid).available);
modelHistory.update(80000,now+2000,true,now+2000);
assert(modelHistory.project(now+2000,modelValues,modelValid).available);
assert(!armRequested_);
Serial.println("Sensor history UI: charts, all sizes/rotations, live/stale gaps, RPM range, reset, typography and bounds PASS");
return true;
