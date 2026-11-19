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
    const auto graphNow=millis();
    auto seed=[&](int32_t peak,bool latestValid){
        auto &history=telemetryHistory_.get()->graphs[2];
        history.reset(graphNow-31000);
        for(unsigned i=0;i<30;++i){
            const auto time=graphNow-(29-i)*1000;
            history.update(i==12?peak:10000+int(i)*100,time,i!=29||latestValid,time);
        }
    };
    seed(11200,true);
    nextChannelSampleMs_=millis()+1000000;
    channelHistoryCount_=30;
    for(unsigned i=0;i<30;++i){
        channelHistory_[0][i]=crsfValueFromPermille(-500+int(i)*20);
        channelHistory_[1][i]=crsfValueFromPermille(500-int(i)*20);
    }
    auto fresh=[&](){graphTelemetry.linkUpdatedMs=graphTelemetry.rpmUpdatedMs=millis();};
    auto detailUpdate=[&](){updateSensorDetail(graphNow);lv_refr_now(nullptr);};
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
    changed([&](){seed(45000,true);},detailUpdate); // geometry and axis range
    unchanged(detailUpdate);
    changed([&](){seed(45000,false);},detailUpdate); // preserve earlier segments across stale gaps
    assert(sensorDetailPoints_[59]!=LV_CHART_POINT_NONE&&sensorDetailPoints_[60]==LV_CHART_POINT_NONE);
    unchanged(detailUpdate);
    changed([&](){seed(45000,true);},detailUpdate);
    assert(sensorDetailPoints_[60]!=LV_CHART_POINT_NONE);
    // Pause freezes graph geometry while readings/statistics can continue.
    controlV2(20,PageControlEvent::Activate);detailUpdate();unchanged(detailUpdate);
    seed(80000,true);unchanged(detailUpdate);
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
    changed([&](){channelHistoryCount_=29;},widgetUpdate); // point count is part of the key
    unchanged(widgetUpdate);channelHistoryCount_=30;

    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
    DashboardWidget sensor;sensor.kind=DashboardWidgetKind::Sensor;sensor.source=2;
    sensor.style=DashboardWidgetStyle::Graph;sensor.width=sensor.height=2;
    placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,sensor);
    auto sensorUpdate=[&](){updateSensorHistoryWidget(0,sensor,graphTelemetry,graphNow);lv_refr_now(nullptr);};
    // Fix the graph's projection time so the real minute window does not
    // scroll between otherwise identical updates on a slower test host.
    drawDashboard();unchanged(sensorUpdate);
    changed([&](){seed(-1000,true);},sensorUpdate);unchanged(sensorUpdate);
    changed([&](){graphTelemetry.rpmValid=false;},sensorUpdate);unchanged(sensorUpdate);
    changed([&](){graphTelemetry.rpmValid=true;},sensorUpdate);unchanged(sensorUpdate);
    changed([&](){seed(-1000,false);},sensorUpdate);
    const auto *points=widgetMetrics_.get()->sensorValues[0];
    assert(points[59]!=LV_CHART_POINT_NONE&&points[60]==LV_CHART_POINT_NONE);unchanged(sensorUpdate);
    changed([&](){seed(-1000,true);},sensorUpdate);
    assert(points[60]!=LV_CHART_POINT_NONE);unchanged(sensorUpdate);
    // Recreate pages/objects so a recycled LVGL address cannot suppress the
    // initial graph, then rotate and repeat at the other panel geometry.
    drawMainMenu();drawDashboard();unchanged(sensorUpdate);
    assert(requestDisplayRotation());applyDisplayRotation();
}
graphAuditDisplay->driver->monitor_cb=savedGraphMonitor;
Serial.println("Graph redraw: 20 unchanged updates=0 pixels; new samples/range/count/stale, frozen cursor, recreated pages and both orientations PASS");
return true;
