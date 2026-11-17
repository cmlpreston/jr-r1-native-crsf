// Shared sensor subview: the Map widget uses the same speed labels, units,
// statistics and reset action without changing its persisted kind or layout.
void TransmitterUi::drawDashboardSensorWidget(uint8_t cell,const DashboardWidget &w,
    int x,int y,int width,int height,bool gpsAuto)
{
    auto &view=widgetViews_[cell];
    if(w.style==DashboardWidgetStyle::Graph&&!gpsAuto){drawSensorHistoryWidget(cell,w,width,height);return;}
    auto parent=gpsAuto?view.speedPanel:view.panel;
    auto title=gpsAuto?(view.values[3]=makeLabel(parent,"",8,5,UiText::Caption,cDim)):view.title;
    auto &unit=gpsAuto?view.values[2]:view.detail;
    auto &marker=gpsAuto?view.values[5]:view.marker;
    lv_label_set_text(title,designSensorNames[w.source]);
    const bool speed=w.source==0;
    if(!speed){
        char titleText[64];snprintf(titleText,sizeof(titleText),"%s%s",w.speedValue==SpeedWidgetValue::Maximum?"MAX ":w.speedValue==SpeedWidgetValue::Average?"AVG ":"",designSensorNames[w.source]);
        uiSetText(title,titleText);boundedLabel(title,width-72);
    }
    if(speed){
        const uint8_t primary=static_cast<uint8_t>(w.speedValue);
        const char *compactTitles[]={"Speed","Max","Avg"};
        lv_label_set_text(title,width<180?compactTitles[primary]:speedValueTitles[primary]);
        boundedLabel(title,width-80);
        view.satellites=makeLabel(parent,"-- sats",width-62,21,UiText::Caption,cDim);
        lv_obj_set_width(view.satellites,54);
        lv_obj_set_style_text_align(view.satellites,LV_TEXT_ALIGN_RIGHT,0);
    }
    const UiText::Style valueStyle=speed?(height>130 && w.style!=DashboardWidgetStyle::Graph?(width>220?UiText::Hero:UiText::Reading):UiText::Heading):UiText::Reading;
    view.values[0]=makeLabel(parent,"--",8,speed?(height>130?40:24):33,valueStyle);
    unit=makeLabel(parent,designSensorUnits[w.source],speed?width-43:8,speed?5:height-20,UiText::Caption,cDim);
    if(!speed){
        lv_obj_set_pos(unit,width-62,5);lv_obj_set_width(unit,54);lv_obj_set_style_text_align(unit,LV_TEXT_ALIGN_RIGHT,0);
        view.values[1]=makeLabel(parent,"Max -- / Avg --",8,height-18,UiText::Caption,cDim);
        lv_obj_set_size(view.values[1],width-16,15);lv_label_set_long_mode(view.values[1],LV_LABEL_LONG_CLIP);
        makePageActionButton(x+width-52,y+height-43,44,22,"Reset",UiText::Caption,cPanel2,cWhite,cLine,&TransmitterUi::controlDesign,250+cell);
    }
    if(speed){
        lv_obj_set_pos(unit,width-62,5);lv_obj_set_width(unit,54);
        lv_obj_set_style_text_align(unit,LV_TEXT_ALIGN_RIGHT,0);
        // Two compact rows fit even a 1x1 landscape panel.
        view.values[1]=makeLabel(parent,"Max --\nAvg --",8,height-36,height>130?UiText::Secondary:UiText::Caption,cDim);
        lv_obj_set_size(view.values[1],width-88,36);
        if(width<180)lv_obj_set_style_text_font(view.values[1],UiText::font(UiText::Caption),0);
        lv_label_set_long_mode(view.values[1],LV_LABEL_LONG_CLIP);
    }
    if(speed && w.style==DashboardWidgetStyle::Dials){
        const bool compact=height<130;
        const int diameter=compact?min(width/2-12,height-28):min(width-16,height-82);
        const int ax=compact?8:(width-diameter)/2,ay=compact?23:40;
        if(compact){lv_obj_set_pos(view.values[1],width/2,36);lv_obj_set_size(view.values[1],width/2-8,30);}
        auto arc=lv_arc_create(parent);view.arcs[0]=arc;
        position(arc,ax,ay,diameter,diameter);lv_obj_clear_flag(arc,LV_OBJ_FLAG_CLICKABLE);
        lv_arc_set_rotation(arc,135);lv_arc_set_bg_angles(arc,0,270);
        lv_arc_set_range(arc,0,1000);
        lv_obj_set_style_arc_width(arc,4,LV_PART_MAIN);lv_obj_set_style_arc_width(arc,4,LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc,colour(cLine),LV_PART_MAIN);lv_obj_set_style_arc_color(arc,colour(cCyan),LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,LV_PART_KNOB);lv_obj_set_style_pad_all(arc,0,LV_PART_KNOB);
        lv_obj_set_pos(view.values[0],ax+4,ay+diameter/2-12);
        lv_obj_set_width(view.values[0],diameter-8);lv_obj_set_style_text_align(view.values[0],LV_TEXT_ALIGN_CENTER,0);
        lv_obj_set_style_text_font(view.values[0],UiText::font(diameter>=110?UiText::Heading:UiText::Secondary),0);
        lv_obj_move_foreground(view.values[0]);
        marker=makeLabel(parent,"0-100",ax,ay+diameter-12,UiText::Caption,cDim);
        lv_obj_set_width(marker,diameter);lv_obj_set_style_text_align(marker,LV_TEXT_ALIGN_CENTER,0);
    }
    if(speed){
        const bool compactDial=w.style==DashboardWidgetStyle::Dials && height<130;
        view.speedReset=makePageActionButton(x+width-76,y+height-(compactDial?26:30),68,compactDial?24:28,"Reset",UiText::Caption,cPanel2,cWhite,cLine,&TransmitterUi::controlDesign,gpsAuto?250+cell:148);
        if(gpsAuto){
            view.gpsResetTarget=pageFocusCount_-1;
            lv_obj_set_parent(view.speedReset,parent);
            lv_obj_set_pos(view.speedReset,width-76,height-(compactDial?26:30));
        }
    }
}

