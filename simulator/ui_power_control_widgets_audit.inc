// Actual LVGL objects/events and disposable model preferences. Time is supplied
// explicitly to collectors; advancing LVGL's clock must not retimestamp inputs.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;bindingCount_=0;
channelRoleSettings_.steeringChannel=1;channelRoleSettings_.throttleChannel=2;
channelRoleSettings_.steeringEnabled=channelRoleSettings_.throttleEnabled=true;
steeringInput_.setCalibration({1000,2000,3000});triggerInput_.setCalibration({1000,2000,3000});
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
const auto metricsCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto metricsCuts=auditCutRequests;
const auto metricsOriginal=dashboardLayouts_[activeModelSlot_];
static_assert(uint8_t(DashboardWidgetKind::Empty)==0&&uint8_t(DashboardWidgetKind::Timer)==1&&
    uint8_t(DashboardWidgetKind::Power)==2&&uint8_t(DashboardWidgetKind::Channels)==3&&
    uint8_t(DashboardWidgetKind::Sensor)==4&&uint8_t(DashboardWidgetKind::Map)==5&&
    uint8_t(DashboardWidgetKind::Model)==6&&uint8_t(DashboardWidgetKind::AllChannels)==7&&
    uint8_t(DashboardWidgetKind::BatteryCells)==8&&uint8_t(DashboardWidgetKind::Course)==9&&
    uint8_t(DashboardWidgetKind::Controls)==10&&uint8_t(DashboardWidgetKind::Trims)==11&&
    uint8_t(DashboardWidgetKind::RfMargin)==12&&uint8_t(DashboardWidgetKind::DrivePack)==13&&
    uint8_t(DashboardWidgetKind::PowerEnergy)==14&&uint8_t(DashboardWidgetKind::ControlHistory)==15,
    "Persisted dashboard kinds must only be appended");
