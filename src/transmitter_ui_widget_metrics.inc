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
struct MetricCaption {
    const char *full;
    const char *shortName;
};

constexpr MetricCaption tripCaptions[] = {
    {"Distance", "Dist"}, {"Moving time", "Move"},
    {"Moving average", "Avg"}, {"Top speed", "Max"},
};
constexpr MetricCaption runCaptions[] = {
    {"Elapsed time", "Time"}, {"Distance", "Dist"}, {"Top speed", "Max"},
    {"Peak power seen", "Peak"}, {"Capacity used", "Used"}, {"Energy used", "Wh"},
};
constexpr MetricCaption balanceCaptions[] = {
    {"Pack / CU01", "Pack"}, {"Current / CU01", "Load"}, {"Min pack seen", "Min"},
    {"Peak amps seen", "Peak"}, {"Lowest / CV01", "Low"}, {"Spread / CV01", "Delta"},
};
constexpr MetricCaption powerCaptions[] = {
    {"Input power", "Power"}, {"Peak seen", "Peak"},
    {"Energy used", "Wh"}, {"Est. Wh/km", "Wh/km"},
};
constexpr MetricCaption packCaptions[] = {
    {"Pack voltage", "Volts"}, {"Current", "Amps"},
    {"Used capacity", "Used"}, {"Est. remaining", "Left"},
};

void setMetricReading(lv_obj_t *label,const char *text)
{
    if (strcmp(lv_label_get_text(label),text)==0) return;
    lv_label_set_text(label,text);
    uiSetTextColor(label,colour(strcmp(text,"--")==0?cDim:cWhite),0);
    uiSetTextFont(label,drivePackReadingFont(text,lv_obj_get_style_width(label,0),
                                          lv_obj_get_style_height(label,0)),0);
}

void formatMetricQuantity(char (&text)[32],lv_obj_t *label,double quantity,int decimals,
                          const char *unit,bool complete)
{
    const char *mark = complete?"":"~";
    snprintf(text,sizeof(text),"%s%.*f %s",mark,decimals,quantity,unit);
    // Long-running totals can outgrow a single card even in the caption font.
    if (lv_txt_get_width(text,strlen(text),UiText::font(UiText::Caption),0,LV_TEXT_FLAG_NONE)
        > lv_obj_get_style_width(label,0)) {
        snprintf(text,sizeof(text),"%s%.1e %s",mark,quantity,unit);
    }
}
void sessionDuration(char *text,size_t size,uint64_t millis)
{
    const auto seconds=millis/1000;
    if(seconds<3600)snprintf(text,size,"%llu:%02u",static_cast<unsigned long long>(seconds/60),unsigned(seconds%60));
    else if(seconds<360000)snprintf(text,size,"%llu:%02u:%02u",static_cast<unsigned long long>(seconds/3600),unsigned(seconds/60%60),unsigned(seconds%60));
    else if(seconds>=864000000)snprintf(text,size,"%.1e d",seconds/86400.0);
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

void TransmitterUi::drawMetricWidget(uint8_t cell,int width,int height)
{
    auto &view = widgetViews_[cell];
    const auto kind = dashboardWidgetForView(cell).kind;
    const int titleWidth = lv_obj_get_style_width(view.title,0);
    const MetricCaption *captions;
    int fields = 4;
    switch (kind) {
        case DashboardWidgetKind::TripComputer:
            captions = tripCaptions;
            break;
        case DashboardWidgetKind::RunSummary:
            captions = runCaptions;
            fields = 6;
            break;
        case DashboardWidgetKind::BatteryLoadBalance:
            captions = balanceCaptions;
            fields = 6;
            uiSetText(view.title,titleWidth<150?"Load / balance":"Battery load / balance");
            break;
        case DashboardWidgetKind::DrivePack:
            captions = packCaptions;
            uiSetText(view.title,"Drive pack / CU01");
            break;
        case DashboardWidgetKind::PowerEnergy:
            captions = powerCaptions;
            uiSetText(view.title,titleWidth<148?"CU01 power / Wh":"Power / energy / CU01");
            break;
        default:
            return;
    }

    const int columns = width<216?1:width>=400&&fields==6?3:2;
    const int rows = (fields+columns-1)/columns;
    const int inset = UiLayout::WidgetInset;
    const int columnWidth = (width-2*inset-(columns-1)*UiLayout::Gap)/columns;
    const int footerY = height-UiLayout::WidgetFooterHeight;
    const int rowHeight = (footerY-UiLayout::SmallGap-UiLayout::WidgetBodyY)/rows;
    const auto *captionFont = UiText::font(UiText::Caption);
    const int captionHeight = captionFont->line_height;
    const bool inlineReading = rowHeight<2*captionHeight+UiLayout::SmallGap;

    for (int n=0;n<fields;++n) {
        const int x = inset+(n%columns)*(columnWidth+UiLayout::Gap);
        const int y = UiLayout::WidgetBodyY+(n/columns)*rowHeight;
        const char *caption = inlineReading?captions[n].shortName:captions[n].full;
        const int captionWidth = inlineReading
            ? lv_txt_get_width(caption,strlen(caption),captionFont,0,LV_TEXT_FLAG_NONE)
            : columnWidth;
        const int captionY = inlineReading?y+(rowHeight-captionHeight)/2:y;
        boundedLabel(makeLabel(view.panel,caption,x,captionY,UiText::Caption,cDim),captionWidth);

        // Inline rows reserve width for the caption. Stacked tiles use all
        // the column's width. Both keep the reading anchored to its right edge.
        const int readX = inlineReading?captionWidth+UiLayout::SmallGap:0;
        const int readY = inlineReading?0:captionHeight+UiLayout::SmallGap;
        const int readHeight = inlineReading
            ? min(rowHeight,int(UiText::font(UiText::Menu)->line_height))
            : rowHeight-readY;
        const int offset = inlineReading?(rowHeight-readHeight)/2:readY;
        auto *value = makeLabel(view.panel,"--",x+readX,y+offset,UiText::Caption,cDim);
        lv_obj_set_size(value,columnWidth-readX,readHeight);
        lv_label_set_long_mode(value,LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(value,LV_TEXT_ALIGN_RIGHT,0);
        uiSetTextFont(value,drivePackReadingFont("--",columnWidth-readX,readHeight),0);
        view.values[n] = value;
    }
    const int footerTextY = footerY+(UiLayout::WidgetFooterHeight-captionHeight)/2;
    view.detail = makeLabel(view.panel,"",inset,footerTextY,UiText::Caption,cDim);
    boundedLabel(view.detail,width-2*inset-(dashboardMetricsResettable(kind)?60:0));
}

void TransmitterUi::updateTripRunBatteryWidget(uint8_t cell,uint32_t now)
{
    auto &view=widgetViews_[cell];
    const auto kind=dashboardWidgetForView(cell).kind;
    if(!dashboardSessionWidget(kind))return;
    const unsigned fields=kind==DashboardWidgetKind::TripComputer?4:6;
    char text[6][32];for(auto &value:text)snprintf(value,sizeof(value),"--");
    const bool narrow=lv_obj_get_style_width(view.panel,0)<200;
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
                else formatMetricQuantity(text[4],view.values[4],r.usedMah/1000.0,1,"Ah",r.usedCapacityComplete);
            }
            if(r.energyValid)formatMetricQuantity(text[5],view.values[5],r.wattHours,2,"Wh",r.energyComplete);
            status=!r.elapsedValid?"Waiting":!r.distanceValid?(narrow?"No GPS":"GPS missing"):!r.energyValid?(narrow?"No CU01":"CU01 missing"):
                !r.distanceComplete||!r.energyComplete||!r.usedCapacityComplete?(narrow?"Gaps ~":"Totals ~ / gaps"):"Since reset";
        }
    }
    for(uint8_t n=0;n<fields;++n)setMetricReading(view.values[n],text[n]);
    uiSetText(view.detail,status);
}

