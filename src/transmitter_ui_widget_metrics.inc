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

void TransmitterUi::resetTripRunMetrics()
{
    if(widgetMetrics_)widgetMetrics_.get()->tripRun.reset(designTelemetry_.ersBatteryUpdatedMs,
                                                        designTelemetry_.gpsSpeedUpdatedMs);
}

void TransmitterUi::resetBatteryLoadMetrics()
{
    if(widgetMetrics_)widgetMetrics_.get()->batteryLoad.reset(designTelemetry_.ersBatteryUpdatedMs,
                                                            designTelemetry_.vehicleCellsUpdatedMs);
}

void TransmitterUi::updateWidgetMetrics(const CrsfTelemetryState &t,OutputSafetyStatus safety,uint32_t now)
{
    if(!widgetMetrics_)return;
    auto &metrics=*widgetMetrics_.get();
    metrics.power.update(t.ersVoltageDeciV,t.ersCurrentDeciA,t.ersBatteryUpdatedMs,t.ersBatteryValid,
                         t.groundSpeedDeciKmh,t.gpsSpeedUpdatedMs,gpsSpeedAvailable(t,now),now);
    metrics.tripRun.update(t,now);
    metrics.batteryLoad.update(t,now);
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

namespace {
const char *sessionMetricCaption(DashboardWidgetKind kind,uint8_t field,bool compact)
{
    const char *trip[]={"Distance","Moving time","Moving average","Top speed"};
    const char *tripShort[]={"Dist","Move","Avg","Max"};
    const char *run[]={"Elapsed time","Distance","Top speed","Peak power seen","Capacity used","Energy used"};
    const char *runShort[]={"Time","Dist","Max","Peak","Used","Energy"};
    const char *battery[]={"Pack / CU01","Current / CU01","Min pack seen","Peak amps seen","Lowest / CV01","Spread / CV01"};
    const char *batteryShort[]={"Pack","Load","Min","Peak","Low","Spread"};
    if(kind==DashboardWidgetKind::TripComputer)return compact?tripShort[field]:trip[field];
    if(kind==DashboardWidgetKind::RunSummary)return compact?runShort[field]:run[field];
    return compact?batteryShort[field]:battery[field];
}
void sessionDuration(char *text,size_t size,uint64_t millis)
{
    const auto seconds=millis/1000;
    if(seconds<3600)snprintf(text,size,"%llu:%02u",static_cast<unsigned long long>(seconds/60),unsigned(seconds%60));
    else if(seconds<360000)snprintf(text,size,"%llu:%02u:%02u",static_cast<unsigned long long>(seconds/3600),unsigned(seconds/60%60),unsigned(seconds%60));
    else snprintf(text,size,"%llud %02uh",static_cast<unsigned long long>(seconds/86400),unsigned(seconds/3600%24));
}
void sessionDistance(char *text,size_t size,double kilometres,bool complete)
{
    const char *mark=complete?"":"~";
    if(kilometres<1)snprintf(text,size,"%s%.0f m",mark,kilometres*1000);
    else if(kilometres<1000)snprintf(text,size,"%s%.2f km",mark,kilometres);
    else if(kilometres<100000)snprintf(text,size,"%s%.0f km",mark,kilometres);
    else snprintf(text,size,"%s%.1e km",mark,kilometres);
}
}

void TransmitterUi::drawTripRunBatteryWidget(uint8_t cell,int width,int height)
{
    auto &view=widgetViews_[cell];
    const auto kind=dashboardWidgetForView(cell).kind;
    if(kind==DashboardWidgetKind::BatteryLoadBalance)uiSetText(view.title,width<200?"Load / balance":"Battery load / balance");
    const unsigned fields=kind==DashboardWidgetKind::TripComputer?4:6;
    const int columns=width>=220?2:1,rows=(fields+columns-1)/columns;
    const int cw=(width-16)/columns,rh=(height-46)/rows;
    const int ch=UiText::font(UiText::Caption)->line_height;
    const bool compact=rh<ch*2+2;
    for(uint8_t n=0;n<fields;++n){
        const int xx=8+(n%columns)*cw,yy=23+(n/columns)*rh;
        if(!compact){
            view.bars[n]=makeLabel(view.panel,sessionMetricCaption(kind,n,false),xx,yy,UiText::Caption,cDim);
            boundedLabel(view.bars[n],cw-6);
        }
        view.values[n]=makeLabel(view.panel,"--",xx,yy+(compact?0:ch),UiText::Caption,cWhite);
        lv_obj_set_size(view.values[n],cw-6,compact?rh:max(ch,rh-ch));
        lv_label_set_long_mode(view.values[n],LV_LABEL_LONG_CLIP);
    }
    view.detail=makeLabel(view.panel,widgetMetrics_?"Since reset":"Unavailable",8,height-17,UiText::Caption,cDim);
    boundedLabel(view.detail,width-76);
}

void TransmitterUi::updateTripRunBatteryWidget(uint8_t cell,uint32_t now)
{
    auto &view=widgetViews_[cell];
    const auto kind=dashboardWidgetForView(cell).kind;
    if(!dashboardSessionWidget(kind))return;
    lv_obj_update_layout(view.panel);
    const unsigned fields=kind==DashboardWidgetKind::TripComputer?4:6;
    char text[6][32];for(auto &value:text)snprintf(value,sizeof(value),"--");
    const bool narrow=lv_obj_get_width(view.panel)<200;
    const char *status="Unavailable";
    if(widgetMetrics_&&kind==DashboardWidgetKind::BatteryLoadBalance){
        const auto r=widgetMetrics_.get()->batteryLoad.result(now);
        if(r.packFresh){snprintf(text[0],sizeof(text[0]),"%.1f V",r.voltageDeciV/10.0);snprintf(text[1],sizeof(text[1]),"%.1f A",r.currentDeciA/10.0);}
        if(r.packExtremaValid){snprintf(text[2],sizeof(text[2]),"%.1f V",r.minimumVoltageDeciV/10.0);snprintf(text[3],sizeof(text[3]),"%.1f A",r.peakCurrentDeciA/10.0);}
        if(r.cellsFresh){snprintf(text[4],sizeof(text[4]),"%.3f V",r.lowestCellMv/1000.0);snprintf(text[5],sizeof(text[5]),"%u mV",r.spreadMv);}
        status=r.packFresh&&r.cellsFresh?"Since reset":r.packFresh?(narrow?"No CV01":"CV01 missing"):r.cellsFresh?(narrow?"No CU01":"CU01 missing"):
            r.packExtremaValid||r.cellExtremaValid?(narrow?"Stale":"Data stale"):"Waiting";
    }else if(widgetMetrics_){
        const auto r=widgetMetrics_.get()->tripRun.result(now);
        if(kind==DashboardWidgetKind::TripComputer){
            if(r.distanceValid)sessionDistance(text[0],sizeof(text[0]),r.kilometres,r.distanceComplete);
            if(r.movingValid){char duration[24];sessionDuration(duration,sizeof(duration),r.movingMs);snprintf(text[1],sizeof(text[1]),"%s%s",r.movingComplete?"":"~",duration);}
            if(r.averageMovingValid)snprintf(text[2],sizeof(text[2]),"%s%.1f km/h",r.distanceComplete&&r.movingComplete?"":"~",r.averageMovingKmh);
            if(r.maxSpeedValid)snprintf(text[3],sizeof(text[3]),"%.1f km/h",r.maxSpeedKmh);
            status=!r.distanceValid?"GPS waiting":!r.speedFresh?"GPS stale":!r.distanceComplete||!r.movingComplete?"GPS gaps":"Since reset";
        }else{
            if(r.elapsedValid)sessionDuration(text[0],sizeof(text[0]),r.elapsedMs);
            if(r.distanceValid)sessionDistance(text[1],sizeof(text[1]),r.kilometres,r.distanceComplete);
            if(r.maxSpeedValid)snprintf(text[2],sizeof(text[2]),"%.1f km/h",r.maxSpeedKmh);
            if(r.powerValid){
                if(r.peakWatts>=1000000)snprintf(text[3],sizeof(text[3]),"%.1f MW",r.peakWatts/1000000);
                else if(r.peakWatts>=10000)snprintf(text[3],sizeof(text[3]),"%.1f kW",r.peakWatts/1000);
                else snprintf(text[3],sizeof(text[3]),"%.0f W",r.peakWatts);
            }
            if(r.usedCapacityValid){
                if(r.usedMah<10000)snprintf(text[4],sizeof(text[4]),"%s%llu mAh",r.usedCapacityComplete?"":"~",static_cast<unsigned long long>(r.usedMah));
                else snprintf(text[4],sizeof(text[4]),"%s%.1f Ah",r.usedCapacityComplete?"":"~",r.usedMah/1000.0);
            }
            if(r.energyValid)snprintf(text[5],sizeof(text[5]),"%s%.2f Wh",r.energyComplete?"":"~",r.wattHours);
            status=!r.elapsedValid?"Waiting":!r.distanceValid?(narrow?"No GPS":"GPS missing"):!r.energyValid?(narrow?"No CU01":"CU01 missing"):
                !r.distanceComplete||!r.energyComplete||!r.usedCapacityComplete?(narrow?"Gaps ~":"Totals ~ / gaps"):"Since reset";
        }
    }
    for(uint8_t n=0;n<fields;++n){
        char combined[64];const char *value=text[n];
        if(!view.bars[n]){
            snprintf(combined,sizeof(combined),"%s %s",sessionMetricCaption(kind,n,true),text[n]);value=combined;
        }
        uiSetText(view.values[n],value);
        uiSetTextFont(view.values[n],drivePackReadingFont(value,lv_obj_get_width(view.values[n]),lv_obj_get_height(view.values[n])),0);
    }
    uiSetText(view.detail,status);
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