auto *metricsDisplay=lv_disp_get_default();
const auto metricsPriorMonitor=metricsDisplay->driver->monitor_cb;
metricsDisplay->driver->monitor_cb=auditRefreshMonitor;
struct MetricsObjectChurn {unsigned created=0;} metricsChurn;
auto metricsObjectEvent=[](lv_event_t *event){
    if(lv_event_get_code(event)==LV_EVENT_CHILD_CREATED)
        ++static_cast<MetricsObjectChurn *>(lv_event_get_user_data(event))->created;
};
lv_obj_add_event_cb(lv_scr_act(),metricsObjectEvent,LV_EVENT_ALL,&metricsChurn);
auto metricsShot=[&](const char *name){
    if(const char *prefix=getenv("JR_R1_POWER_CONTROL_WIDGET_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
};
auto metricsFit=[&](uint8_t cell){
    auto panel=widgetViews_[cell].panel;assert(panel);
    lv_obj_update_layout(panel);lv_refr_now(nullptr);
    lv_area_t bounds;lv_obj_get_coords(panel,&bounds);std::vector<lv_area_t> labels;
    for(uint32_t n=0;n<lv_obj_get_child_cnt(panel);++n){
        auto child=lv_obj_get_child(panel,n);
        if(!lv_obj_check_type(child,&lv_label_class)||lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))continue;
        const char *text=lv_label_get_text(child);if(!*text)continue;
        lv_area_t area;lv_obj_get_coords(child,&area);
        if(area.x1<bounds.x1||area.y1<bounds.y1||area.x2>bounds.x2||area.y2>bounds.y2)
            Serial.printf("Metrics label outside %dx%d panel: '%s' (%d,%d,%d,%d) / (%d,%d,%d,%d)\n",kScreenWidth,kScreenHeight,text,
                area.x1,area.y1,area.x2,area.y2,bounds.x1,bounds.y1,bounds.x2,bounds.y2);
        assert(area.x1>=bounds.x1&&area.y1>=bounds.y1&&area.x2<=bounds.x2&&area.y2<=bounds.y2);
        const auto font=lv_obj_get_style_text_font(child,0);
        assert(font->line_height<=lv_obj_get_height(child));
        const int measured=lv_txt_get_width(text,strlen(text),font,0,LV_TEXT_FLAG_NONE);
        if(measured>lv_obj_get_width(child)){
            const auto &widget=dashboardWidgetForView(cell);
            Serial.printf("Metrics text clipped: text='%s' measured=%d label=%d font=%u kind=%u span=%ux%u panel=%dx%d screen=%dx%d\n",
                text,measured,int(lv_obj_get_width(child)),font->line_height,unsigned(widget.kind),widget.width,widget.height,
                int(lv_obj_get_width(panel)),int(lv_obj_get_height(panel)),kScreenWidth,kScreenHeight);
            metricsShot("fit-failure");
        }
        assert(measured<=lv_obj_get_width(child));
        for(const auto &other:labels)
            assert(area.x2<other.x1||area.x1>other.x2||area.y2<other.y1||area.y1>other.y2);
        labels.push_back(area);
    }
    auditTypography(panel);
    assert(!safetyResetRequested_&&auditCutRequests==metricsCuts);
};
auto metricsTap=[&](uint8_t argument){
    lv_obj_update_layout(lv_scr_act());
    for(uint8_t n=0;n<pageFocusCount_;++n){const auto target=pageFocusTargets_[n];
        if(target.handler==&TransmitterUi::controlDesign&&target.argument==argument){
            handleTouch(target.activate.x+target.activate.width/2,target.activate.y+target.activate.height/2);return true;
        }
    }
    return false;
};
auto metricsModalFit=[&](){
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    bool found=false;int buttonsY=kScreenHeight;
    for(uint8_t n=0;n<pageFocusCount_;++n){const auto &target=pageFocusTargets_[n];
        if(target.handler==&TransmitterUi::controlDesign&&(target.argument==158||target.argument==159))
            buttonsY=min(buttonsY,int(target.activate.y));
    }
    for(uint32_t n=0;n<lv_obj_get_child_cnt(lv_scr_act());++n){
        auto child=lv_obj_get_child(lv_scr_act(),n);if(!lv_obj_check_type(child,&lv_label_class))continue;
        const char *text=lv_label_get_text(child);
        if(!strstr(text,"Clear peak power"))continue;found=true;
        lv_area_t area;lv_obj_get_coords(child,&area);const auto font=lv_obj_get_style_text_font(child,0);
        lv_point_t required;lv_txt_get_size(&required,text,font,0,0,lv_obj_get_width(child),LV_TEXT_FLAG_NONE);
        assert(area.x1>=0&&area.x2<kScreenWidth&&area.y1>=0&&area.y2<buttonsY);
        assert(required.y<=lv_obj_get_height(child));
    }
    assert(found&&buttonsY<kScreenHeight);auditTypography(lv_scr_act());
};
auto metricsRender=[&](const CrsfTelemetryState &telemetry){
    designTelemetry_=telemetry;updateDashboardWidgets(telemetry,99,true);
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
};
auto metricsUnchanged=[&](const CrsfTelemetryState &telemetry){
    metricsRender(telemetry);const uint32_t renderNow=millis();
    const bool controls=dashboardWidgetForView(0).kind==DashboardWidgetKind::ControlHistory;
    auto unchangedRender=[&](){
        // A moving time axis is a changed graph input. Hold its explicit render
        // time fixed here, rather than racing a real 100 ms clock boundary.
        if(controls){updateControlHistoryWidget(0,renderNow);lv_refr_now(nullptr);}
        else metricsRender(telemetry);
    };
    unchangedRender();metricsChurn.created=0;auditRedrawnPixels=0;
    lv_mem_monitor_t before;lv_mem_monitor(&before);
    for(unsigned n=0;n<20;++n)unchangedRender();
    lv_mem_monitor_t after;lv_mem_monitor(&after);
    assert(auditRedrawnPixels==0&&metricsChurn.created==0);
    assert(before.used_cnt==after.used_cnt&&before.free_size==after.free_size);
};
auto metricsPlace=[&](DashboardWidgetKind kind,uint8_t width,uint8_t height,uint8_t cell=0){
    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
    DashboardWidget widget;widget.kind=kind;widget.width=width;widget.height=height;
    if(kind==DashboardWidgetKind::ControlHistory)widget.style=DashboardWidgetStyle::Graph;
    placeDashboardWidget(dashboardLayouts_[activeModelSlot_],cell,widget);drawDashboard();
};
CrsfTelemetryState metricsTelemetry{};
metricsTelemetry.ersBatteryValid=true;metricsTelemetry.ersVoltageDeciV=160;metricsTelemetry.ersCurrentDeciA=100;
metricsTelemetry.gpsSpeedValid=true;metricsTelemetry.groundSpeedDeciKmh=600;
metricsTelemetry.receiverVoltageValid=true;metricsTelemetry.receiverVoltageDecivolts=62;
metricsTelemetry.vehicleCellsValid=true;metricsTelemetry.vehiclePackMv=14800;
const bool metricsUnavailable=getenv("JR_R1_SIM_WIDGET_METRICS_ALLOC_FAIL")!=nullptr;
assert(bool(widgetMetrics_)!=metricsUnavailable);
if(metricsUnavailable){
    for(unsigned orientation=0;orientation<2;++orientation){
        for(const auto kind:{DashboardWidgetKind::PowerEnergy,DashboardWidgetKind::ControlHistory}){
            metricsPlace(kind,1,1);metricsRender(metricsTelemetry);metricsFit(0);metricsUnchanged(metricsTelemetry);
            assert(widgetViews_[0].detail&&(strstr(lv_label_get_text(widgetViews_[0].detail),"unavailable")||
                strstr(lv_label_get_text(widgetViews_[0].detail),"Unavailable")));
            metricsShot(kind==DashboardWidgetKind::PowerEnergy?"power-unavailable":"controls-unavailable");
        }
        assert(!armRequested_);assert(requestDisplayRotation());applyDisplayRotation();
    }
}else{
    auto seedPower=[&](uint32_t now){
        const uint32_t begin=now-12000;widgetMetrics_.get()->power.reset(begin-1,begin-1);
        for(unsigned n=0;n<=12;++n){const auto stamp=begin+n*1000;
            widgetMetrics_.get()->power.update(160,100,stamp,true,600,stamp,true,stamp);
        }
        metricsTelemetry.ersBatteryUpdatedMs=metricsTelemetry.gpsSpeedUpdatedMs=now;
    };
    auto seedControls=[&](){
        widgetMetrics_.get()->controls.reset();const uint32_t now=millis(),begin=now-10000;
        for(unsigned n=0;n<=100;++n){
            mixingLiveSamples_.values[0]=int(n%21)*100-1000;
            mixingLiveSamples_.values[1]=1000-int(n%21)*100;
            mixingLiveSamples_.valid=3;mixingLiveUpdatedMs_=begin+n*100;
            updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,mixingLiveUpdatedMs_);
        }
        assert(widgetMetrics_.get()->controls.count()==101);
        const auto &last=widgetMetrics_.get()->controls.sample(100);
        assert(last.steeringValid&&last.throttleValid&&last.steering==600&&last.throttle==-600);
    };
    auto controlsArray=[&](uint8_t cell,uint8_t series){
        auto pointer=widgetMetrics_.get()->controlSeries[cell][series];
        assert(widgetViews_[cell].plot&&pointer);
        return lv_chart_get_y_array(widgetViews_[cell].plot,pointer);
    };
    for(unsigned orientation=0;orientation<2;++orientation){
        for(const auto kind:{DashboardWidgetKind::PowerEnergy,DashboardWidgetKind::ControlHistory})
            for(uint8_t width:{1,2})for(uint8_t height:{1,2}){
                seedControls();seedPower(millis());metricsPlace(kind,width,height);metricsRender(metricsTelemetry);metricsFit(0);
                if(kind==DashboardWidgetKind::ControlHistory){
                    assert(strstr(lv_label_get_text(widgetViews_[0].detail),"CUT"));
                    assert(lv_chart_get_point_count(widgetViews_[0].plot)==101);
                    const auto *steering=controlsArray(0,0),*throttle=controlsArray(0,1);
                    // Chart values are driver input percentages, including negative
                    // trigger values, without claiming braking/reverse state.
                    assert(steering[100]==600&&throttle[100]==-600);
                    auto &limit=const_cast<ControlHistory::Sample &>(widgetMetrics_.get()->controls.sample(100));
                    limit.steering=-1000;limit.throttle=1000;
                    updateControlHistoryWidget(0,millis());metricsFit(0);
                    limit.steering=600;limit.throttle=-600;
                    updateControlHistoryWidget(0,millis());
                }else{
                    const auto power=widgetMetrics_.get()->power.result(millis());
                    assert(power.powerValid&&power.peakValid&&power.energyValid&&power.efficiencyValid);
                    assert(power.watts==160&&power.peakWatts==160);
                    assert(power.wattHours>0.53&&power.wattHours<0.54&&power.kilometres>0.19&&power.kilometres<0.21);
                }
                metricsUnchanged(metricsTelemetry);
                char name[48];snprintf(name,sizeof(name),"%s-%ux%u",kind==DashboardWidgetKind::PowerEnergy?"power":"controls",width,height);metricsShot(name);
                const auto layout=dashboardLayouts_[activeModelSlot_];
                lv_area_t area;lv_obj_get_coords(widgetViews_[0].panel,&area);
                const int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;
                assert(handleDashboardWidgetHold(x,y,x,y,700)&&maximizedWidget_==0);
                metricsRender(metricsTelemetry);metricsFit(0);metricsUnchanged(metricsTelemetry);
                metricsShot(kind==DashboardWidgetKind::PowerEnergy?"power-maximized":"controls-maximized");
                controlDesign(243,activate);assert(maximizedWidget_==-1);
                assert(!memcmp(&layout,&dashboardLayouts_[activeModelSlot_],sizeof(layout)));
            }
        // Rendering missing/stale CU01 hides instantaneous power but retains
        // measured session totals, and never substitutes receiver/cell voltage.
        const uint32_t staleStamp=millis()-4000;seedPower(staleStamp);
        const auto complete=widgetMetrics_.get()->power.result(staleStamp);
        metricsPlace(DashboardWidgetKind::PowerEnergy,2,2);metricsRender(metricsTelemetry);
        assert(strstr(lv_label_get_text(widgetViews_[0].values[0]),"--"));
        auto retained=widgetMetrics_.get()->power.result(millis());
        assert(!retained.powerValid&&retained.wattHours==complete.wattHours&&retained.peakWatts==complete.peakWatts);
        updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,millis());
        metricsTelemetry.ersBatteryValid=false;updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,millis());
        metricsRender(metricsTelemetry);metricsFit(0);metricsShot("power-stale");
        retained=widgetMetrics_.get()->power.result(millis());assert(retained.wattHours==complete.wattHours);
        metricsTelemetry.ersBatteryValid=true;
        metricsTelemetry.ersBatteryUpdatedMs=metricsTelemetry.gpsSpeedUpdatedMs=millis();
        updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,millis());metricsRender(metricsTelemetry);
        const auto recovered=widgetMetrics_.get()->power.result(millis());
        assert(recovered.powerValid&&!recovered.energyComplete&&recovered.wattHours==complete.wattHours);
        assert(strchr(lv_label_get_text(widgetViews_[0].values[2]),'~'));metricsFit(0);metricsShot("power-recovered-gap");
        seedPower(millis());

        // Responses, mixing and final sent values are deliberately different
        // from these calibrated role inputs. CUT still records valid inputs.
        auto changedProfile=profileForChannel(1);changedProfile.reversed=true;
        changedProfile.positiveExpoPercent=80;changedProfile.negativeExpoPercent=-50;
        channelModel_.setResponse(1,changedProfile);channelModel_.parameters[0].trimPermille=175;
        channelModel_.parameters[0].subtrimPermille=-150;channelModel_.mixing[0].constantPercent=60;
        for(auto &value:dashboardChannelValues_)value=CRSF_CHANNEL_MID;
        widgetMetrics_.get()->controls.reset();const uint32_t controlNow=millis();
        mixingLiveSamples_.values[0]=-750;mixingLiveSamples_.values[1]=400;mixingLiveSamples_.valid=3;
        mixingLiveUpdatedMs_=controlNow-100;updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,controlNow-100);
        const auto beforeConfiguration=controlConfigurationSnapshot();
        mixingLiveUpdatedMs_=controlNow;updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,controlNow);
        const auto &cutInput=widgetMetrics_.get()->controls.sample(widgetMetrics_.get()->controls.count()-1);
        assert(cutInput.steeringValid&&cutInput.throttleValid&&cutInput.steering==-750&&cutInput.throttle==400);
        assert(!armRequested_&&equivalentControlConfiguration(beforeConfiguration,controlConfigurationSnapshot()));
        // Old data must not become fresh merely because the UI polls again.
        updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,controlNow+400);
        auto gap=widgetMetrics_.get()->controls.sample(widgetMetrics_.get()->controls.count()-1);
        assert(!gap.steeringValid&&!gap.throttleValid);
        for(const auto fault:{OutputSafetyStatus::InputFault,OutputSafetyStatus::ControlStale,OutputSafetyStatus::Calibration}){
            widgetMetrics_.get()->controls.reset();mixingLiveUpdatedMs_=millis();
            updateWidgetMetrics(metricsTelemetry,fault,mixingLiveUpdatedMs_);
            gap=widgetMetrics_.get()->controls.sample(widgetMetrics_.get()->controls.count()-1);
            assert(!gap.steeringValid&&!gap.throttleValid);
        }
        widgetMetrics_.get()->controls.reset();channelRoleSettings_.steeringEnabled=false;
        mixingLiveUpdatedMs_=millis();updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,mixingLiveUpdatedMs_);
        gap=widgetMetrics_.get()->controls.sample(0);assert(!gap.steeringValid&&gap.throttleValid);
        channelRoleSettings_.steeringEnabled=true;
        widgetMetrics_.get()->controls.reset();mixingLiveSamples_.valid=1;mixingLiveUpdatedMs_=millis();
        updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,mixingLiveUpdatedMs_);
        gap=widgetMetrics_.get()->controls.sample(0);assert(gap.steeringValid&&!gap.throttleValid);
        mixingLiveSamples_.valid=3;widgetMetrics_.get()->controls.reset();
        steeringInput_.startCalibration(2000);mixingLiveUpdatedMs_=millis();
        updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,mixingLiveUpdatedMs_);
        gap=widgetMetrics_.get()->controls.sample(0);assert(!gap.steeringValid&&!gap.throttleValid);
        steeringInput_.cancelCalibration();
        seedControls();metricsPlace(DashboardWidgetKind::ControlHistory,2,2);
        metricsRender(metricsTelemetry);const uint32_t gapNow=millis()+300;
        widgetMetrics_.get()->controls.update(1,1,gapNow,false,false,gapNow);
        updateControlHistoryWidget(0,gapNow);lv_refr_now(nullptr);
        assert(controlsArray(0,0)[100]==LV_CHART_POINT_NONE&&controlsArray(0,1)[100]==LV_CHART_POINT_NONE);
        assert(strstr(lv_label_get_text(widgetViews_[0].detail),"gap"));metricsFit(0);metricsShot("controls-gap");
        // A retained explicit break must survive even adjacent projected bins.
        // Force the collector's public sample contract at this boundary so the
        // renderer cannot silently depend only on an empty intermediate slot.
        auto &breakHistory=widgetMetrics_.get()->controls;breakHistory.reset();
        const uint32_t breakNow=millis();
        breakHistory.update(-750,400,breakNow-100,true,true,breakNow-100);
        breakHistory.update(500,-500,breakNow,true,true,breakNow);
        breakHistory.update(100,100,breakNow-1,true,true,breakNow); // rejected old source
        assert(breakHistory.count()==2);
        const_cast<ControlHistory::Sample &>(breakHistory.sample(1)).breakBefore=true;
        updateControlHistoryWidget(0,breakNow);lv_refr_now(nullptr);
        assert(controlsArray(0,0)[99]==LV_CHART_POINT_NONE&&controlsArray(0,1)[99]==LV_CHART_POINT_NONE);
        assert(controlsArray(0,0)[100]==500&&controlsArray(0,1)[100]==-500);
        metricsChurn.created=0;auditRedrawnPixels=0;
        for(unsigned n=0;n<20;++n){updateControlHistoryWidget(0,breakNow);lv_refr_now(nullptr);}
        assert(auditRedrawnPixels==0&&metricsChurn.created==0);
        // Exercise a complete rolling window repeatedly: chart objects,
        // external arrays and the cold owner remain fixed on collector ticks.
        widgetMetrics_.get()->controls.reset();const auto metricsOwner=widgetMetrics_.get();
        const auto metricsPlot=widgetViews_[0].plot;
        const auto steeringSeries=widgetMetrics_.get()->controlSeries[0][0];
        const auto throttleSeries=widgetMetrics_.get()->controlSeries[0][1];
        const uint32_t tickBegin=millis();
        mixingLiveSamples_.values[0]=-750;mixingLiveSamples_.values[1]=400;
        auto collectTick=[&](uint32_t now){
            mixingLiveUpdatedMs_=now;updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,now);
            updateControlHistoryWidget(0,now);lv_refr_now(nullptr);
        };
        collectTick(tickBegin);metricsChurn.created=0;
        lv_mem_monitor_t tickBefore;lv_mem_monitor(&tickBefore);
        for(unsigned n=1;n<=220;++n)collectTick(tickBegin+n*100);
        lv_mem_monitor_t tickAfter;lv_mem_monitor(&tickAfter);
        assert(widgetMetrics_.get()==metricsOwner&&widgetViews_[0].plot==metricsPlot);
        assert(widgetMetrics_.get()->controlSeries[0][0]==steeringSeries&&widgetMetrics_.get()->controlSeries[0][1]==throttleSeries);
        assert(widgetMetrics_.get()->controls.count()==101&&metricsChurn.created==0);
        assert(tickBefore.used_cnt==tickAfter.used_cnt&&tickBefore.free_size==tickAfter.free_size);

        // Reset is a local confirmed view action: Cancel retains totals, and
        // confirmation neither changes output configuration nor writes sensors.
        seedPower(millis());armRequested_=true;designSafety_=OutputSafetyStatus::Armed;
        metricsPlace(DashboardWidgetKind::PowerEnergy,2,2);metricsRender(metricsTelemetry);
        const auto configuration=controlConfigurationSnapshot();const auto ers=ersConfig_;
        const auto sequence=ersSequence_;const auto resetCuts=auditCutRequests;
        SensorStatistics savedSensorStatistics[DashboardSensorCount];
        if(telemetryHistory_)memcpy(savedSensorStatistics,sensorStatistics_.data(),sizeof(savedSensorStatistics));
        const bool savedSettingsPending=settingsSavePending_,savedChannelsPending=channelParameterSavePending_;
        const auto beforeReset=widgetMetrics_.get()->power.result(millis());
        assert(metricsTap(154)&&activePage_==UiPage::WidgetMetricsReset);
        assert(metricsTap(158)&&activePage_==UiPage::Dashboard);
        assert(widgetMetrics_.get()->power.result(millis()).wattHours==beforeReset.wattHours);
        assert(metricsTap(154)&&activePage_==UiPage::WidgetMetricsReset);metricsModalFit();metricsShot("power-reset-confirmation");
        assert(metricsTap(159)&&activePage_==UiPage::Dashboard);
        const auto reset=widgetMetrics_.get()->power.result(millis());assert(reset.wattHours==0);
        updateWidgetMetrics(metricsTelemetry,OutputSafetyStatus::Cut,millis());
        assert(widgetMetrics_.get()->power.result(millis()).wattHours==0); // blocked old packet
        assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==resetCuts);
        assert(equivalentControlConfiguration(configuration,controlConfigurationSnapshot()));
        assert(!memcmp(&ers,&ersConfig_,sizeof(ers))&&ersSequence_==sequence);
        if(telemetryHistory_)assert(!memcmp(savedSensorStatistics,sensorStatistics_.data(),sizeof(savedSensorStatistics)));
        assert(settingsSavePending_==savedSettingsPending&&channelParameterSavePending_==savedChannelsPending);
        armRequested_=false;designSafety_=OutputSafetyStatus::Cut;
        // Deliberately synthetic, location-free dashboard for visual review.
        seedControls();seedPower(millis());dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
        DashboardWidget previewPower;previewPower.kind=DashboardWidgetKind::PowerEnergy;previewPower.width=2;
        DashboardWidget previewControl;previewControl.kind=DashboardWidgetKind::ControlHistory;
        previewControl.width=2;previewControl.style=DashboardWidgetStyle::Graph;
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,previewPower);
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],2,previewControl);
        drawDashboard();metricsRender(metricsTelemetry);metricsFit(0);metricsFit(2);metricsShot("combined-dashboard");

        // Mix compact cards so the graph, bars and dials share a real dashboard.
        // At the narrowest width both channel readings and both rail values fit.
        auto &layout=dashboardLayouts_[activeModelSlot_];layout=DashboardLayout{};
        DashboardWidget dials;
        dials.kind=DashboardWidgetKind::Power;dials.style=DashboardWidgetStyle::Dials;
        DashboardWidget bars;
        bars.kind=DashboardWidgetKind::Channels;bars.style=DashboardWidgetStyle::Bars;
        bars.source=0;bars.secondSource=1;
        DashboardWidget graph=bars;graph.style=DashboardWidgetStyle::Graph;
        DashboardWidget speedRpm;
        speedRpm.kind=DashboardWidgetKind::SpeedRpm;speedRpm.style=DashboardWidgetStyle::Dials;
        placeDashboardWidget(layout,0,speedRpm);
        placeDashboardWidget(layout,1,dials);
        placeDashboardWidget(layout,2,graph);
        placeDashboardWidget(layout,3,bars);
        metricsTelemetry.rpmValid=true;metricsTelemetry.rpm=36700;
        metricsTelemetry.rpmUpdatedMs=metricsTelemetry.gpsSpeedUpdatedMs=millis();
        const auto negative=crsfValueFromPermille(-1000),positive=crsfValueFromPermille(1000);
        designRequested_[0]=negative;designRequested_[1]=positive;
        channelHistoryCount_=30;
        if(telemetryHistory_){
            for(int n=0;n<30;++n){
                const int value=n<15?-1000+n*130:1000-(n-15)*130;
                channelHistory_[0][n]=crsfValueFromPermille(value);
                channelHistory_[1][n]=crsfValueFromPermille(-value);
            }
        }
        drawDashboard();metricsRender(metricsTelemetry);
        for(uint8_t cell:{1,2,3})metricsFit(cell);
        assert(widgetViews_[0].arcs[0]&&widgetViews_[0].arcs[1]);
        assert(!strcmp(lv_label_get_text(widgetViews_[1].values[0]),"9.9"));
        assert(!strcmp(lv_label_get_text(widgetViews_[1].values[1]),"6.2"));
        assert(widgetViews_[2].plot&&widgetViews_[2].secondPlot);
        if(telemetryHistory_)assert(widgetViews_[2].points[0].y>widgetViews_[2].secondPoints[0].y);
        for(uint8_t cell:{2,3}){
            assert(!strcmp(lv_label_get_text(widgetViews_[cell].values[0]),"-100%"));
            assert(!strcmp(lv_label_get_text(widgetViews_[cell].values[1]),"+100%"));
        }
        metricsShot("instruments");
        assert(requestDisplayRotation());applyDisplayRotation();
    }
}
// Persist both appended kinds together, preserving style, spans and other models.
armRequested_=false;dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
DashboardWidget metricsPower;metricsPower.kind=DashboardWidgetKind::PowerEnergy;metricsPower.width=2;
DashboardWidget metricsControls;metricsControls.kind=DashboardWidgetKind::ControlHistory;metricsControls.width=2;metricsControls.style=DashboardWidgetStyle::Graph;
placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,metricsPower);
placeDashboardWidget(dashboardLayouts_[activeModelSlot_],2,metricsControls);
const auto metricsSaved=dashboardLayouts_[activeModelSlot_];
const uint8_t metricsOtherSlot=(activeModelSlot_+1)%kModelMemoryCount;
const auto metricsOther=dashboardLayouts_[metricsOtherSlot];
saveModelMemorySettings();dashboardLayouts_[activeModelSlot_]=DashboardLayout{};loadModelMemorySettings();
assert(!memcmp(&metricsSaved,&dashboardLayouts_[activeModelSlot_],sizeof(metricsSaved)));
assert(!memcmp(&metricsOther,&dashboardLayouts_[metricsOtherSlot],sizeof(metricsOther)));
assert(!armRequested_&&!safetyResetRequested_&&auditCutRequests==metricsCuts);
dashboardLayouts_[activeModelSlot_]=metricsOriginal;
if(!metricsUnavailable){
    // A real model switch owns its deliberate CUT; neither session totals nor
    // a still-fresh input packet from the previous model may cross that edge.
    const uint8_t previousSlot=activeModelSlot_;const uint32_t beforeSwitch=millis();
    auto &metrics=*widgetMetrics_.get();metrics.power.reset();metrics.controls.reset();
    metrics.power.update(160,100,beforeSwitch,true,600,beforeSwitch,true,beforeSwitch);
    metrics.controls.update(900,-900,beforeSwitch,true,true,beforeSwitch);
    designTelemetry_.ersBatteryUpdatedMs=designTelemetry_.gpsSpeedUpdatedMs=beforeSwitch;
    latestRawAdc_=triggerInput_.centre();assert(selectModelMemory(metricsOtherSlot));
    assert(!armRequested_&&safetyResetRequested_&&auditCutRequests==metricsCuts+1);
    assert(metrics.controls.count()==0&&!metrics.power.result(millis()).energyValid);
    assert(metrics.controlSessionGuard);
    mixingLiveUpdatedMs_=metrics.controlSessionAfterMs-1;
    updateWidgetMetrics(designTelemetry_,OutputSafetyStatus::Cut,metrics.controlSessionAfterMs);
    assert(metrics.controls.count()==0&&metrics.controlSessionGuard);
    mixingLiveUpdatedMs_=metrics.controlSessionAfterMs;
    updateWidgetMetrics(designTelemetry_,OutputSafetyStatus::Cut,metrics.controlSessionAfterMs);
    assert(metrics.controls.count()==0&&metrics.controlSessionGuard); // same-ms old publication
    const uint32_t newSample=metrics.controlSessionAfterMs+1;
    mixingLiveUpdatedMs_=newSample;mixingLiveSamples_.values[0]=250;mixingLiveSamples_.values[1]=-250;mixingLiveSamples_.valid=3;
    designTelemetry_.ersBatteryValid=designTelemetry_.gpsSpeedValid=true;
    designTelemetry_.ersVoltageDeciV=100;designTelemetry_.ersCurrentDeciA=10;
    designTelemetry_.ersBatteryUpdatedMs=designTelemetry_.gpsSpeedUpdatedMs=newSample;
    updateWidgetMetrics(designTelemetry_,OutputSafetyStatus::Cut,newSample);
    assert(!metrics.controlSessionGuard&&metrics.controls.count()==1);
    const auto newPower=metrics.power.result(newSample);assert(newPower.peakValid&&newPower.peakWatts==10&&newPower.wattHours==0);
    assert(!memcmp(&metricsOther,&dashboardLayouts_[metricsOtherSlot],sizeof(metricsOther)));
    latestRawAdc_=triggerInput_.centre();assert(selectModelMemory(previousSlot));
    assert(!armRequested_&&safetyResetRequested_&&auditCutRequests==metricsCuts+2);
    const auto restored=metrics.power.result(millis());
    assert(metrics.controls.count()==0&&restored.peakValid&&restored.peakWatts==160&&!restored.powerValid);
}
lv_obj_remove_event_cb(lv_scr_act(),metricsObjectEvent);metricsDisplay->driver->monitor_cb=metricsPriorMonitor;
safetyCutHandler_=metricsCutHandler;
Serial.println("Power/control widgets: actual LVGL spans/maximize, labels, unchanged zero-pixel/no-allocation ticks, raw CUT input/fault gaps, confirmed local reset, cold failure and model persistence; both orientations PASS");
return true;