void TransmitterUi::updateDashboardSensorWidget(uint8_t cell,const DashboardWidget &w,
    const CrsfTelemetryState &t,uint32_t now,bool gpsAuto)
{
    if(w.style==DashboardWidgetStyle::Graph&&!gpsAuto){updateSensorHistoryWidget(cell,w,t,now);return;}
    auto &view=widgetViews_[cell];
    auto unit=gpsAuto?view.values[2]:view.detail;
    auto marker=gpsAuto?view.values[5]:view.marker;
    if(view.satellites){
        if(t.gpsSatellitesValid&&uint32_t(now-t.gpsSatellitesUpdatedMs)<=3000)
            uiSetTextFmt(view.satellites,"%u sats",t.satellites);
        else uiSetText(view.satellites,"-- sats");
    }
    int32_t value=0;uint32_t stamp=0;
    const bool valid=w.source==0?(value=t.groundSpeedDeciKmh,stamp=t.gpsSpeedUpdatedMs,gpsSpeedAvailable(t,now)):
        sensorReading(t,w.source,value,stamp);
    char text[24];sensorText(w.source,value,text,sizeof(text));
    auto stats=telemetryHistory_?sensorStatistics_[w.source].result(sensorSessionStats_[w.source],now):SensorStatistics::Result{};
    if(w.source==0&&sensorSessionStats_[0])stats={speedStatistics_.available(),speedStatistics_.maximum(),0,speedStatistics_.average()};
    {
        uiSetText(unit,designSensorUnits[w.source]);
        char readings[3][16];
        snprintf(readings[0],sizeof(readings[0]),"%s",valid?text:"--");
        for(uint8_t i=1;i<3;++i){
            const int32_t stat=i==1?stats.maximum:stats.average;
            if(stats.valid)sensorText(w.source,stat,readings[i],sizeof(readings[i]));
            else snprintf(readings[i],sizeof(readings[i]),"--");
        }
        const uint8_t primary=static_cast<uint8_t>(w.speedValue);
        uiSetText(view.values[0],readings[primary]);
        if(view.arcs[0]){
            const bool available=primary==0?valid:stats.valid;
            const int selected=primary==0?value:primary==1?stats.maximum:stats.average;
            const int peak=max(int(stats.maximum),available?selected:0);
            const int maximum=max(1000,((peak+999)/1000)*1000);
            lv_arc_set_range(view.arcs[0],0,1000);
            lv_arc_set_value(view.arcs[0],available?int(int64_t(selected)*1000/maximum):0);
            uiSetArcOpacity(view.arcs[0],available?LV_OPA_COVER:LV_OPA_TRANSP,LV_PART_INDICATOR);
            uiSetTextFmt(marker,"0-%d",maximum/10);
        }
        // Keep the remaining values in Current / Maximum / Average order.
        uint8_t secondary[2],count=0;
        for(uint8_t i=0;i<3;++i)if(i!=primary)secondary[count++]=i;
        uiSetTextFmt(view.values[1],w.source==0?"%s %s\n%s %s":"%s %s / %s %s",
            speedValueShortNames[secondary[0]],readings[secondary[0]],
            speedValueShortNames[secondary[1]],readings[secondary[1]]);
    }
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
            card.focus.height=card.activate.height=lv_obj_get_height(view.panel)-(speed?34:0);
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