void TransmitterUi::updateMetricWidget(uint8_t cell,const CrsfTelemetryState &telemetry,uint32_t now)
{
    const auto kind = dashboardWidgetForView(cell).kind;
    if (dashboardSessionWidget(kind)) {
        updateTripRunBatteryWidget(cell,now);
        return;
    }

    auto &view = widgetViews_[cell];
    char text[4][32] = {"--","--","--","--"};
    const char *status;
    if (kind==DashboardWidgetKind::DrivePack) {
        const auto reading = drivePackWidgetReading(telemetry,now);
        if (reading.fresh) {
            snprintf(text[0],sizeof(text[0]),"%u.%u V",reading.voltageDeciV/10,reading.voltageDeciV%10);
            snprintf(text[1],sizeof(text[1]),"%u.%u A",reading.currentDeciA/10,reading.currentDeciA%10);
            snprintf(text[2],sizeof(text[2]),"%lu mAh",static_cast<unsigned long>(reading.usedMah));
            if (reading.remainingValid) snprintf(text[3],sizeof(text[3]),"%u%%",reading.remainingPercent);
        }
        status = reading.fresh?(reading.remainingValid?"Live":"Remaining unknown")
            :telemetry.ersBatteryUpdatedMs?"CU01 stale":"CU01 waiting";
    } else {
        const auto reading = widgetMetrics_?widgetMetrics_.get()->power.result(now):PowerEnergyStatistics::Result{};
        if (reading.powerValid) snprintf(text[0],sizeof(text[0]),"%.1f W",reading.watts);
        if (reading.peakValid) snprintf(text[1],sizeof(text[1]),"%.1f W",reading.peakWatts);
        if (reading.energyValid) formatMetricQuantity(text[2],view.values[2],reading.wattHours,2,"Wh",reading.energyComplete);
        if (reading.efficiencyValid) formatMetricQuantity(text[3],view.values[3],reading.wattHoursPerKm,1,"Wh/km",reading.efficiencyComplete);
        const bool narrow = lv_obj_get_style_width(view.panel,0)<200;
        status = !widgetMetrics_?(narrow?"Unavailable":"Metrics unavailable"):
            !reading.energyValid?(narrow?"CU01 wait":"CU01 / waiting"):
            !reading.energyComplete?(narrow?"Wh gaps":"CU01 / gaps"):
            !reading.efficiencyValid?(narrow?"GPS/100m":"Wh/km needs GPS/100m"):
            !reading.efficiencyComplete?(narrow?"Wh/km gap":"Wh/km / gaps"):"Since reset";
    }
    for (uint8_t n=0;n<4;++n) setMetricReading(view.values[n],text[n]);
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
