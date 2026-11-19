// Focused actual-LVGL checks for the optional trip, run and battery cards.
// Disposable model preferences and synthetic, location-free telemetry only.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;bindingCount_=0;
channelRoleSettings_.steeringChannel=1;channelRoleSettings_.throttleChannel=2;
channelRoleSettings_.steeringEnabled=channelRoleSettings_.throttleEnabled=true;
steeringInput_.setCalibration({1000,2000,3000});triggerInput_.setCalibration({1000,2000,3000});
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
const auto trbCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto trbCuts=auditCutRequests;
const auto trbOriginal=dashboardLayouts_[activeModelSlot_];
static_assert(uint8_t(DashboardWidgetKind::PowerEnergy)==14&&uint8_t(DashboardWidgetKind::ControlHistory)==15&&
    uint8_t(DashboardWidgetKind::TripComputer)==16&&uint8_t(DashboardWidgetKind::RunSummary)==17&&
    uint8_t(DashboardWidgetKind::BatteryLoadBalance)==18,
    "Persisted widget IDs must only be appended");
auto *trbDisplay=lv_disp_get_default();
const auto trbPriorMonitor=trbDisplay->driver->monitor_cb;
trbDisplay->driver->monitor_cb=auditRefreshMonitor;
struct TrbObjectChurn {unsigned created=0;} trbChurn;
auto trbObjectEvent=[](lv_event_t *event){
    if(lv_event_get_code(event)==LV_EVENT_CHILD_CREATED)
        ++static_cast<TrbObjectChurn *>(lv_event_get_user_data(event))->created;
};
lv_obj_add_event_cb(lv_scr_act(),trbObjectEvent,LV_EVENT_ALL,&trbChurn);
auto trbShot=[&](const char *name){
    if(const char *prefix=getenv("JR_R1_TRIP_RUN_BATTERY_WIDGET_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
};
auto trbFit=[&](uint8_t cell){
    auto panel=widgetViews_[cell].panel;assert(panel);
    lv_obj_update_layout(panel);lv_refr_now(nullptr);
    lv_area_t bounds;lv_obj_get_coords(panel,&bounds);std::vector<lv_area_t> labels;
    for(uint32_t n=0;n<lv_obj_get_child_cnt(panel);++n){
        auto child=lv_obj_get_child(panel,n);
        if(!lv_obj_check_type(child,&lv_label_class)||lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))continue;
        const char *text=lv_label_get_text(child);if(!*text)continue;
        lv_area_t area;lv_obj_get_coords(child,&area);
        const auto font=lv_obj_get_style_text_font(child,0);
        const int measured=lv_txt_get_width(text,strlen(text),font,0,LV_TEXT_FLAG_NONE);
        const bool fits=area.x1>=bounds.x1&&area.y1>=bounds.y1&&area.x2<=bounds.x2&&area.y2<=bounds.y2&&
            font->line_height<=lv_obj_get_height(child)&&measured<=lv_obj_get_width(child);
        if(!fits){
            const auto &widget=dashboardWidgetForView(cell);
            Serial.printf("Trip/run/battery label clipped: '%s' measured=%d label=%dx%d font=%u kind=%u span=%ux%u panel=%dx%d screen=%dx%d\n",
                text,measured,int(lv_obj_get_width(child)),int(lv_obj_get_height(child)),font->line_height,unsigned(widget.kind),
                widget.width,widget.height,int(lv_obj_get_width(panel)),int(lv_obj_get_height(panel)),kScreenWidth,kScreenHeight);
            trbShot("fit-failure");
        }
        assert(fits);
        for(const auto &other:labels)
            assert(area.x2<other.x1||area.x1>other.x2||area.y2<other.y1||area.y1>other.y2);
        labels.push_back(area);
    }
    auditTypography(panel);
    assert(!safetyResetRequested_&&auditCutRequests==trbCuts);
};
auto trbTap=[&](uint8_t argument){
    lv_obj_update_layout(lv_scr_act());
    for(uint8_t n=0;n<pageFocusCount_;++n){const auto target=pageFocusTargets_[n];
        if(target.enabled&&target.handler==&TransmitterUi::controlDesign&&target.argument==argument){
            handleTouch(target.activate.x+target.activate.width/2,target.activate.y+target.activate.height/2);return true;
        }
    }
    return false;
};
auto trbPlace=[&](DashboardWidgetKind kind,uint8_t width,uint8_t height,uint8_t cell=0){
    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
    DashboardWidget widget;widget.kind=kind;widget.width=width;widget.height=height;
    placeDashboardWidget(dashboardLayouts_[activeModelSlot_],cell,widget);drawDashboard();
};
const bool trbUnavailable=getenv("JR_R1_SIM_WIDGET_METRICS_ALLOC_FAIL")!=nullptr;
assert(bool(widgetMetrics_)!=trbUnavailable);
const DashboardWidgetKind trbKinds[]={DashboardWidgetKind::TripComputer,DashboardWidgetKind::RunSummary,DashboardWidgetKind::BatteryLoadBalance};
auto trbName=[](DashboardWidgetKind kind){
    return kind==DashboardWidgetKind::TripComputer?"trip":kind==DashboardWidgetKind::RunSummary?"run":"battery";
};
auto trbRender=[&](const CrsfTelemetryState &telemetry,uint32_t now){
    designTelemetry_=telemetry;
    for(uint8_t cell=0;cell<4;++cell)
        if(widgetViews_[cell].panel&&dashboardSessionWidget(dashboardWidgetForView(cell).kind))
            updateTripRunBatteryWidget(cell,now);
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
};
auto trbUnchanged=[&](const CrsfTelemetryState &telemetry,uint32_t now){
    trbRender(telemetry,now);trbChurn.created=0;auditRedrawnPixels=0;
    lv_mem_monitor_t before;lv_mem_monitor(&before);
    for(unsigned n=0;n<20;++n)trbRender(telemetry,now);
    lv_mem_monitor_t after;lv_mem_monitor(&after);
    assert(auditRedrawnPixels==0&&trbChurn.created==0);
    assert(before.used_cnt==after.used_cnt&&before.free_size==after.free_size);
};
auto trbModalFit=[&](){
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    int buttonsY=kScreenHeight;bool foundHelp=false;
    for(uint8_t n=0;n<pageFocusCount_;++n){const auto &target=pageFocusTargets_[n];
        if(target.handler==&TransmitterUi::controlDesign&&(target.argument==158||target.argument==159))
            buttonsY=min(buttonsY,int(target.activate.y));
    }
    for(uint32_t n=0;n<lv_obj_get_child_cnt(lv_scr_act());++n){
        auto child=lv_obj_get_child(lv_scr_act(),n);if(!lv_obj_check_type(child,&lv_label_class))continue;
        const char *text=lv_label_get_text(child);if(!strchr(text,'\n'))continue;
        foundHelp=true;lv_area_t area;lv_obj_get_coords(child,&area);
        const auto *font=lv_obj_get_style_text_font(child,0);lv_point_t required;
        lv_txt_get_size(&required,text,font,0,0,lv_obj_get_width(child),LV_TEXT_FLAG_NONE);
        assert(area.x1>=0&&area.x2<kScreenWidth&&area.y1>=0&&area.y2<buttonsY);
        assert(required.y<=lv_obj_get_height(child));
    }
    assert(foundHelp&&buttonsY<kScreenHeight);auditTypography(lv_scr_act());
};
CrsfTelemetryState trbTelemetry{};
trbTelemetry.ersBatteryValid=true;trbTelemetry.ersVoltageDeciV=160;trbTelemetry.ersCurrentDeciA=100;
trbTelemetry.gpsSpeedValid=true;trbTelemetry.groundSpeedDeciKmh=600;
trbTelemetry.vehicleCellsValid=true;trbTelemetry.vehicleCellCount=4;
trbTelemetry.vehicleCellsMv[0]=3600;trbTelemetry.vehicleCellsMv[1]=3700;
trbTelemetry.vehicleCellsMv[2]=3500;trbTelemetry.vehicleCellsMv[3]=3650;
trbTelemetry.vehicleMinimumCellMv=3500;trbTelemetry.vehiclePackMv=14450;
trbTelemetry.receiverVoltageValid=true;trbTelemetry.receiverVoltageDecivolts=62;
auto trbSeed=[&](uint32_t now){
    auto &metrics=*widgetMetrics_.get();metrics.tripRun.reset();metrics.batteryLoad.reset();
    const uint32_t begin=now-12000;
    trbTelemetry.ersBatteryValid=trbTelemetry.gpsSpeedValid=trbTelemetry.vehicleCellsValid=true;
    trbTelemetry.ersVoltageDeciV=160;trbTelemetry.ersCurrentDeciA=100;trbTelemetry.groundSpeedDeciKmh=600;
    for(unsigned n=0;n<=12;++n){
        const uint32_t stamp=begin+n*1000;
        trbTelemetry.ersCapacityMah=1000+n*10;
        trbTelemetry.ersBatteryUpdatedMs=trbTelemetry.gpsSpeedUpdatedMs=trbTelemetry.vehicleCellsUpdatedMs=stamp;
        metrics.tripRun.update(trbTelemetry,stamp);metrics.batteryLoad.update(trbTelemetry,stamp);
    }
    const auto trip=metrics.tripRun.result(now);const auto battery=metrics.batteryLoad.result(now);
    assert(trip.elapsedValid&&trip.elapsedMs==12000&&trip.movingValid&&trip.movingMs==12000);
    assert(trip.distanceValid&&trip.kilometres>0.19999&&trip.kilometres<0.20001);
    assert(trip.averageMovingValid&&trip.averageMovingKmh>59.99&&trip.averageMovingKmh<60.01);
    assert(trip.maxSpeedValid&&trip.maxSpeedKmh==60&&trip.powerValid&&trip.peakWatts==160);
    assert(trip.usedCapacityValid&&trip.usedMah==120&&trip.energyValid&&trip.wattHours>0.533&&trip.wattHours<0.534);
    assert(battery.packFresh&&battery.cellsFresh&&battery.packExtremaValid);
    assert(battery.voltageDeciV==160&&battery.currentDeciA==100&&battery.lowestCell==3&&battery.lowestCellMv==3500&&battery.spreadMv==200);
};
for(unsigned orientation=0;orientation<2;++orientation){
    for(const auto kind:trbKinds)for(uint8_t cell=0;cell<4;++cell)
        for(uint8_t width=1;width<=2;++width)for(uint8_t height=1;height<=2;++height){
            if(cell%2+width>2||cell/2+height>2)continue;
            const uint32_t now=millis();if(!trbUnavailable)trbSeed(now);
            trbPlace(kind,width,height,cell);trbRender(trbTelemetry,now);trbFit(cell);trbUnchanged(trbTelemetry,now);
            if(trbUnavailable){
                assert(strstr(lv_label_get_text(widgetViews_[cell].detail),"Unavailable")||strstr(lv_label_get_text(widgetViews_[cell].detail),"unavailable"));
            }else{
                const unsigned count=kind==DashboardWidgetKind::TripComputer?4:6;
                for(unsigned n=0;n<count;++n){assert(widgetViews_[cell].values[n]);assert(!strstr(lv_label_get_text(widgetViews_[cell].values[n]),"--"));}
            }
            if(cell==0){char name[64];snprintf(name,sizeof(name),"%s-%ux%u%s",trbName(kind),width,height,trbUnavailable?"-unavailable":"");trbShot(name);}
            const auto layout=dashboardLayouts_[activeModelSlot_];
            lv_area_t area;lv_obj_get_coords(widgetViews_[cell].panel,&area);
            const int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;
            assert(handleDashboardWidgetHold(x,y,x,y,700)&&maximizedWidget_==cell);
            trbRender(trbTelemetry,now);trbFit(cell);trbUnchanged(trbTelemetry,now);
            assert(!lv_obj_has_flag(widgetViews_[cell].title,LV_OBJ_FLAG_HIDDEN));
            if(cell==0){char name[64];snprintf(name,sizeof(name),"%s-maximized%s",trbName(kind),trbUnavailable?"-unavailable":"");trbShot(name);}
            controlDesign(243,activate);assert(maximizedWidget_==-1);
            assert(!memcmp(&layout,&dashboardLayouts_[activeModelSlot_],sizeof(layout)));
        }
    if(!trbUnavailable){
        auto &metrics=*widgetMetrics_.get();const uint32_t now=millis();trbSeed(now);
        // Independent clocks: CU01 is stale at 3s, while CV01 remains live to20s.
        trbPlace(DashboardWidgetKind::BatteryLoadBalance,2,2);
        trbRender(trbTelemetry,now+3001);trbFit(0);
        auto battery=metrics.batteryLoad.result(now+3001);
        assert(!battery.packFresh&&battery.cellsFresh&&battery.packExtremaValid);
        assert(strstr(lv_label_get_text(widgetViews_[0].values[0]),"--")&&strstr(lv_label_get_text(widgetViews_[0].values[1]),"--"));
        assert(!strstr(lv_label_get_text(widgetViews_[0].values[2]),"--")&&!strstr(lv_label_get_text(widgetViews_[0].values[3]),"--"));
        assert(!strstr(lv_label_get_text(widgetViews_[0].values[4]),"--")&&!strstr(lv_label_get_text(widgetViews_[0].values[5]),"--"));
        trbRender(trbTelemetry,now+20001);battery=metrics.batteryLoad.result(now+20001);
        assert(!battery.cellsFresh&&strstr(lv_label_get_text(widgetViews_[0].values[4]),"--")&&strstr(lv_label_get_text(widgetViews_[0].values[5]),"--"));
        trbFit(0);trbShot("battery-stale");
        const auto measured=metrics.tripRun.result(now);
        trbTelemetry.ersBatteryValid=trbTelemetry.gpsSpeedValid=trbTelemetry.vehicleCellsValid=false;
        metrics.tripRun.update(trbTelemetry,now+20001);metrics.batteryLoad.update(trbTelemetry,now+20001);
        const auto retained=metrics.tripRun.result(now+20001);
        assert(retained.kilometres==measured.kilometres&&retained.wattHours==measured.wattHours&&retained.usedMah==measured.usedMah);
        assert(!retained.distanceComplete&&!retained.energyComplete&&!retained.usedCapacityComplete);
        trbPlace(DashboardWidgetKind::RunSummary,2,2);trbRender(trbTelemetry,now+20001);trbFit(0);trbShot("run-stale");

        // GPS alone can provide a trip; RX/cell telemetry never supplies CU01.
        metrics.tripRun.reset();metrics.batteryLoad.reset();
        trbTelemetry.gpsSpeedValid=true;trbTelemetry.vehicleCellsValid=true;
        trbTelemetry.gpsSpeedUpdatedMs=trbTelemetry.vehicleCellsUpdatedMs=now;
        metrics.tripRun.update(trbTelemetry,now);metrics.batteryLoad.update(trbTelemetry,now);
        trbTelemetry.gpsSpeedUpdatedMs=now+1000;metrics.tripRun.update(trbTelemetry,now+1000);
        trbPlace(DashboardWidgetKind::RunSummary,2,2);trbRender(trbTelemetry,now+1000);trbFit(0);
        assert(!strstr(lv_label_get_text(widgetViews_[0].values[0]),"--")&&!strstr(lv_label_get_text(widgetViews_[0].values[1]),"--"));
        for(unsigned n=3;n<6;++n)assert(strstr(lv_label_get_text(widgetViews_[0].values[n]),"--"));
        trbPlace(DashboardWidgetKind::BatteryLoadBalance,2,2);trbRender(trbTelemetry,now+1000);trbFit(0);
        for(unsigned n=0;n<4;++n)assert(strstr(lv_label_get_text(widgetViews_[0].values[n]),"--"));
        assert(!strstr(lv_label_get_text(widgetViews_[0].values[4]),"--"));

        // Genuine reported zero is valid; no GPS position or fix is required.
        metrics.tripRun.reset();metrics.batteryLoad.reset();
        trbTelemetry.groundSpeedDeciKmh=0;trbTelemetry.ersVoltageDeciV=trbTelemetry.ersCurrentDeciA=0;
        trbTelemetry.ersBatteryValid=true;trbTelemetry.ersCapacityMah=0;
        trbTelemetry.gpsValid=false;trbTelemetry.gpsSatellitesValid=false;
        trbTelemetry.gpsSpeedUpdatedMs=trbTelemetry.ersBatteryUpdatedMs=now;
        metrics.tripRun.update(trbTelemetry,now);metrics.batteryLoad.update(trbTelemetry,now);
        trbTelemetry.gpsSpeedUpdatedMs=trbTelemetry.ersBatteryUpdatedMs=now+1000;
        metrics.tripRun.update(trbTelemetry,now+1000);metrics.batteryLoad.update(trbTelemetry,now+1000);
        const auto zero=metrics.tripRun.result(now+1000);assert(zero.distanceValid&&zero.kilometres==0&&zero.maxSpeedValid&&zero.maxSpeedKmh==0&&!zero.averageMovingValid);
        trbPlace(DashboardWidgetKind::TripComputer,1,1);trbRender(trbTelemetry,now+1000);trbFit(0);
        assert(!strstr(lv_label_get_text(widgetViews_[0].values[0]),"--")&&!strstr(lv_label_get_text(widgetViews_[0].values[3]),"--"));
        assert(strstr(lv_label_get_text(widgetViews_[0].values[2]),"--"));trbShot("trip-zero");
        trbPlace(DashboardWidgetKind::BatteryLoadBalance,1,1);trbRender(trbTelemetry,now+1000);trbFit(0);
        assert(!strstr(lv_label_get_text(widgetViews_[0].values[0]),"--")&&!strstr(lv_label_get_text(widgetViews_[0].values[1]),"--"));

        // Packet-domain maxima and long sessions must still fit the small card.
        metrics.tripRun.reset();metrics.batteryLoad.reset();
        trbTelemetry.groundSpeedDeciKmh=65535;trbTelemetry.ersVoltageDeciV=trbTelemetry.ersCurrentDeciA=65535;
        trbTelemetry.ersCapacityMah=0;trbTelemetry.ersBatteryUpdatedMs=trbTelemetry.gpsSpeedUpdatedMs=now-3*24*60*60*1000u;
        metrics.tripRun.update(trbTelemetry,trbTelemetry.ersBatteryUpdatedMs);
        trbTelemetry.ersCapacityMah=0xFFFFFF;trbTelemetry.ersBatteryUpdatedMs=trbTelemetry.gpsSpeedUpdatedMs=now;
        metrics.tripRun.update(trbTelemetry,now);metrics.batteryLoad.update(trbTelemetry,now);
        for(const auto kind:trbKinds){trbPlace(kind,1,1);trbRender(trbTelemetry,now);trbFit(0);}

        // All resets are local and confirmed. Trip and Run share one session;
        // Battery and the existing energy/speed statistics stay independent.
        for(const auto kind:trbKinds){
            trbSeed(now);metrics.power.reset();metrics.power.update(160,100,now,true,600,now,true,now);
            armRequested_=true;designSafety_=OutputSafetyStatus::Armed;
            trbPlace(kind,2,2);trbRender(trbTelemetry,now);
            const auto configuration=controlConfigurationSnapshot();const auto ers=ersConfig_;const auto sequence=ersSequence_;
            const auto resetCuts=auditCutRequests;const bool settingsPending=settingsSavePending_,channelsPending=channelParameterSavePending_;
            SensorStatistics savedStatistics[DashboardSensorCount];
            if(telemetryHistory_)memcpy(savedStatistics,sensorStatistics_.data(),sizeof(savedStatistics));
            const auto beforeTrip=metrics.tripRun.result(now);const auto beforeBattery=metrics.batteryLoad.result(now);
            const auto beforePower=metrics.power.result(now);
            assert(trbTap(154)&&activePage_==UiPage::WidgetMetricsReset);
            assert(trbTap(158)&&activePage_==UiPage::Dashboard);
            assert(metrics.tripRun.result(now).usedMah==beforeTrip.usedMah&&metrics.batteryLoad.result(now).minimumVoltageDeciV==beforeBattery.minimumVoltageDeciV);
            assert(trbTap(154)&&activePage_==UiPage::WidgetMetricsReset);trbModalFit();
            char name[64];snprintf(name,sizeof(name),"%s-reset",trbName(kind));trbShot(name);
            assert(trbTap(159)&&activePage_==UiPage::Dashboard);
            if(kind==DashboardWidgetKind::BatteryLoadBalance){
                assert(!metrics.batteryLoad.result(now).packExtremaValid&&!metrics.batteryLoad.result(now).cellExtremaValid);
                assert(metrics.tripRun.result(now).usedMah==beforeTrip.usedMah&&metrics.tripRun.result(now).kilometres==beforeTrip.kilometres);
            }else{
                assert(!metrics.tripRun.result(now).elapsedValid&&!metrics.tripRun.result(now).distanceValid&&!metrics.tripRun.result(now).usedCapacityValid);
                assert(metrics.batteryLoad.result(now).minimumVoltageDeciV==beforeBattery.minimumVoltageDeciV&&metrics.batteryLoad.result(now).cellsFresh);
            }
            updateWidgetMetrics(trbTelemetry,OutputSafetyStatus::Armed,now);
            if(kind==DashboardWidgetKind::BatteryLoadBalance)assert(!metrics.batteryLoad.result(now).packExtremaValid);
            else assert(!metrics.tripRun.result(now).elapsedValid);
            assert(metrics.power.result(now).peakWatts==beforePower.peakWatts&&metrics.power.result(now).wattHours==beforePower.wattHours);
            assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==resetCuts);
            assert(equivalentControlConfiguration(configuration,controlConfigurationSnapshot()));
            assert(!memcmp(&ers,&ersConfig_,sizeof(ers))&&ersSequence_==sequence);
            if(telemetryHistory_)assert(!memcmp(savedStatistics,sensorStatistics_.data(),sizeof(savedStatistics)));
            assert(settingsSavePending_==settingsPending&&channelParameterSavePending_==channelsPending);
            armRequested_=false;designSafety_=OutputSafetyStatus::Cut;
        }
        // A stale action from a replaced card cannot open a hidden reset panel.
        trbPlace(DashboardWidgetKind::Model,1,1);controlDesign(154,activate);assert(activePage_==UiPage::Dashboard);
        trbSeed(now);dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
        DashboardWidget trip;trip.kind=DashboardWidgetKind::TripComputer;
        DashboardWidget run;run.kind=DashboardWidgetKind::RunSummary;
        DashboardWidget load;load.kind=DashboardWidgetKind::BatteryLoadBalance;load.width=2;
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,trip);
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],1,run);
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],2,load);
        drawDashboard();trbRender(trbTelemetry,now);trbFit(0);trbFit(1);trbFit(2);trbUnchanged(trbTelemetry,now);trbShot("combined-dashboard");
    }
    assert(!armRequested_);assert(requestDisplayRotation());applyDisplayRotation();
}
// Persist all three appended IDs without changing other model layouts.
armRequested_=false;dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
for(uint8_t cell=0;cell<3;++cell){DashboardWidget widget;widget.kind=trbKinds[cell];placeDashboardWidget(dashboardLayouts_[activeModelSlot_],cell,widget);}
const auto trbSaved=dashboardLayouts_[activeModelSlot_];const uint8_t trbOtherSlot=(activeModelSlot_+1)%kModelMemoryCount;
const auto trbOther=dashboardLayouts_[trbOtherSlot];
saveModelMemorySettings();dashboardLayouts_[activeModelSlot_]=DashboardLayout{};loadModelMemorySettings();
assert(!memcmp(&trbSaved,&dashboardLayouts_[activeModelSlot_],sizeof(trbSaved))&&!memcmp(&trbOther,&dashboardLayouts_[trbOtherSlot],sizeof(trbOther)));
assert(!armRequested_&&!safetyResetRequested_&&auditCutRequests==trbCuts);
dashboardLayouts_[activeModelSlot_]=trbOriginal;
if(!trbUnavailable){
    // A model switch rejects old packets and restores the selected model's saved totals.
    const auto previousSlot=activeModelSlot_;const uint32_t now=millis();trbSeed(now);
    const auto savedTrip=widgetMetrics_.get()->tripRun.result(now);
    const auto savedBattery=widgetMetrics_.get()->batteryLoad.result(now);
    designTelemetry_=trbTelemetry;latestRawAdc_=triggerInput_.centre();assert(selectModelMemory(trbOtherSlot));
    auto &metrics=*widgetMetrics_.get();
    assert(!metrics.tripRun.result(now).elapsedValid&&!metrics.batteryLoad.result(now).packExtremaValid);
    updateWidgetMetrics(trbTelemetry,OutputSafetyStatus::Cut,now);
    assert(!metrics.tripRun.result(now).elapsedValid&&!metrics.batteryLoad.result(now).packExtremaValid);
    trbTelemetry.ersBatteryUpdatedMs=trbTelemetry.gpsSpeedUpdatedMs=trbTelemetry.vehicleCellsUpdatedMs=now+1;
    updateWidgetMetrics(trbTelemetry,OutputSafetyStatus::Cut,now+1);
    assert(metrics.tripRun.result(now+1).elapsedValid&&metrics.batteryLoad.result(now+1).packExtremaValid);
    latestRawAdc_=triggerInput_.centre();assert(selectModelMemory(previousSlot));
    const auto restoredTrip=metrics.tripRun.result(millis());
    const auto restoredBattery=metrics.batteryLoad.result(millis());
    assert(restoredTrip.elapsedValid&&restoredTrip.kilometres==savedTrip.kilometres&&!restoredTrip.speedFresh);
    assert(restoredBattery.packExtremaValid&&restoredBattery.minimumVoltageDeciV==savedBattery.minimumVoltageDeciV&&!restoredBattery.packFresh);
    assert(!armRequested_&&safetyResetRequested_&&auditCutRequests==trbCuts+2);
}
lv_obj_remove_event_cb(lv_scr_act(),trbObjectEvent);trbDisplay->driver->monitor_cb=trbPriorMonitor;
safetyCutHandler_=trbCutHandler;
Serial.println("Trip/run/battery widgets: actual LVGL all spans/origins/maximize, labels, independent freshness, zero/missing fields, local reset isolation, zero-redraw/no-allocation, model persistence and cold failure; both orientations PASS");
return true;
