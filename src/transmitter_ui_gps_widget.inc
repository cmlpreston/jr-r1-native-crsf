namespace {
lv_obj_t *makeSensorWidgetLabel(lv_obj_t *parent,const char *text,int x,int y,int width,int height,
    UiText::Style style = UiText::Caption,uint32_t ink = cDim,lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
{
    auto label = makeLabel(parent,text,x,y,style,ink);
    lv_obj_set_size(label,width,height);
    lv_obj_set_style_text_align(label,align,0);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    return label;
}

void setSensorWidgetTitle(lv_obj_t *label,const char *title,int width)
{
    const auto font = UiText::font(UiText::Secondary);
    width = min(width,int(lv_obj_get_style_width(label,0)));
    uiSetTextFont(label,lv_txt_get_width(title,strlen(title),font,0,LV_TEXT_FLAG_NONE)<=width?
        font:UiText::font(UiText::Caption),0);
    boundedLabel(label,width);
    uiSetText(label,title);
}

void setSensorWidgetReading(lv_obj_t *label,const char *reading,bool available)
{
    uiSetText(label,reading);
    uiSetTextFont(label,drivePackReadingFont(reading,lv_obj_get_style_width(label,0),
        lv_obj_get_style_height(label,0)),0);
    uiSetTextColor(label,colour(available?cWhite:cDim),0);
}

void makeSensorDialLabels(lv_obj_t *parent,int x,int y,int diameter,const char *unit,
    lv_obj_t *&reading,lv_obj_t *&unitLabel)
{
    const int inset = UiLayout::SmallGap;
    const int caption = UiText::font(UiText::Caption)->line_height;
    const int valueHeight = max(caption,min<int>(UiText::font(UiText::Reading)->line_height,diameter/3));
    const int valueY = y+(diameter-valueHeight-caption)/2;
    reading = makeSensorWidgetLabel(parent,"--",x+inset,valueY,diameter-2*inset,valueHeight,
        UiText::Caption,cWhite,LV_TEXT_ALIGN_CENTER);
    unitLabel = makeSensorWidgetLabel(parent,unit,x+inset,valueY+valueHeight,
        diameter-2*inset,caption,UiText::Caption,cDim,LV_TEXT_ALIGN_CENTER);
}
}

// The Map widget uses this same speed card when position data is unavailable.
void TransmitterUi::drawDashboardSensorWidget(uint8_t cell,const DashboardWidget &w,
    int x,int y,int width,int height,bool gpsAuto)
{
    if(w.style==DashboardWidgetStyle::Graph&&!gpsAuto){
        drawSensorHistoryWidget(cell,w,width,height);
        return;
    }
    auto &view = widgetViews_[cell];
    auto parent = gpsAuto?view.speedPanel:view.panel;
    const auto &sensor = sensorDescriptors[w.source];
    const int inset = UiLayout::WidgetInset;
    const int bodyY = UiLayout::WidgetBodyY;
    const int caption = UiText::font(UiText::Caption)->line_height;
    const bool speed = w.source==0;
    const bool dial = speed&&w.style==DashboardWidgetStyle::Dials;
    // Keep the side-by-side dial and statistics in either one-row landscape layout.
    const bool compactDial = dial&&height<140;
    const bool narrow = width<180;
    const auto primary = static_cast<uint8_t>(w.speedValue);
    auto title = view.title;
    if(gpsAuto){
        title = view.values[3] = makeLabel(parent,"",inset,UiLayout::WidgetTitleY,UiText::Secondary);
        boundedLabel(title,width-2*inset);
    }
    auto &unit = gpsAuto?view.values[2]:view.detail;
    auto &marker = gpsAuto?view.values[5]:view.marker;

    if(speed){
        constexpr int satellitesWidth = 53;
        setSensorWidgetTitle(title,speedValueTitles[primary],
            width-2*inset-satellitesWidth-UiLayout::SmallGap);
        view.satellites = makeSensorWidgetLabel(parent,"-- sats",width-inset-satellitesWidth,
            UiLayout::WidgetTitleY,satellitesWidth,caption,UiText::Caption,cDim,LV_TEXT_ALIGN_RIGHT);
    } else {
        char text[64];
        snprintf(text,sizeof(text),"%s%s",w.speedValue==SpeedWidgetValue::Maximum?"MAX ":
            w.speedValue==SpeedWidgetValue::Average?"AVG ":"",sensor.name);
        setSensorWidgetTitle(title,text,width-2*inset);
    }

    const int statsHeight = 2*caption;
    int statsX = inset;
    int statsY = height-inset-statsHeight-2;
    int statsWidth = width-2*inset;
    if(narrow) statsY -= UiLayout::WidgetFooterHeight+UiLayout::SmallGap;
    else statsWidth -= 72;

    if(dial){
        const int dialSpace = compactDial?height-bodyY-inset-2:statsY-bodyY-UiLayout::SmallGap;
        const int dialWidth = compactDial?width/2-inset-UiLayout::SmallGap:width-2*inset;
        const int diameter = min(dialSpace,dialWidth);
        const int dialX = compactDial?inset:(width-diameter)/2;
        const int dialY = bodyY+(dialSpace-diameter)/2;
        view.arcs[0] = makeWidgetDial(parent,dialX,dialY,diameter);
        lv_arc_set_range(view.arcs[0],0,1000);
        makeSensorDialLabels(parent,dialX,dialY,diameter,sensor.unit,view.values[0],unit);
        marker = makeSensorWidgetLabel(parent,"0-100",dialX,dialY+diameter-caption,
            diameter,caption,UiText::Caption,cDim,LV_TEXT_ALIGN_CENTER);
        if(compactDial){
            statsX = width/2;
            statsY = bodyY+(dialSpace-statsHeight)/2-UiLayout::SmallGap;
            statsWidth = width-statsX-inset;
        }
    } else {
        const int statusY = statsY-caption-UiLayout::SmallGap;
        const int valueEnd = gpsAuto?statusY:statsY-UiLayout::SmallGap;
        const int valueHeight = min<int>(UiText::font(UiText::Reading)->line_height,valueEnd-bodyY);
        const int valueWidth = width-2*inset-40;
        view.values[0] = makeSensorWidgetLabel(parent,"--",inset,bodyY,valueWidth,valueHeight,
            UiText::Heading,cWhite);
        unit = makeSensorWidgetLabel(parent,sensor.unit,width-inset-36,
            bodyY+(valueHeight-caption)/2,36,caption,UiText::Caption,cDim,LV_TEXT_ALIGN_RIGHT);
        if(gpsAuto){
            view.values[4] = makeSensorWidgetLabel(parent,"Speed data only",inset,statusY,
                width-2*inset,caption);
        }
    }
    view.values[1] = makeSensorWidgetLabel(parent,"Max --\nAvg --",statsX,statsY,statsWidth,statsHeight);
    lv_obj_set_style_text_line_space(view.values[1],0,0);
    const int resetY = height-UiLayout::WidgetSensorFooterHeight;
    auto reset = makePageActionButton(x+width-76,y+resetY,68,UiLayout::WidgetSensorResetHeight,"Reset",
        UiText::Caption,cPanel2,cWhite,cLine,&TransmitterUi::controlDesign,speed&&!gpsAuto?148:250+cell);
    if(gpsAuto){
        view.speedReset = reset;
        view.gpsResetTarget = pageFocusCount_-1;
        lv_obj_set_parent(reset,parent);
        lv_obj_set_pos(reset,width-76,resetY);
    }
}

void TransmitterUi::updateDashboardSensorWidget(uint8_t cell,const DashboardWidget &w,
    const CrsfTelemetryState &t,uint32_t now,bool gpsAuto)
{
    if(w.style==DashboardWidgetStyle::Graph&&!gpsAuto){
        updateSensorHistoryWidget(cell,w,t,now);
        return;
    }
    auto &view = widgetViews_[cell];
    if(view.satellites){
        if(t.gpsSatellitesValid&&uint32_t(now-t.gpsSatellitesUpdatedMs)<=3000)
            uiSetTextFmt(view.satellites,"%u sats",t.satellites);
        else uiSetText(view.satellites,"-- sats");
    }

    const auto reading = sensorReading(t,w.source,now);
    auto stats = telemetryHistory_?sensorStatistics_[w.source].result(sensorSessionStats_[w.source],now):
        SensorStatistics::Result{};
    if(w.source==0&&sensorSessionStats_[0])
        stats = {speedStatistics_.available(),speedStatistics_.maximum(),0,speedStatistics_.average()};

    const int32_t values[] = {reading.value,stats.maximum,stats.average};
    const bool available[] = {reading.valid,stats.valid,stats.valid};
    char text[3][16];
    for(unsigned i=0;i<3;++i){
        if(available[i]) sensorText(w.source,values[i],text[i],sizeof(text[i]));
        else strcpy(text[i],"--");
    }
    const auto primary = static_cast<uint8_t>(w.speedValue);
    setSensorWidgetReading(view.values[0],text[primary],available[primary]);
    if(view.arcs[0]){
        const int peak = max(int(stats.maximum),available[primary]?int(values[primary]):0);
        const int maximum = max(1000,((peak+999)/1000)*1000);
        lv_arc_set_value(view.arcs[0],available[primary]?int(int64_t(values[primary])*1000/maximum):0);
        uiSetArcOpacity(view.arcs[0],available[primary]?LV_OPA_COVER:LV_OPA_TRANSP,LV_PART_INDICATOR);
        uiSetTextFmt(gpsAuto?view.values[5]:view.marker,"0-%d",maximum/10);
    }

    // The remaining readings retain Current / Maximum / Average order.
    uint8_t secondary[2],count=0;
    for(uint8_t i=0;i<3;++i) if(i!=primary) secondary[count++]=i;
    uiSetTextFmt(view.values[1],"%s %s\n%s %s",
        speedValueShortNames[secondary[0]],text[secondary[0]],
        speedValueShortNames[secondary[1]],text[secondary[1]]);
}

void TransmitterUi::updateGpsWidgetPolicy(const CrsfTelemetryState &t,uint32_t now)
{
    GpsWidgetView::Input input;
    input.positionUsable=gpsPositionAvailable(t,now);
    input.positionFieldsFresh=t.gpsValid&&uint32_t(now-t.gpsUpdatedMs)<=3000;
    input.satellitesKnownFresh=t.gpsSatellitesValid&&uint32_t(now-t.gpsSatellitesUpdatedMs)<=3000;
    input.satellites=t.satellites;
    input.speedFresh=gpsSpeedAvailable(t,now);
    input.positionStamp=t.gpsUpdatedMs;
    gpsWidgetPolicy_.update(input,now);
}

void TransmitterUi::updateGpsAutoWidget(uint8_t cell,const CrsfTelemetryState &t,uint32_t now)
{
    auto &view=widgetViews_[cell];
    if(!view.panel||!view.basemap||!view.speedPanel)return;
    updateGpsWidgetPolicy(t,now);
    const auto selection=gpsWidgetPolicy_.result();
    // Keep the meaning of a press fixed through release. A telemetry update
    // must not place a reset action under a finger or disturb an output hold.
    if(view.gpsDisplayed!=selection.view&&!touchWasDown_&&!remoteArmHold_.active(now)){
        view.gpsDisplayed=selection.view;
        const bool speed=selection.view==GpsWidgetView::View::Speed;
        uiSetHidden(view.basemap,speed);uiSetHidden(view.speedPanel,!speed);
        // An initial source change can occur while the new dashboard still
        // awaits its first layout; resolve the explicit panel size before
        // replacing its stable touch/focus rectangle.
        lv_obj_update_layout(view.panel);
        if(view.gpsCardTarget<pageFocusCount_){
            auto &card=pageFocusTargets_[view.gpsCardTarget];
            card.focus.height=card.activate.height=lv_obj_get_height(view.panel)-
                (speed?2+UiLayout::WidgetSensorFooterHeight:0);
        }
        for(auto index:{view.gpsFooterTarget,view.gpsResetTarget})
            if(index<pageFocusCount_)pageFocusTargets_[index].enabled=speed;
        if(!speed&&(pageFocusIndex_==view.gpsFooterTarget||pageFocusIndex_==view.gpsResetTarget))
            pageFocusIndex_=view.gpsCardTarget;
        // Reset now occupies a region that had a different action in the old
        // browser frame. Reject delayed remote taps without rebuilding the UI
        // or changing the independent control-publication/safety epoch.
        SetupPortal::screenChanged();
        refreshPageFocus();
    }
    if(view.gpsDisplayed==GpsWidgetView::View::Speed){
        auto widget=dashboardWidgetForView(cell);
        widget.kind=DashboardWidgetKind::Sensor;widget.source=0;
        widget.style=DashboardWidgetStyle::Numbers;widget.speedValue=SpeedWidgetValue::Current;
        updateDashboardSensorWidget(cell,widget,t,now,true);
        const bool narrow=lv_obj_get_width(view.speedPanel)<180;
        const char *status=selection.status==GpsWidgetView::Status::SpeedOnly?(narrow?"Speed only":"Speed data only"):
            selection.status==GpsWidgetView::Status::Unavailable?"GPS data stale":
            selection.status==GpsWidgetView::Status::Acquiring?"GPS acquiring":
            selection.status==GpsWidgetView::Status::Position?"Position available":"No position";
        uiSetText(view.values[4],status);
        return;
    }
    const bool reported=gpsPositionAvailable(t,now),fix=gpsFixConfirmed(t,now);
    const int width=lv_obj_get_width(view.basemap),height=lv_obj_get_height(view.basemap);
    const bool anyFresh=(t.gpsValid&&uint32_t(now-t.gpsUpdatedMs)<=3000)||
        (t.gpsSatellitesValid&&uint32_t(now-t.gpsSatellitesUpdatedMs)<=3000)||gpsSpeedAvailable(t,now);
    const char *status=reported&&t.satellites==0?"Position unconfirmed":
        selection.status==GpsWidgetView::Status::Position&&gpsCount_?"":
        selection.status==GpsWidgetView::Status::Unavailable?"GPS data stale":
        selection.status==GpsWidgetView::Status::Acquiring?"GPS acquiring":
        selection.status==GpsWidgetView::Status::SpeedOnly?"Speed data only":
        anyFresh?"No position":"Waiting for GPS";
    uiSetText(view.detail,status);uiSetHidden(view.detail,!*status);
    if(!gpsCount_){
        uiSetHidden(view.marker,true);previewGpsBasemap(view.basemap,view.mapViewport,true);return;
    }
    const double scale=max(0.15,min(2.0,min(width-20,height-10)/40.0));
    followGpsBasemap(view.basemap,view.mapViewport,gpsLat_[gpsCount_-1],gpsLon_[gpsCount_-1],scale);
    updateGpsTrail(view.plot,view.points,view.gpsRenderedCount,gpsLat_,gpsLon_,gpsCount_,view.mapViewport,width,height,true,view.gpsProjection,gpsTrailRevision_);
    const auto marker=gpsViewportPoint(view.mapViewport,gpsLat_[gpsCount_-1],gpsLon_[gpsCount_-1],width,height);
    lv_obj_set_pos(view.marker,marker.x-8,marker.y-8);
    uiSetHidden(view.marker,false);uiSetBgColor(view.marker,colour(fix?0x00707d:reported?0xb87800:0x53636c),0);
    uiSetOpacity(view.marker,reported?LV_OPA_COVER:LV_OPA_50);
}
