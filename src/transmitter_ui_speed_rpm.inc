// Independent observed GPS speed and reported motor RPM. These dials never
// infer either source from the other or apply a motor-pole/gearing correction.
namespace {
uint32_t speedRpmDialMaximum(uint32_t reading,uint32_t minimum)
{
    if(reading<=minimum)return minimum;
    uint64_t decade=1;
    while(decade*10<reading)decade*=10;
    for(unsigned multiple:{1u,2u,3u,5u,10u}){
        const uint64_t ceiling=decade*multiple;
        if(ceiling>=reading)return uint32_t(ceiling);
    }
    return minimum; // All admitted nonnegative int32 values are covered above.
}
}

void TransmitterUi::drawSpeedRpmWidget(uint8_t cell,int width,int height)
{
    auto &view = widgetViews_[cell];
    uiSetText(view.title,"Speed / RPM");
    const int inset = UiLayout::WidgetInset;
    const int gap = UiLayout::SmallGap;
    const int bodyY = UiLayout::WidgetBodyY;
    const int caption = UiText::font(UiText::Caption)->line_height;
    const bool stacked = width<200&&height>140;
    const int columns = stacked?1:2;
    const int rows = stacked?2:1;
    const int columnWidth = (width-2*inset)/columns;
    const int rowHeight = (height-bodyY-gap)/rows;
    const int diameter = max(30,min(columnWidth-2*gap,rowHeight-2*caption-gap));
    const int occupied = diameter+2*caption+gap;

    for(unsigned n=0;n<2;++n){
        const int x = inset+(n%columns)*columnWidth;
        const int y = bodyY+(n/columns)*rowHeight+max(0,(rowHeight-occupied)/2);
        const int dialX = x+(columnWidth-diameter)/2;
        const int dialY = y+caption+gap/2;
        const auto accent = n?cAmber:cCyan;
        view.values[4+n] = makeSensorWidgetLabel(view.panel,n?"MOTOR RPM":"SPEED",
            x,y,columnWidth,caption,UiText::Caption,accent,LV_TEXT_ALIGN_CENTER);
        view.arcs[n] = makeWidgetDial(view.panel,dialX,dialY,diameter,accent);
        lv_arc_set_range(view.arcs[n],0,1000);
        lv_arc_set_value(view.arcs[n],0);
        makeSensorDialLabels(view.panel,dialX,dialY,diameter,n?"krpm":"km/h",view.values[n],view.bars[n]);
        view.values[2+n] = makeSensorWidgetLabel(view.panel,n?"0-60":"0-100",
            x,dialY+diameter+gap/2,columnWidth,caption,UiText::Caption,cDim,LV_TEXT_ALIGN_CENTER);
        view.dialMaximum[n] = n?60000:1000;
    }
}

void TransmitterUi::updateSpeedRpmWidget(uint8_t cell,const CrsfTelemetryState &t,uint32_t now)
{
    auto &view=widgetViews_[cell];
    if(!view.panel||dashboardWidgetForView(cell).kind!=DashboardWidgetKind::SpeedRpm)return;
    const bool valid[]={gpsSpeedAvailable(t,now),t.rpmValid&&t.rpm>=0&&uint32_t(now-t.rpmUpdatedMs)<=3000};
    const uint32_t readings[]={t.groundSpeedDeciKmh,t.rpm>=0?uint32_t(t.rpm):0};
    for(uint8_t n=0;n<2;++n){
        auto &maximum=view.dialMaximum[n];
        if(!maximum)maximum=n?60000:1000;
        if(valid[n]&&readings[n]>maximum)maximum=speedRpmDialMaximum(readings[n],maximum);
        const double divisor=n?(maximum>=1000000000?1000000000.0:maximum>=1000000?1000000.0:1000.0):10.0;
        const char *unit=n?(divisor==1000000000.0?"Grpm":divisor==1000000.0?"Mrpm":"krpm"):"km/h";
        char text[24],scale[24];
        if(!valid[n])snprintf(text,sizeof(text),"--");
        else if(!n)snprintf(text,sizeof(text),readings[n]>=10000?"%.0f":"%.1f",readings[n]/divisor);
        else snprintf(text,sizeof(text),readings[n]/divisor>=10?"%.1f":"%.2f",readings[n]/divisor);
        snprintf(scale,sizeof(scale),"0-%.0f",maximum/divisor);
        setSensorWidgetReading(view.values[n],text,valid[n]);
        uiSetText(view.bars[n],unit);
        uiSetText(view.values[2+n],scale);
        lv_arc_set_value(view.arcs[n],valid[n]?int(uint64_t(readings[n])*1000/maximum):0);
        uiSetArcOpacity(view.arcs[n],valid[n]?LV_OPA_COVER:LV_OPA_TRANSP,LV_PART_INDICATOR);
    }
}
