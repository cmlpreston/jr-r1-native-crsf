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
const lv_font_t *speedRpmReadingFont(const char *text,int width,int height)
{
    for(auto style:{UiText::Reading,UiText::Heading,UiText::Menu,UiText::Secondary,UiText::Caption}){
        const auto font=UiText::font(style);
        if(font->line_height<=height&&lv_txt_get_width(text,strlen(text),font,0,LV_TEXT_FLAG_NONE)<=width)return font;
    }
    return UiText::font(UiText::Caption);
}
}

void TransmitterUi::drawSpeedRpmWidget(uint8_t cell,int width,int height)
{
    auto &view=widgetViews_[cell];
    uiSetText(view.title,width<200?"Speed / RPM":"Speed / motor RPM");
    const bool stacked=width<200&&height>140;
    const int columns=stacked?1:2,rows=stacked?2:1;
    const int cw=(width-16)/columns,rh=(height-28)/rows;
    const int ch=UiText::font(UiText::Caption)->line_height;
    const int diameter=max(30,min(cw-8,rh-2*ch-4));
    const int occupied=diameter+2*ch+4;
    for(uint8_t n=0;n<2;++n){
        const int xx=8+(n%columns)*cw,yy=23+(n/columns)*rh+max(0,(rh-occupied)/2);
        const int ax=xx+(cw-diameter)/2,ay=yy+ch+2;
        view.values[4+n]=makeLabel(view.panel,n?(diameter<80?"RPM":"MOTOR RPM"):"SPEED",xx,yy,UiText::Caption,n?cAmber:cCyan);
        lv_obj_set_size(view.values[4+n],cw-4,ch);lv_obj_set_style_text_align(view.values[4+n],LV_TEXT_ALIGN_CENTER,0);
        auto arc=lv_arc_create(view.panel);view.arcs[n]=arc;
        position(arc,ax,ay,diameter,diameter);
        lv_obj_clear_flag(arc,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        lv_arc_set_rotation(arc,135);lv_arc_set_bg_angles(arc,0,270);lv_arc_set_range(arc,0,1000);lv_arc_set_value(arc,0);
        lv_obj_set_style_arc_width(arc,diameter>=100?5:3,LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc,diameter>=100?5:3,LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc,colour(cLine),LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc,colour(n?cAmber:cCyan),LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,LV_PART_KNOB);
        lv_obj_set_style_pad_all(arc,0,LV_PART_KNOB);
        const int valueHeight=max(ch,diameter/3);
        const int valueY=ay+(diameter-valueHeight-ch)/2;
        view.values[n]=makeLabel(view.panel,"--",ax+4,valueY,UiText::Caption,cWhite);
        lv_obj_set_size(view.values[n],diameter-8,valueHeight);
        lv_obj_set_style_text_align(view.values[n],LV_TEXT_ALIGN_CENTER,0);
        lv_label_set_long_mode(view.values[n],LV_LABEL_LONG_CLIP);
        view.bars[n]=makeLabel(view.panel,n?"krpm":"km/h",ax+4,valueY+valueHeight,UiText::Caption,cDim);
        lv_obj_set_size(view.bars[n],diameter-8,ch);lv_obj_set_style_text_align(view.bars[n],LV_TEXT_ALIGN_CENTER,0);
        view.values[2+n]=makeLabel(view.panel,n?"0-60":"0-100",xx,ay+diameter+2,UiText::Caption,cDim);
        lv_obj_set_size(view.values[2+n],cw-4,ch);lv_obj_set_style_text_align(view.values[2+n],LV_TEXT_ALIGN_CENTER,0);
        view.dialMaximum[n]=n?60000:1000;
    }
}

void TransmitterUi::updateSpeedRpmWidget(uint8_t cell,const CrsfTelemetryState &t,uint32_t now)
{
    auto &view=widgetViews_[cell];
    if(!view.panel||dashboardWidgetForView(cell).kind!=DashboardWidgetKind::SpeedRpm)return;
    lv_obj_update_layout(view.panel);
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
        uiSetText(view.values[n],text);uiSetText(view.bars[n],unit);uiSetText(view.values[2+n],scale);
        uiSetTextFont(view.values[n],speedRpmReadingFont(text,lv_obj_get_width(view.values[n]),lv_obj_get_height(view.values[n])),0);
        lv_arc_set_value(view.arcs[n],valid[n]?int(uint64_t(readings[n])*1000/maximum):0);
        uiSetArcOpacity(view.arcs[n],valid[n]?LV_OPA_COVER:LV_OPA_TRANSP,LV_PART_INDICATOR);
    }
}
