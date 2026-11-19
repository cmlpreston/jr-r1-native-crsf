// Shared chart rendering for dashboard sensors and Sensor detail. Normalized
// coordinates keep large current, capacity and RPM values in LVGL's range.
namespace {
lv_obj_t *makeSensorPlot(lv_obj_t *parent, int x, int y, int width, int height, lv_coord_t *values)
{
    auto plot = lv_chart_create(parent);
    position(plot,x,y,width,height);
    lv_obj_set_style_pad_all(plot,0,0);
    lv_obj_set_style_border_width(plot,0,0);
    lv_obj_set_style_bg_opa(plot,LV_OPA_TRANSP,0);
    lv_obj_set_style_line_color(plot,colour(cLine),LV_PART_MAIN);
    lv_obj_set_style_line_width(plot,1,LV_PART_MAIN);
    lv_obj_set_style_line_width(plot,2,LV_PART_ITEMS);
    lv_obj_set_style_size(plot,3,LV_PART_INDICATOR);
    lv_obj_clear_flag(plot,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(plot,LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(plot,SensorHistory::Capacity);
    lv_chart_set_range(plot,LV_CHART_AXIS_PRIMARY_Y,0,1000);
    lv_chart_set_div_line_count(plot,3,5);
    for (size_t i = 0; i < SensorHistory::Capacity; ++i) values[i] = LV_CHART_POINT_NONE;
    auto series = lv_chart_add_series(plot,colour(cCyan),LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(plot,series,values);
    return plot;
}

void updateSensorPlot(lv_obj_t *plot, lv_coord_t *points, const SensorHistory::Range &range,
                      const int32_t *readings, const bool *valid)
{
    bool changed = false;
    for (size_t i = 0; i < SensorHistory::Capacity; ++i) {
        const lv_coord_t value = valid[i] ? lv_coord_t((int64_t(readings[i]) - range.minimum) * 1000 /
            (int64_t(range.maximum) - range.minimum)) : LV_CHART_POINT_NONE;
        changed |= points[i] != value;
        points[i] = value;
    }
    if (changed) lv_chart_refresh(plot);
}
}

void TransmitterUi::drawSensorHistoryWidget(uint8_t cell,const DashboardWidget &w,int width,int height)
{
    auto &v=widgetViews_[cell];
    uiSetText(v.title,sensorDescriptors[w.source].name);boundedLabel(v.title,width-76);
    uiSetHidden(v.title,false);
    v.detail=makeLabel(v.panel,sensorDescriptors[w.source].unit,width-68,5,UiText::Caption,cDim);
    lv_obj_set_width(v.detail,60);lv_obj_set_style_text_align(v.detail,LV_TEXT_ALIGN_RIGHT,0);
    v.values[0]=makeLabel(v.panel,"--",width-96,22,UiText::Secondary,cWhite);
    lv_obj_set_size(v.values[0],88,20);lv_obj_set_style_text_align(v.values[0],LV_TEXT_ALIGN_RIGHT,0);
    lv_label_set_long_mode(v.values[0],LV_LABEL_LONG_CLIP);
    v.values[1]=makeLabel(v.panel,"60 s / Waiting",8,24,UiText::Caption,cDim);
    boundedLabel(v.values[1],width-112);
    const int top=48,bottom=height-24;
    v.values[2]=makeLabel(v.panel,"--",3,top-4,UiText::Caption,cDim);
    v.values[3]=makeLabel(v.panel,"--",3,bottom-10,UiText::Caption,cDim);
    for(uint8_t n=2;n<4;++n){lv_obj_set_width(v.values[n],54);lv_label_set_long_mode(v.values[n],LV_LABEL_LONG_CLIP);}
    makeLabel(v.panel,"-60s",60,bottom+3,UiText::Caption,cDim);
    makeLabel(v.panel,"Now",width-34,bottom+3,UiText::Caption,cDim);
    if(!telemetryHistory_||!widgetMetrics_){uiSetText(v.values[1],"Unavailable");return;}
    v.plot = makeSensorPlot(v.panel,60,top,width-70,max(10,bottom-top),
                            widgetMetrics_.get()->sensorValues[cell]);
}

void TransmitterUi::updateSensorHistoryWidget(uint8_t cell,const DashboardWidget &w,
    const CrsfTelemetryState &t,uint32_t now)
{
    auto &v=widgetViews_[cell];
    if (!v.values[0]) return;
    const auto reading = sensorReading(t,w.source,now);
    const bool fresh = reading.valid;
    char text[32];sensorText(w.source,reading.value,text,sizeof(text));
    uiSetText(v.values[0],fresh?text:"--");
    uiSetTextFont(v.values[0],drivePackReadingFont(fresh?text:"--",88,20),0);
    uiSetTextColor(v.values[0],colour(fresh?cWhite:cDim),0);
    if(!v.plot||!telemetryHistory_||!widgetMetrics_)return;
    int32_t readings[SensorHistory::Capacity];bool valid[SensorHistory::Capacity];
    const auto range=telemetryHistory_.get()->graphs[w.source].project(now,readings,valid);
    const bool narrow=lv_obj_get_width(v.panel)<200;
    uiSetText(v.values[1],narrow?"60 s":fresh?"60 s / Live":range.available?"60 s / Gap":"60 s / Waiting");
    for(uint8_t n=0;n<2;++n){
        sensorText(w.source,n?range.minimum:range.maximum,text,sizeof(text));
        uiSetText(v.values[2+n],range.available?text:"--");
    }
    updateSensorPlot(v.plot, widgetMetrics_.get()->sensorValues[cell], range, readings, valid);
}

void TransmitterUi::drawSensorDetail()
{
    const auto &sensor = sensorDescriptors[sensorSelected_];
    boundedLabel(makeLabel(lv_scr_act(), sensor.name, 12, designPortrait ? 90 : 49, UiText::Menu, cDim),
                 kScreenWidth - 156);
    designValues_[0] = makeLabel(lv_scr_act(), "--", 12, designPortrait ? 110 : 77, UiText::Heading);
    designDetails_[0] = makeLabel(lv_scr_act(), "No data", designPortrait ? kScreenWidth - 140 : 140,
                                  designPortrait ? 118 : 82, UiText::Caption, cDim);
    makePageActionButton(kScreenWidth-140,designPortrait?85:47,70,30,
        sensorSessionStats_[sensorSelected_]?"Session":"30s",UiText::Caption,cPanel,cCyan,cLine,&TransmitterUi::controlDesign,248);
    makePageActionButton(kScreenWidth-66,designPortrait?85:47,54,30,
        "Reset",UiText::Caption,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,247);
    designDetails_[2] = makeLabel(lv_scr_act(), "Max -- / Avg --", designPortrait ? 12 : 200,
                                  designPortrait ? 137 : 82, UiText::Caption, cDim);
    lv_obj_set_size(designDetails_[2], designPortrait ? kScreenWidth - 24 : 268,
                    UiText::font(UiText::Caption)->line_height);
    lv_label_set_long_mode(designDetails_[2],LV_LABEL_LONG_CLIP);

    if (sensorSelected_ == 17) {
        boundedLabel(makeLabel(lv_scr_act(),"Reported by CU01, not voltage-derived.",12,184,
                               UiText::Caption,cDim),kScreenWidth-24);
        makePageActionButton(12,211,kScreenWidth-24,32,"Set CU01 capacity",
                             UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,249);
        designDetails_[1] = makeLabel(lv_scr_act(),"Discover CU01, read, set mAh, then Apply.",12,
                                     designFooterY-31,UiText::Caption,cDim);
        boundedLabel(designDetails_[1],kScreenWidth-24);
        return;
    }
    const int graphTop = designPortrait ? 157 : 111;
    const int width = kScreenWidth-24;
    const int height = designFooterY-graphTop-(designPortrait?8:48);
    auto panel = makePanel(12,graphTop,width,height,cPanel,cLine);
    for (unsigned i = 0; i < 3; ++i) {
        const int y = i*(height-29)/2;
        auto axis = makeLabel(panel,"--",2,y,UiText::Caption,cDim);
        designDetails_[3+i] = axis;
        lv_obj_set_size(axis,52,15);
        lv_label_set_long_mode(axis,LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(axis,LV_TEXT_ALIGN_RIGHT,0);
    }
    makeLabel(panel,"-60 s",60,height-18,UiText::Caption,cDim);
    makeLabel(panel,"Now",width-34,height-18,UiText::Caption,cDim);
    if (telemetryHistory_) {
        sensorDetailChart_ = makeSensorPlot(panel,60,7,width-70,height-29,sensorDetailPoints_);
    }
    designDetails_[1] = makeLabel(panel,"Waiting for history",64,height/2-7,UiText::Caption,cDim);
    lv_obj_set_width(designDetails_[1],width-84);
    lv_label_set_long_mode(designDetails_[1],LV_LABEL_LONG_CLIP);
    if (designPortrait) return;

    if (telemetryHistory_) {
        makePageActionButton(12,238,68,30,v2GraphPaused_?"Live":"Pause",UiText::Caption,cPanel,cCyan,cLine,&TransmitterUi::controlV2,20);
        makePageActionButton(84,238,30,30,"<",UiText::Caption,cPanel,cWhite,cLine,&TransmitterUi::controlV2,21);
        makePageActionButton(118,238,30,30,">",UiText::Caption,cPanel,cWhite,cLine,&TransmitterUi::controlV2,22);
        designDetails_[6] = makeLabel(lv_scr_act(),"60 s history",154,245,UiText::Caption,cDim);
        boundedLabel(designDetails_[6],143);
        v2GraphCursorLine_ = lv_line_create(panel);
        lv_obj_set_style_line_color(v2GraphCursorLine_,colour(cAmber),0);
        lv_obj_set_style_line_width(v2GraphCursorLine_,1,0);
    }
    const char *setup=sensorSelected_==0?"Dashboard setup":sensorSelected_==2?"ERS / motor poles":
        sensorSelected_==4?"RF warnings":sensorSelected_>=6&&sensorSelected_<=11?"Battery alarms":
        sensorSelected_>=14?"ERS sensor setup":"Sensor settings";
    makePageActionButton(300,238,168,30,setup,UiText::Caption,cPanel2,cCyan,cLine,&TransmitterUi::controlV2,30);
}

void TransmitterUi::updateSensorDetail(uint32_t now)
{
    if (!telemetryHistory_) {
        uiSetText(designDetails_[2],"Statistics unavailable: memory");
        if (sensorSelected_ != 17) uiSetText(designDetails_[1],"History unavailable: memory");
        return;
    }
    const auto &sensor = sensorDescriptors[sensorSelected_];
    auto stats = sensorStatistics_[sensorSelected_].result(sensorSessionStats_[sensorSelected_],now);
    if (sensorSelected_==0 && sensorSessionStats_[0])
        stats={speedStatistics_.available(),speedStatistics_.maximum(),0,speedStatistics_.average()};
    if (stats.valid) {
        char maximum[24],average[24];
        sensorText(sensorSelected_,stats.maximum,maximum,sizeof(maximum));
        sensorText(sensorSelected_,stats.average,average,sizeof(average));
        uiSetTextFmt(designDetails_[2],"Max %s / Avg %s %s",maximum,average,sensor.unit);
    } else {
        uiSetTextFmt(designDetails_[2],"Max -- / Avg -- %s",sensor.unit);
    }
    if (!sensorDetailChart_) return;

    int32_t readings[SensorHistory::Capacity];
    bool valid[SensorHistory::Capacity];
    const auto &history = *telemetryHistory_.get();
    const auto range = v2GraphPaused_ ? history.frozenRange :
        history.graphs[sensorSelected_].project(now,readings,valid);
    for (unsigned i = 0; i < 3; ++i) {
        const int64_t value = int64_t(range.maximum) - (int64_t(range.maximum)-range.minimum)*i/2;
        char text[24];
        sensorText(sensorSelected_,value,text,sizeof(text));
        uiSetText(designDetails_[3+i],range.available?text:"--");
    }
    uiSetHidden(sensorDetailChart_,!range.available);
    uiSetHidden(designDetails_[1],range.available);
    updateSensorPlot(sensorDetailChart_,sensorDetailPoints_,range,
                     v2GraphPaused_?history.frozen:readings,v2GraphPaused_?history.frozenValid:valid);
    updateSensorGraphCursor();
}

void TransmitterUi::updateSensorGraphCursor()
{
    if (!v2GraphCursorLine_) return;
    const auto &history = *telemetryHistory_.get();
    const bool show = v2GraphPaused_ && history.frozenRange.available;
    uiSetHidden(v2GraphCursorLine_,!show);
    if (!show) {
        uiSetText(designDetails_[6],v2GraphPaused_?"No samples":"60 s history");
        return;
    }

    constexpr unsigned newest = SensorHistory::Capacity - 1;
    const int y = lv_obj_get_y(sensorDetailChart_);
    const auto x = lv_coord_t(lv_obj_get_x(sensorDetailChart_) +
        (lv_obj_get_width(sensorDetailChart_)-1)*v2GraphCursor_/newest);
    const lv_point_t points[]={{x,lv_coord_t(y)},{x,lv_coord_t(y+lv_obj_get_height(sensorDetailChart_)-1)}};
    uiSetLinePoints(v2GraphCursorLine_,v2GraphCursorPoints_,points,2);
    const unsigned age = newest - v2GraphCursor_;
    if (history.frozenValid[v2GraphCursor_]) {
        char value[24];
        sensorText(sensorSelected_,history.frozen[v2GraphCursor_],value,sizeof(value));
        uiSetTextFmt(designDetails_[6],"-%us: %s %s",age,value,sensorDescriptors[sensorSelected_].unit);
    } else {
        uiSetTextFmt(designDetails_[6],"-%us: no sample",age);
    }
}
