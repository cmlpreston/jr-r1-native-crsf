// Shared chart rendering for dashboard sensors and Sensor detail. Normalized
// coordinates keep large current, capacity and RPM values in LVGL's range.
namespace {
lv_obj_t *makeSensorPlot(lv_obj_t *parent, int x, int y, int width, int height, lv_coord_t *values)
{
    auto plot = lv_chart_create(parent);
    position(plot,x,y,width,height);
    styleWidgetChart(plot,3);
    lv_chart_set_point_count(plot,SensorHistory::Capacity);
    lv_chart_set_range(plot,LV_CHART_AXIS_PRIMARY_Y,0,1000);
    lv_chart_set_div_line_count(plot,3,5);
    for (size_t i = 0; i < SensorHistory::Capacity; ++i) values[i] = LV_CHART_POINT_NONE;
    auto series = lv_chart_add_series(plot,colour(cCyan),LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(plot,series,values);
    return plot;
}

int sensorPlotAxisWidth(uint8_t source)
{
    // RPM and ERS accumulated capacity can fill a signed 32-bit reading.
    return source==2||source==16?82:54;
}

void makeSensorTimeAxis(lv_obj_t *panel,int x,int y,int width)
{
    const int caption = UiText::font(UiText::Caption)->line_height;
    constexpr int timeWidth = 32;
    const bool startsAtInset = x==UiLayout::WidgetInset;
    makeSensorWidgetLabel(panel,"-60 s",startsAtInset?x:x-timeWidth/2,y,timeWidth,caption,
        UiText::Caption,cDim,startsAtInset?LV_TEXT_ALIGN_LEFT:LV_TEXT_ALIGN_CENTER);
    if(width>=110){
        makeSensorWidgetLabel(panel,"-30 s",x+(width-timeWidth)/2,y,timeWidth,caption,
            UiText::Caption,cDim,LV_TEXT_ALIGN_CENTER);
    }
    makeSensorWidgetLabel(panel,"Now",x+width-24,y,24,caption,
        UiText::Caption,cDim,LV_TEXT_ALIGN_RIGHT);
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
    auto &view = widgetViews_[cell];
    const auto &sensor = sensorDescriptors[w.source];
    const int inset = UiLayout::WidgetInset;
    const int bodyY = UiLayout::WidgetBodyY;
    const int caption = UiText::font(UiText::Caption)->line_height;
    const int unitWidth = lv_txt_get_width(sensor.unit,strlen(sensor.unit),UiText::font(UiText::Caption),0,LV_TEXT_FLAG_NONE);
    constexpr int statusWidth = 44;
    constexpr int valueHeight = 20;
    setSensorWidgetTitle(view.title,sensor.name,width-2*inset-unitWidth-UiLayout::SmallGap);
    view.detail = makeSensorWidgetLabel(view.panel,sensor.unit,width-inset-unitWidth,
        UiLayout::WidgetTitleY,unitWidth,caption,UiText::Caption,cDim,LV_TEXT_ALIGN_RIGHT);
    view.values[1] = makeSensorWidgetLabel(view.panel,"Waiting",inset,bodyY+2,statusWidth,caption);
    const int valueX = inset+statusWidth+UiLayout::SmallGap;
    view.values[0] = makeSensorWidgetLabel(view.panel,"--",valueX,bodyY,
        width-inset-valueX,valueHeight,UiText::Menu,cWhite,LV_TEXT_ALIGN_RIGHT);

    const int axisWidth = sensorPlotAxisWidth(w.source);
    const int gap = UiLayout::SmallGap;
    // Keep one pixel per second so LVGL draws true line breaks on narrow cards.
    const bool stackedAxis = width-2*inset-axisWidth-gap-2<=int(SensorHistory::Capacity);
    const int plotX = stackedAxis?inset:inset+axisWidth+gap;
    const int plotY = bodyY+valueHeight+gap+(stackedAxis?caption+gap:0);
    const int plotWidth = width-inset-2-plotX;
    const int plotBottom = height-UiLayout::WidgetFooterHeight-2-(stackedAxis?caption+gap:0);
    const auto align = stackedAxis?LV_TEXT_ALIGN_LEFT:LV_TEXT_ALIGN_RIGHT;
    view.values[2] = makeSensorWidgetLabel(view.panel,"--",inset,
        stackedAxis?plotY-caption-gap:plotY-caption/2,axisWidth,caption,UiText::Caption,cDim,align);
    view.values[3] = makeSensorWidgetLabel(view.panel,"--",inset,
        stackedAxis?plotBottom+gap:plotBottom-caption/2,axisWidth,caption,UiText::Caption,cDim,align);
    makeSensorTimeAxis(view.panel,plotX,height-UiLayout::WidgetFooterHeight+2,plotWidth);
    if(!telemetryHistory_||!widgetMetrics_){
        uiSetText(view.values[1],"No plot");
        return;
    }
    view.plot = makeSensorPlot(view.panel,plotX,plotY,plotWidth,plotBottom-plotY,
        widgetMetrics_.get()->sensorValues[cell]);
}

void TransmitterUi::updateSensorHistoryWidget(uint8_t cell,const DashboardWidget &w,
    const CrsfTelemetryState &t,uint32_t now)
{
    auto &view = widgetViews_[cell];
    if(!view.values[0]) return;
    const auto reading = sensorReading(t,w.source,now);
    char text[32];
    sensorText(w.source,reading.value,text,sizeof(text));
    setSensorWidgetReading(view.values[0],reading.valid?text:"--",reading.valid);
    if(!view.plot||!telemetryHistory_||!widgetMetrics_) return;

    int32_t readings[SensorHistory::Capacity];
    bool valid[SensorHistory::Capacity];
    const auto range = telemetryHistory_.get()->graphs[w.source].project(now,readings,valid);
    uiSetText(view.values[1],reading.valid?"Live":range.available?"Gap":"Waiting");
    for(unsigned n=0;n<2;++n){
        sensorText(w.source,n?range.minimum:range.maximum,text,sizeof(text));
        uiSetText(view.values[2+n],range.available?text:"--");
    }
    updateSensorPlot(view.plot,widgetMetrics_.get()->sensorValues[cell],range,readings,valid);
}

void TransmitterUi::drawSensorDetail()
{
    const auto &sensor = sensorDescriptors[sensorSelected_];
    auto screen = lv_scr_act();
    constexpr int inset = UiLayout::PageInset;
    constexpr int gap = UiLayout::Gap;
    constexpr int buttonHeight = 40;
    constexpr int resetWidth = 68;
    constexpr int sessionWidth = 80;
    const int caption = UiText::font(UiText::Caption)->line_height;
    const int resetX = kScreenWidth-inset-resetWidth;
    const int sessionX = resetX-gap-sessionWidth;
    const int actionsY = designPortrait?84:UiLayout::HeaderHeight+gap;
    const int titleY = actionsY+5;
    const int readingWidth = designPortrait?sessionX-inset-gap:168;
    const int statusX = designPortrait?sessionX:inset+readingWidth+gap;
    const int statusY = designPortrait?actionsY+buttonHeight+UiLayout::SmallGap:titleY+2;
    const int statsX = designPortrait?inset:statusX;
    const int graphTop = designPortrait?157:111;
    const int statsY = designPortrait?graphTop-caption-UiLayout::SmallGap:
        actionsY+buttonHeight+UiLayout::SmallGap;
    const int width = kScreenWidth-2*inset;

    boundedLabel(makeLabel(screen,sensor.name,inset,titleY,UiText::Menu,cDim),readingWidth);
    designValues_[0] = makeSensorWidgetLabel(screen,"--",inset,titleY+22,readingWidth,
        UiText::font(UiText::Heading)->line_height,UiText::Heading,cWhite);
    designDetails_[0] = makeSensorWidgetLabel(screen,"No data",statusX,statusY,72,caption);
    makePageActionButton(sessionX,actionsY,sessionWidth,buttonHeight,
        sensorSessionStats_[sensorSelected_]?"Session":"30s",UiText::Secondary,cPanel,cCyan,cLine,
        &TransmitterUi::controlDesign,248);
    makePageActionButton(resetX,actionsY,resetWidth,buttonHeight,"Reset",
        UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,247);
    designDetails_[2] = makeSensorWidgetLabel(screen,"Max -- / Avg --",statsX,statsY,
        kScreenWidth-inset-statsX,caption);

    if (sensorSelected_ == 17) {
        constexpr int noteY = 184;
        boundedLabel(makeLabel(screen,"Reported by CU01, not voltage-derived.",inset,noteY,
                               UiText::Caption,cDim),width);
        makePageActionButton(inset,noteY+caption+gap,width,buttonHeight,"Set CU01 capacity",
                             UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,249);
        designDetails_[1] = makeLabel(screen,"Discover CU01, read, set mAh, then Apply.",inset,
                                     designFooterY-31,UiText::Caption,cDim);
        boundedLabel(designDetails_[1],width);
        return;
    }
    const int height = designFooterY-graphTop-(designPortrait?8:48);
    auto panel = makePanel(inset,graphTop,width,height,cPanel,cLine);
    constexpr int plotTop = 7;
    const int axisWidth = sensorPlotAxisWidth(sensorSelected_);
    const int plotX = UiLayout::WidgetInset+axisWidth+UiLayout::SmallGap;
    const int plotBottom = height-UiLayout::WidgetFooterHeight-2;
    const int plotHeight = plotBottom-plotTop;
    const int plotWidth = width-plotX-UiLayout::WidgetInset-2;
    for (unsigned i = 0; i < 3; ++i) {
        const int y = plotTop+i*plotHeight/2-caption/2;
        designDetails_[3+i] = makeSensorWidgetLabel(panel,"--",UiLayout::WidgetInset,y,
            axisWidth,caption,UiText::Caption,cDim,LV_TEXT_ALIGN_RIGHT);
    }
    makeSensorTimeAxis(panel,plotX,plotBottom+UiLayout::SmallGap,plotWidth);
    if (telemetryHistory_) {
        sensorDetailChart_ = makeSensorPlot(panel,plotX,plotTop,plotWidth,plotHeight,sensorDetailPoints_);
    }
    designDetails_[1] = makeLabel(panel,"Waiting for history",plotX+UiLayout::SmallGap,
        height/2-caption/2,UiText::Caption,cDim);
    lv_obj_set_width(designDetails_[1],plotWidth-2*UiLayout::SmallGap);
    lv_label_set_long_mode(designDetails_[1],LV_LABEL_LONG_CLIP);
    if (designPortrait) return;

    const int controlsY = designFooterY-buttonHeight-UiLayout::SmallGap;
    constexpr int pauseWidth = 68;
    constexpr int setupWidth = 132;
    const int previousX = inset+pauseWidth+gap;
    const int nextX = previousX+buttonHeight+gap;
    const int cursorX = nextX+buttonHeight+gap;
    const int setupX = kScreenWidth-inset-setupWidth;
    if (telemetryHistory_) {
        makePageActionButton(inset,controlsY,pauseWidth,buttonHeight,v2GraphPaused_?"Live":"Pause",
            UiText::Secondary,cPanel,cCyan,cLine,&TransmitterUi::controlV2,20);
        makePageActionButton(previousX,controlsY,buttonHeight,buttonHeight,"<",
            UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlV2,21);
        makePageActionButton(nextX,controlsY,buttonHeight,buttonHeight,">",
            UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlV2,22);
        designDetails_[6] = makeSensorWidgetLabel(screen,"60 s history",cursorX,
            controlsY+(buttonHeight-caption)/2,setupX-gap-cursorX,caption);
        v2GraphCursorLine_ = lv_line_create(panel);
        lv_obj_set_style_line_color(v2GraphCursorLine_,colour(cAmber),0);
        lv_obj_set_style_line_width(v2GraphCursorLine_,1,0);
    }
    const char *setup=sensorSelected_==0?"Dashboard setup":sensorSelected_==2?"ERS / motor poles":
        sensorSelected_==4?"RF warnings":sensorSelected_>=6&&sensorSelected_<=11?"Battery alarms":
        sensorSelected_>=14?"ERS sensor setup":"Sensor settings";
    makePageActionButton(setupX,controlsY,setupWidth,buttonHeight,setup,
        UiText::Secondary,cPanel2,cCyan,cLine,&TransmitterUi::controlV2,30);
}

void TransmitterUi::updateSensorDetail(uint32_t now)
{
    auto valueLabel = designValues_[0];
    const auto *reading = lv_label_get_text(valueLabel);
    uiSetTextFont(valueLabel,drivePackReadingFont(reading,lv_obj_get_style_width(valueLabel,0),
        lv_obj_get_style_height(valueLabel,0)),0);

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
