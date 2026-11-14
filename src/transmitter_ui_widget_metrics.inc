// UI-task session statistics only: no sensor commands, persistence, or changes
// to the control/RF path. Chart backing arrays are allocated once in cold RAM.
static_assert(ControlHistory::Capacity==ControlHistory::WindowMs/ControlHistory::PeriodMs+1,
              "Control chart points must cover the fixed trailing time window");
bool TransmitterUi::initializeWidgetMetrics()
{
#ifdef JR_R1_SIMULATOR
    const bool available=!getenv("JR_R1_SIM_WIDGET_METRICS_ALLOC_FAIL")&&widgetMetrics_.initialize();
#else
    const bool available=widgetMetrics_.initialize();
#endif
    if(!available)Serial.println("UI MEMORY: widget session metrics unavailable");
    return available;
}

void TransmitterUi::resetWidgetPowerMetrics()
{
    if(!widgetMetrics_)return;
    // Await a new CU01/GPS packet after reset; a cached stale reading must not
    // silently become the baseline of a new energy/distance session.
    widgetMetrics_.get()->power.reset(designTelemetry_.ersBatteryUpdatedMs,
                                      designTelemetry_.gpsSpeedUpdatedMs);
}

void TransmitterUi::updateWidgetMetrics(const CrsfTelemetryState &t,OutputSafetyStatus safety,uint32_t now)
{
    if(!widgetMetrics_)return;
    auto &metrics=*widgetMetrics_.get();
    metrics.power.update(t.ersVoltageDeciV,t.ersCurrentDeciA,t.ersBatteryUpdatedMs,t.ersBatteryValid,
                         t.groundSpeedDeciKmh,t.gpsSpeedUpdatedMs,gpsSpeedAvailable(t,now),now);
    if(metrics.controlSessionGuard){
        if(int32_t(mixingLiveUpdatedMs_-metrics.controlSessionAfterMs)<=0)return;
        metrics.controlSessionGuard=false;
    }
    const bool fault=hardwareRecovery_||calibrationActive()||
        safety==OutputSafetyStatus::Calibration||safety==OutputSafetyStatus::InputFault||
        safety==OutputSafetyStatus::ControlStale||safety==OutputSafetyStatus::ConfigurationFault||
        safety==OutputSafetyStatus::ChannelDisabled;
    const bool steeringValid=!fault&&channelRoleSettings_.steeringEnabled&&
        steeringInputProfile().valid()&&mixInputValid(mixingLiveSamples_,0);
    const bool throttleValid=!fault&&channelRoleSettings_.throttleEnabled&&
        throttleInputProfile().valid()&&mixInputValid(mixingLiveSamples_,1);
    // CUT holds outputs, not driver inputs. Values are the calibrated raw
    // role inputs, before expo, trim, reversal, mixing or the output resolver.
    metrics.controls.update(mixingLiveSamples_.values[0],mixingLiveSamples_.values[1],
                            mixingLiveUpdatedMs_,steeringValid,throttleValid,now);
}

void TransmitterUi::updateControlHistoryWidget(uint8_t cell,uint32_t now)
{
    auto &view=widgetViews_[cell];
    if(!widgetMetrics_){
        uiSetText(view.detail,"Metrics unavailable");return;
    }
    auto &metrics=*widgetMetrics_.get();
    const auto &history=metrics.controls;
    const unsigned count=history.count();
    const auto *latest=count?&history.sample(count-1):nullptr;
    const bool fresh=latest&&uint32_t(now-latest->stampMs)<=ControlHistory::FreshnessMs;
    const bool steeringValid=fresh&&latest->steeringValid,throttleValid=fresh&&latest->throttleValid;
    if(steeringValid)uiSetTextFmt(view.values[0],"ST %+.1f%%",latest->steering/10.0);
    else uiSetText(view.values[0],"ST --");
    if(throttleValid)uiSetTextFmt(view.values[1],"TH %+.1f%%",latest->throttle/10.0);
    else uiSetText(view.values[1],"TH --");
    uiSetText(view.detail,!steeringValid||!throttleValid?"INPUTS / gap":
        designSafety_==OutputSafetyStatus::Armed?"INPUTS / live":"INPUTS / CUT");
    if(!view.plot)return;
    bool changed=false;
    // A fixed 100 ms time grid keeps the horizontal scale at ten seconds.
    // Missing samples remain POINT_NONE, so LVGL never joins across gaps.
    // Compare in place: unchanged flat histories do not invalidate the chart.
    unsigned sampleIndex=0;
    for(unsigned point=0;point<ControlHistory::Capacity;++point){
        lv_coord_t steering=LV_CHART_POINT_NONE,throttle=LV_CHART_POINT_NONE;
        while(sampleIndex<count){
            const auto &sample=history.sample(sampleIndex);
            const uint32_t age=now-sample.stampMs;
            if(age>ControlHistory::WindowMs){++sampleIndex;continue;}
            const unsigned slot=ControlHistory::Capacity-1-age/ControlHistory::PeriodMs;
            if(slot>point)break;
            ++sampleIndex;
            if(slot!=point)continue;
            if(sample.steeringValid)steering=sample.steering;
            if(sample.throttleValid)throttle=sample.throttle;
        }
        // A rejected timestamp can break continuity even when the accepted
        // samples land in adjacent bins. Look ahead before comparing/writing
        // the current bin, preserving the new point without invalidation churn
        // from clearing and restoring the same cached predecessor every tick.
        if(sampleIndex<count){
            const auto &next=history.sample(sampleIndex);
            const uint32_t age=now-next.stampMs;
            if(next.breakBefore&&age<=ControlHistory::WindowMs&&
               ControlHistory::Capacity-1-age/ControlHistory::PeriodMs==point+1)
                steering=throttle=LV_CHART_POINT_NONE;
        }
        auto &st=metrics.controlValues[cell][0][point];
        auto &th=metrics.controlValues[cell][1][point];
        changed|=st!=steering||th!=throttle;st=steering;th=throttle;
    }
    if(changed)lv_chart_refresh(view.plot);
}
