// Live-only sensor history. LVGL uses a normalized scale so full int32 RPM,
// current and capacity readings never overflow its 16-bit chart coordinates.
void TransmitterUi::drawSensorHistoryWidget(uint8_t cell,const DashboardWidget &w,int width,int height)
{
    auto &v=widgetViews_[cell];
    uiSetText(v.title,designSensorNames[w.source]);boundedLabel(v.title,width-76);
    uiSetHidden(v.title,false);
    v.detail=makeLabel(v.panel,designSensorUnits[w.source],width-68,5,UiText::Caption,cDim);
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
    v.plot=lv_chart_create(v.panel);position(v.plot,60,top,width-70,max(10,bottom-top));
    lv_obj_set_style_pad_all(v.plot,0,0);lv_obj_set_style_border_width(v.plot,0,0);
    lv_obj_set_style_bg_opa(v.plot,LV_OPA_TRANSP,0);
    lv_obj_set_style_line_color(v.plot,colour(cLine),LV_PART_MAIN);
    lv_obj_set_style_line_width(v.plot,1,LV_PART_MAIN);
    lv_obj_set_style_line_width(v.plot,2,LV_PART_ITEMS);
    lv_obj_set_style_size(v.plot,3,LV_PART_INDICATOR);
    lv_obj_clear_flag(v.plot,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(v.plot,LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(v.plot,SensorHistory::Capacity);
    lv_chart_set_range(v.plot,LV_CHART_AXIS_PRIMARY_Y,0,1000);
    lv_chart_set_div_line_count(v.plot,3,5);
    auto values=widgetMetrics_.get()->sensorValues[cell];
    for(size_t p=0;p<SensorHistory::Capacity;++p)values[p]=LV_CHART_POINT_NONE;
    auto series=lv_chart_add_series(v.plot,colour(cCyan),LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(v.plot,series,values);
}

void TransmitterUi::updateSensorHistoryWidget(uint8_t cell,const DashboardWidget &w,
    const CrsfTelemetryState &t,uint32_t now)
{
    auto &v=widgetViews_[cell];
    if(!v.plot||!telemetryHistory_||!widgetMetrics_)return;
    int32_t current=0;uint32_t stamp=0;
    const bool fresh=sensorReading(t,w.source,current,stamp)&&uint32_t(now-stamp)<=SensorHistory::FreshnessMs;
    char text[32];sensorText(w.source,current,text,sizeof(text));
    uiSetText(v.values[0],fresh?text:"--");
    uiSetTextFont(v.values[0],drivePackReadingFont(fresh?text:"--",88,20),0);
    uiSetTextColor(v.values[0],colour(fresh?cWhite:cDim),0);
    int32_t readings[SensorHistory::Capacity];bool valid[SensorHistory::Capacity];
    const auto range=telemetryHistory_.get()->graphs[w.source].project(now,readings,valid);
    const bool narrow=lv_obj_get_width(v.panel)<200;
    uiSetText(v.values[1],narrow?"60 s":fresh?"60 s / Live":range.available?"60 s / Gap":"60 s / Waiting");
    for(uint8_t n=0;n<2;++n){
        sensorText(w.source,n?range.minimum:range.maximum,text,sizeof(text));
        uiSetText(v.values[2+n],range.available?text:"--");
    }
    bool changed=false;auto values=widgetMetrics_.get()->sensorValues[cell];
    for(size_t p=0;p<SensorHistory::Capacity;++p){
        const lv_coord_t value=valid[p]?lv_coord_t((int64_t(readings[p])-range.minimum)*1000/
            (int64_t(range.maximum)-range.minimum)):LV_CHART_POINT_NONE;
        changed|=values[p]!=value;values[p]=value;
    }
    if(changed)lv_chart_refresh(v.plot);
}
