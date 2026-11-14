// Included inside runSimulatorUiAudit: disposable preferences, actual LVGL
// monitor pixels, and production page/widget updates in both dimensions.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
armRequested_=false;startupPreflightComplete_=true;
auto *graphAuditDisplay=lv_disp_get_default();
const auto savedGraphMonitor=graphAuditDisplay->driver->monitor_cb;
graphAuditDisplay->driver->monitor_cb=auditRefreshMonitor;
for(unsigned orientation=0;orientation<2;++orientation){
    CrsfTelemetryState graphTelemetry{};
    graphTelemetry.linkValid=true;graphTelemetry.uplinkLinkQuality=100;
    graphTelemetry.linkUpdatedMs=millis();
    graphTelemetry.rpmValid=true;graphTelemetry.rpm=20000;graphTelemetry.rpmUpdatedMs=millis();
    nextSensorSampleMs_=millis()+1000000;
    sensorHistoryCount_=30;
    for(unsigned i=0;i<30;++i){
        sensorHistory_[2][i]=10000+int(i)*100;
        sensorHistoryValid_[2][i]=true;
        channelHistory_[0][i]=crsfValueFromPermille(-500+int(i)*20);
        channelHistory_[1][i]=crsfValueFromPermille(500-int(i)*20);
    }
    auto fresh=[&](){graphTelemetry.linkUpdatedMs=graphTelemetry.rpmUpdatedMs=millis();};
    auto detailUpdate=[&](){fresh();updateDesign(graphTelemetry,OutputSafetyStatus::Cut,99,true);lv_refr_now(nullptr);};
    auto widgetUpdate=[&](){fresh();updateDashboardWidgets(graphTelemetry,99,true);lv_refr_now(nullptr);};
    unsigned check=0;
    auto unchanged=[&](auto update){
        ++check;
        update();lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        auditRedrawnPixels=0;
        for(unsigned i=0;i<20;++i)update();
        if(auditRedrawnPixels)Serial.printf("Graph unchanged check %u (%dx%d) redrew %llu pixels\n",check,kScreenWidth,kScreenHeight,static_cast<unsigned long long>(auditRedrawnPixels));
        assert(auditRedrawnPixels==0);
    };
    auto changed=[&](auto change,auto update){auditRedrawnPixels=0;change();update();assert(auditRedrawnPixels>0);};

    sensorSelected_=2;v2GraphPaused_=false;drawDesignPage(UiPage::SensorDetail);
    unchanged(detailUpdate);
    changed([&](){sensorHistory_[2][12]=45000;},detailUpdate); // geometry and axis range
    unchanged(detailUpdate);
    changed([&](){sensorHistoryValid_[2][29]=false;},detailUpdate); // never join across stale gaps
    assert(lv_obj_has_flag(designCurve_,LV_OBJ_FLAG_HIDDEN));unchanged(detailUpdate);
    changed([&](){sensorHistoryValid_[2][29]=true;},detailUpdate);
    assert(!lv_obj_has_flag(designCurve_,LV_OBJ_FLAG_HIDDEN));
    // Pause freezes graph geometry while readings/statistics can continue.
    controlV2(20,PageControlEvent::Activate);detailUpdate();unchanged(detailUpdate);
    sensorHistory_[2][12]=80000;unchanged(detailUpdate);
    if(v2GraphCursorLine_){
        changed([&](){--v2GraphCursor_;},detailUpdate);unchanged(detailUpdate);
    }
    controlV2(20,PageControlEvent::Activate);detailUpdate();unchanged(detailUpdate);

    // One full-sized channel widget exercises both stable backing point arrays.
    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
    DashboardWidget channels;channels.kind=DashboardWidgetKind::Channels;
    channels.source=0;channels.secondSource=1;channels.style=DashboardWidgetStyle::Graph;
    channels.width=channels.height=2;placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,channels);
    drawDashboard();unchanged(widgetUpdate);
    changed([&](){channelHistory_[0][15]=crsfValueFromPermille(900);},widgetUpdate);
    unchanged(widgetUpdate);
    changed([&](){sensorHistoryCount_=29;},widgetUpdate); // point count is part of the key
    unchanged(widgetUpdate);sensorHistoryCount_=30;

    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
    DashboardWidget sensor;sensor.kind=DashboardWidgetKind::Sensor;sensor.source=2;
    sensor.style=DashboardWidgetStyle::Graph;sensor.width=sensor.height=2;
    placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,sensor);
    drawDashboard();unchanged(widgetUpdate);
    changed([&](){sensorHistory_[2][10]=-1000;},widgetUpdate);unchanged(widgetUpdate);
    changed([&](){graphTelemetry.rpmValid=false;},widgetUpdate);unchanged(widgetUpdate);
    changed([&](){graphTelemetry.rpmValid=true;},widgetUpdate);unchanged(widgetUpdate);
    changed([&](){sensorHistoryValid_[2][29]=false;},widgetUpdate);
    assert(lv_obj_has_flag(widgetViews_[0].plot,LV_OBJ_FLAG_HIDDEN));unchanged(widgetUpdate);
    changed([&](){sensorHistoryValid_[2][29]=true;},widgetUpdate);
    assert(!lv_obj_has_flag(widgetViews_[0].plot,LV_OBJ_FLAG_HIDDEN));unchanged(widgetUpdate);
    // Recreate pages/objects so a recycled LVGL address cannot suppress the
    // initial graph, then rotate and repeat at the other panel geometry.
    drawMainMenu();drawDashboard();widgetUpdate();unchanged(widgetUpdate);
    assert(requestDisplayRotation());applyDisplayRotation();
}
graphAuditDisplay->driver->monitor_cb=savedGraphMonitor;
Serial.println("Graph redraw: 20 unchanged updates=0 pixels; new samples/range/count/stale, frozen cursor, recreated pages and both orientations PASS");
return true;
