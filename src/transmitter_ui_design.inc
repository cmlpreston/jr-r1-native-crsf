// Shared Drive / Tune / Data / Setup presentation. Included by the LVGL
// translation unit so all screens use one colour, font and focus system.
namespace {
const char *const designSensorNames[] = {"SPEED", "TEMPERATURE", "RPM", "RX VOLTAGE", "LINK QUALITY", "SATELLITES"};
const char *const speedValueNames[] = {"Current", "Maximum", "Average"};
const char *const speedValueTitles[] = {"SPEED", "MAX SPEED", "AVG SPEED"};
const char *const speedValueShortNames[] = {"Now", "Max", "Avg"};
const char *const designSensorUnits[] = {"km/h", "C", "rpm", "V", "%", "sats"};
const char *const designWidgetNames[] = {"Run timer", "Speed", "Temperature", "RPM", "RX voltage", "Link quality", "Satellites", "GPS trail"};
const char *const designFieldNames[] = {"Rate", "Expo", "Endpoint", "Response speed", "Direction", "Trim", "Subtrim"};
const char *const designRoleNames[] = {"Steering", "Throttle", "Brake", "Channel"};
const char *dashboardKindName(DashboardWidgetKind kind) {
    const char *names[]={"Empty","Timer","TX / RX power","Channels","Sensor","GPS map / trail","Model image"};
    return names[static_cast<uint8_t>(kind)<7?static_cast<uint8_t>(kind):0];
}
const char *dashboardStyleName(DashboardWidgetStyle style) {
    const char *names[]={"Numbers","Bars","Dials","Graph","Switches"};
    return names[static_cast<uint8_t>(style)<5?static_cast<uint8_t>(style):0];
}
constexpr bool designPortrait = kScreenWidth < kScreenHeight;
constexpr int designFooterY = kScreenHeight - (designPortrait ? 60 : 40);

bool sensorReading(const CrsfTelemetryState &t, uint8_t sensor, int32_t &value, uint32_t &stamp)
{
    switch (sensor) {
        case 0: value = t.groundSpeedDeciKmh; stamp = t.gpsUpdatedMs; return t.gpsValid && t.satellites >= 4 && uint32_t(millis()-stamp)<=SpeedStatistics::FreshnessMs;
        case 1: value = t.temperatureDeciC; stamp = t.temperatureUpdatedMs; return t.temperatureValid;
        case 2: value = t.rpm; stamp = t.rpmUpdatedMs; return t.rpmValid;
        case 3: value = t.receiverVoltageDecivolts; stamp = t.batteryUpdatedMs; return t.receiverVoltageValid;
        case 4: value = t.uplinkLinkQuality; stamp = t.linkUpdatedMs; return t.linkValid;
        case 5: value = t.satellites; stamp = t.gpsUpdatedMs; return t.gpsValid;
        default: value = 0; stamp = 0; return false;
    }
}
void sensorText(uint8_t sensor, int32_t value, char *text, size_t capacity)
{
    if (sensor == 0 || sensor == 1 || sensor == 3)
        snprintf(text, capacity, "%s%ld.%ld", value < 0 ? "-" : "", labs(value) / 10, labs(value) % 10);
    else snprintf(text, capacity, "%ld", static_cast<long>(value));
}
void boundedLabel(lv_obj_t *label, int width)
{
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}
}

bool TransmitterUi::designPage() const
{
    switch (activePage_) {
        case UiPage::Dashboard: case UiPage::Tune: case UiPage::ValueEditor:
        case UiPage::Telemetry: case UiPage::Gps: case UiPage::ChannelMonitor:
        case UiPage::SensorDetail: case UiPage::MainMenu: case UiPage::Channels:
        case UiPage::ModelSelect: case UiPage::WidgetSelect: case UiPage::WidgetEditor: case UiPage::BatteryMenu: return true;
        default: return false;
    }
}

void TransmitterUi::resetSpeedStatistics()
{
    speedStatistics_.reset(designTelemetry_.gpsUpdatedMs);
    memset(sensorHistoryValid_[0], 0, sizeof(sensorHistoryValid_[0]));
}

void TransmitterUi::toggleRunTimer()
{
    if (runTimerRunning_) runTimerElapsedMs_ += millis() - runTimerStartedMs_;
    else runTimerStartedMs_ = millis();
    runTimerRunning_ = !runTimerRunning_;
    if (activePage_ == UiPage::Dashboard) drawDashboard();
}

int TransmitterUi::designValue() const
{
    const auto &p = profileForChannel(selectedChannel_);
    const bool negative = tuneRole_ == 2;
    switch (editorField_) {
        case 0: return negative ? p.negativeRatePercent : p.positiveRatePercent;
        case 1: return negative ? p.negativeExpoPercent : p.positiveExpoPercent;
        case 2: return negative ? p.negativeEndpointPercent : p.positiveEndpointPercent;
        case 3: return negative ? p.negativeRampMs : p.positiveRampMs;
        case 4: return p.reversed;
        case 5: return channelParameterValue(selectedChannel_, ChannelParameterKind::Trim) / 10;
        case 6: return channelParameterValue(selectedChannel_, ChannelParameterKind::Subtrim) / 10;
        default: return 0;
    }
}

void TransmitterUi::applyDesignValue(int value)
{
    if (editorField_ >= 5) {
        applyChannelParameterAction(encodeChannelParameterTarget(selectedChannel_,
            editorField_ == 5 ? ChannelParameterKind::Trim : ChannelParameterKind::Subtrim), value * 10);
        return;
    }
    auto &p = profileForChannel(selectedChannel_);
    if (editorField_ == 2 && tuneRole_ == 0) {
        p.negativeEndpointPercent = steeringEndpointDrafts_[0];
        p.positiveEndpointPercent = steeringEndpointDrafts_[1];
        sanitizeChannelResponseProfile(p);
        scheduleSettingsSave();
        return;
    }
    // Other steering edits remain symmetric; throttle and brake are independent.
    const bool positive = tuneRole_ != 2;
    const bool negative = tuneRole_ == 0 || tuneRole_ == 2;
    switch (editorField_) {
        case 0: if (positive) p.positiveRatePercent = value; if (negative) p.negativeRatePercent = value; break;
        case 1: if (positive) p.positiveExpoPercent = value; if (negative) p.negativeExpoPercent = value; break;
        case 2: if (positive) p.positiveEndpointPercent = value; if (negative) p.negativeEndpointPercent = value; break;
        case 3: if (positive) p.positiveRampMs = value; if (negative) p.negativeRampMs = value; break;
        case 4: p.reversed = value != 0; break;
    }
    sanitizeChannelResponseProfile(p);
    scheduleSettingsSave();
}

void TransmitterUi::openDesignEditor(uint8_t field, UiPage returnPage)
{
    editorField_ = field;
    editorReturn_ = returnPage;
    editorDraft_ = designValue();
    editorFine_ = false;
    if (field == 2 && tuneRole_ == 0) {
        const auto &profile = profileForChannel(selectedChannel_);
        steeringEndpointDrafts_[0] = profile.negativeEndpointPercent;
        steeringEndpointDrafts_[1] = profile.positiveEndpointPercent;
        steeringEndpointInputSide_ = 0;
        const int input = latestSteeringRawAdc_ <= 4095 ? steeringInput_.mapToPermille(latestSteeringRawAdc_) : 0;
        if (input < -150) steeringEndpointInputSide_ = -1;
        else if (input > 150) steeringEndpointInputSide_ = 1;
        if (steeringEndpointInputSide_) steeringEndpointSide_ = steeringEndpointInputSide_ > 0;
        editorDraft_ = steeringEndpointDrafts_[steeringEndpointSide_];
        editorFine_ = true;
    }
    drawDesignPage(UiPage::ValueEditor);
}

bool TransmitterUi::steeringEndpointEditor() const
{
    return activePage_ == UiPage::ValueEditor && editorField_ == 2 && tuneRole_ == 0;
}

void TransmitterUi::selectSteeringEndpoint(uint8_t side)
{
    if (!steeringEndpointEditor() || side > 1 || side == steeringEndpointSide_) return;
    steeringEndpointSide_ = side;
    editorDraft_ = steeringEndpointDrafts_[side];
    lastInteractionMs_ = millis();
    drawDesignPage(UiPage::ValueEditor);
}

void TransmitterUi::updateSteeringEndpointSelection()
{
    if (!channelRoleSettings_.steeringEnabled || !steeringEndpointEditor() || latestSteeringRawAdc_ > 4095) return;
    // Select from calibrated physical input, before trim, rate or reversal.
    // Hysteresis rejects centre noise. A tap overrides a held wheel until the
    // wheel enters a new side; returning to centre retains the selected side.
    const int input = steeringInput_.mapToPermille(latestSteeringRawAdc_);
    int8_t side = steeringEndpointInputSide_;
    if (input < -150) side = -1;
    else if (input > 150) side = 1;
    else if (input >= -75 && input <= 75) side = 0;
    if (side == steeringEndpointInputSide_) return;
    steeringEndpointInputSide_ = side;
    if (side) selectSteeringEndpoint(side > 0);
}

void TransmitterUi::drawDesignPage(UiPage page)
{
    activePage_ = page;
    startScreen(); resetLvglObjects(); beginPageFocus(false);
    auto button = [this](int x, int y, int w, int h, const char *label, uint8_t action, bool selected = false) {
        return makePageActionButton(x,y,w,h,label,14, selected ? cPanel2 : cPanel,
            selected ? cCyan : cWhite,cLine,&TransmitterUi::controlDesign,action);
    };
    auto label = [](const char *text, int x, int y, int size = 14, uint32_t col = cWhite) {
        return makeLabel(lv_scr_act(), text, x,y,size,col);
    };
    auto row = [&](int y, const char *title, const char *value, uint8_t action, int height = 39) {
        auto panel = makePanel(10,y,kScreenWidth-20,height,cBlack,cLine,0);
        lv_obj_set_style_border_side(panel,LV_BORDER_SIDE_BOTTOM,0);
        boundedLabel(makeLabel(panel,title,10,designPortrait?6:(height-16)/2,14), designPortrait?kScreenWidth-64:kScreenWidth/2-25);
        auto v = makeLabel(panel,value,0,0,14,cCyan);
        if(designPortrait){lv_obj_set_pos(v,10,height-21);boundedLabel(v,kScreenWidth-64);}else lv_obj_align(v,LV_ALIGN_RIGHT_MID,-22,0);
        label(LV_SYMBOL_RIGHT,kScreenWidth-30,y+(height-16)/2,12,cDim);
        addPageActionTarget(10,y,kScreenWidth-20,height,&TransmitterUi::controlDesign,action);
    };
    makePanel(0,0,kScreenWidth,38,cHeader,cHeader,0);
    auto logo = lv_img_create(lv_scr_act());
    if (activeModelSlot_ == 0) {
        lv_img_set_src(logo,&hyper7_icon); lv_img_set_zoom(logo,106);
        lv_img_set_pivot(logo,0,0); lv_obj_set_pos(logo,5,4);
    } else { lv_obj_del(logo); label(LV_SYMBOL_IMAGE,10,10,18,cDim); }
    auto modelLabel = label(modelNames_[activeModelSlot_],47,10,14,0xe7edef);
    boundedLabel(modelLabel,kScreenWidth-(designPortrait?218:248));
    addPageActionTarget(0,0,kScreenWidth-(designPortrait?170:200),38,&TransmitterUi::controlDesign,4);
    designLink_ = label("LQ --",kScreenWidth-(designPortrait?166:192),12,12,cDim);
    addPageActionTarget(kScreenWidth-(designPortrait?170:198),0,77,38,&TransmitterUi::controlDesign,63);
    auto safety = button(kScreenWidth-(designPortrait?88:117),3,designPortrait?83:112,32,"CUT",5);
    designStatus_ = lv_obj_get_child(safety,0);
    const bool data = page == UiPage::Telemetry || page == UiPage::Gps || page == UiPage::ChannelMonitor || page == UiPage::SensorDetail;
    const uint8_t nav = page == UiPage::Dashboard ? 0 : (page == UiPage::Tune || page == UiPage::ValueEditor) ? 1 : data ? 2 : 3;
    const char *const navNames[] = {"Drive","Tune","Data","Setup"};
    if (page != UiPage::ValueEditor) {
        for (uint8_t i=0;i<4;++i) {
            auto tab=button(i*kScreenWidth/4,designFooterY,kScreenWidth/4,40,navNames[i],i,i==nav);
            lv_obj_set_style_text_font(lv_obj_get_child(tab,0),fontFor(16),0);
            if(i==nav){auto mark=lv_obj_create(tab);position(mark,0,35,kScreenWidth/4,5);lv_obj_set_style_bg_color(mark,colour(cCyan),0);lv_obj_set_style_border_width(mark,0,0);}
        }
    }
    if (page == UiPage::Dashboard) {
        label("Drive",12,48,22);
        button(kScreenWidth-135,42,123,34,driveModeSettings_.activeMode == DriveMode::Walk ? "WALK  >" : "NORMAL  >",6);
        button(kScreenWidth-219,42,78,34,"Layout",49);
        drawDashboardWidgets();
    } else if (page == UiPage::Tune) {
        if (tuneRole_ < 3) selectedChannel_ = tuneRole_ == 0 ? channelRoleSettings_.steeringChannel : channelRoleSettings_.throttleChannel;
        for (uint8_t i=0;i<3;++i) button(10+i*(kScreenWidth-20)/3,44,(kScreenWidth-20)/3,34,designRoleNames[i],10+i,tuneRole_==i);
        const int gap = designPortrait ? 62 : 40;
        for (uint8_t i=0;i<4;++i) {
            const uint8_t field = tuneBank_*4+i;
            if (field > 6) break;
            editorField_ = field;
            char value[32]; int v=designValue();
            if (field == 2 && tuneRole_ == 0) {
                const auto &p = profileForChannel(selectedChannel_);
                snprintf(value,sizeof(value),"L %u / R %u%%",p.negativeEndpointPercent,p.positiveEndpointPercent);
            }
            else if (field == 4) snprintf(value,sizeof(value),"%s",v ? "Reversed" : "Normal");
            else snprintf(value,sizeof(value),"%d%s",v,field==3 ? " ms" : "%");
            row(86+i*gap,designFieldNames[field],value,20+field,designPortrait ? 54 : 37);
        }
        button(10,designFooterY-34,90,28,tuneBank_ ? "< Basic" : "More >",28);
        button(kScreenWidth-165,designFooterY-34,155,28,"Detailed settings",29);
    } else if (page == UiPage::ValueEditor) {
        char heading[48]; snprintf(heading,sizeof(heading),"%s / %s",designRoleNames[tuneRole_],designFieldNames[editorField_]);
        label(heading,12,47,18);
        const bool steeringEndpoints = steeringEndpointEditor();
        if (steeringEndpoints) {
            for (uint8_t side=0; side<2; ++side) {
                char text[32];snprintf(text,sizeof(text),"%s%s  %d%%",side?"Right":"Left",
                    side==steeringEndpointSide_?" *":"",steeringEndpointDrafts_[side]);
                button(12+side*(kScreenWidth/2),83,kScreenWidth/2-24,
                       designPortrait?62:38,text,150+side,side==steeringEndpointSide_);
            }
        } else {
            auto value = label("",0,0,32,cCyan);
            lv_label_set_text_fmt(value,"%d%s",editorDraft_,editorField_==3 ? " ms" : editorField_==4 ? "" : "%");
            if (editorField_==4) lv_label_set_text(value,editorDraft_ ? "Reversed" : "Normal");
            lv_obj_align(value,LV_ALIGN_TOP_MID,0,83);
        }
        const int graphY=steeringEndpoints && designPortrait?160:128;
        const int graphH=designPortrait ? (steeringEndpoints?118:150) : 60;
        auto graph=makePanel(12,graphY,kScreenWidth-24,graphH,cPanel,cLine,0);
        designCurve_=lv_line_create(graph);
        auto draft = profileForChannel(selectedChannel_);
        const bool pos=tuneRole_!=2, neg=tuneRole_==0 || tuneRole_==2;
        switch(editorField_) {
            case 0: if(pos)draft.positiveRatePercent=editorDraft_;if(neg)draft.negativeRatePercent=editorDraft_;break;
            case 1: if(pos)draft.positiveExpoPercent=editorDraft_;if(neg)draft.negativeExpoPercent=editorDraft_;break;
            case 2:
                if(steeringEndpoints){draft.negativeEndpointPercent=steeringEndpointDrafts_[0];draft.positiveEndpointPercent=steeringEndpointDrafts_[1];}
                else {if(pos)draft.positiveEndpointPercent=editorDraft_;if(neg)draft.negativeEndpointPercent=editorDraft_;}
                break;
            case 4: draft.reversed=editorDraft_!=0;break;
        }
        for(int i=0;i<33;++i) {
            int input=-1000+i*2000/32;
            int output=applyChannelResponse(draft,input);
            designPoints_[i]={static_cast<lv_coord_t>(4+i*(kScreenWidth-34)/32),static_cast<lv_coord_t>(graphH/2-output*(graphH-8)/2400)};
        }
        lv_line_set_points(designCurve_,designPoints_,33);
        lv_obj_set_style_line_color(designCurve_,colour(cCyan),0); lv_obj_set_style_line_width(designCurve_,2,0);
        if(editorField_==3 || editorField_>=5) { lv_obj_add_flag(designCurve_,LV_OBJ_FLAG_HIDDEN); makeLabel(graph,editorField_==3 ? "Time to reach requested output" : "Offset from channel neutral",10,graphH/2-7,12,cDim); }
        if (steeringEndpoints) label("Steer or tap a side, then use + / -",12,designPortrait?282:190,10,cDim);
        const int y=designPortrait ? 302 : 204, w=(kScreenWidth-44)/4;
        button(10,y,w,38,"-",30); button(18+w,y,w,38,editorFine_ ? "Fine" : "Coarse",32);
        button(26+w*2,y,w,38,"+",31); button(34+w*3,y,w,38,"Reset",33);
        label("Changes take effect only after Apply",12,designPortrait ? 352 : 249,12,cDim);
        button(10,designFooterY,kScreenWidth/2-15,36,"Cancel",34);
        button(kScreenWidth/2+5,designFooterY,kScreenWidth/2-15,36,"Apply",35,true);
    } else if (data) {
        const char *tabs[]={"Sensors","GPS","Channels"};
        for(uint8_t i=0;i<3;++i)button(10+i*(kScreenWidth-20)/3,44,(kScreenWidth-20)/3,34,tabs[i],60+i,(i==0 && page!=UiPage::Gps && page!=UiPage::ChannelMonitor)||(i==1 && page==UiPage::Gps)||(i==2 && page==UiPage::ChannelMonitor));
        if(page==UiPage::Telemetry) {
            const int cols=designPortrait?2:3, w=(kScreenWidth-20)/cols, h=(designFooterY-92)/(6/cols);
            for(uint8_t i=0;i<6;++i) {
                int x=10+i%cols*w,y=85+i/cols*h;
                auto panel=makePanel(x,y,w-4,h-4,cPanel,cLine,0);
                makeLabel(panel,designSensorNames[i],8,9,12,cDim);
                designValues_[i]=makeLabel(panel,"--",8,30,24);
                makeLabel(panel,designSensorUnits[i],8,58,12,cDim);
                designDetails_[i]=makeLabel(panel,"No data",w-78,58,12,cDim);
                addPageActionTarget(x,y,w-4,h-4,&TransmitterUi::controlDesign,70+i);
            }
        } else if(page==UiPage::SensorDetail) {
            label(designSensorNames[sensorSelected_],12,90,16,cDim);
            designValues_[0]=label("--",12,117,32);
            designDetails_[0]=label("No data",170,127,14,cDim);
            if(sensorSelected_==0){
                button(kScreenWidth-140,85,128,32,"Reset stats",148);
                designDetails_[2]=label("Max -- / Avg -- km/h",12,153,12,cDim);
            }
            const int graphTop=sensorSelected_==0?177:163;
            auto panel=makePanel(12,graphTop,kScreenWidth-24,designFooterY-graphTop-41,cPanel,cLine,0);
            designCurve_=lv_line_create(panel);
            lv_obj_set_style_line_color(designCurve_,colour(cCyan),0);lv_obj_set_style_line_width(designCurve_,2,0);
            designDetails_[1]=label("History fills as readings arrive",12,designFooterY-31,12,cDim);
        } else if(page==UiPage::ChannelMonitor) {
            label("Requested / sent (%)",12,85,12,cDim);
            for(uint8_t i=0;i<4;++i) {
                int y=106+i*(designPortrait?66:32);char name[12];snprintf(name,sizeof(name),"CH%u",designBank_*4+i+1);
                label(name,12,y,14);designValues_[i]=label("--",78,y,14,cCyan);
                designBars_[i]=makeBar(lv_scr_act(),designPortrait?12:245,designPortrait?y+25:y+5,designPortrait?290:222,8,-1000,1000);
                lv_bar_set_mode(designBars_[i],LV_BAR_MODE_SYMMETRICAL);
            }
            button(10,designFooterY-34,60,28,"<",80);button(kScreenWidth-70,designFooterY-34,60,28,">",81);
            char bank[32];snprintf(bank,sizeof(bank),"CH%u - CH%u",designBank_*4+1,designBank_*4+4);label(bank,kScreenWidth/2-52,designFooterY-28,12,cDim);
        } else {
            designDetails_[0]=label("Waiting for GPS",12,85,12,cDim);
            int mapH=designFooterY-(designPortrait?212:168);
            designMapViewport_=GpsMapViewport{};
            auto map=createGpsBasemap(lv_scr_act(),10,108,kScreenWidth-20,mapH,designMapViewport_);
            makeLabel(map,"N ^",8,5,12,cDim);
            designCurve_=lv_line_create(map);lv_obj_set_style_line_color(designCurve_,colour(0x00707d),0);lv_obj_set_style_line_width(designCurve_,2,0);
            designMarker_=makeLabel(map,LV_SYMBOL_GPS,0,0,18,cCyan);lv_obj_add_flag(designMarker_,LV_OBJ_FLAG_HIDDEN);
            designDetails_[1]=makeLabel(map,"(c) OpenStreetMap",8,mapH-16,10,cDim);
            button(10,designFooterY-(designPortrait?94:50),80,36,"Zoom -",82);button(94,designFooterY-(designPortrait?94:50),80,36,"Zoom +",83);
            button(178,designFooterY-(designPortrait?94:50),designPortrait?132:130,36,gpsTrailVisible_?"Trail on":"Trail off",84);
            if(designPortrait)button(10,designFooterY-48,300,40,"Reset trail",85);else button(316,designFooterY-50,154,36,"Reset trail",85);
        }
    } else if(page==UiPage::MainMenu || page==UiPage::BatteryMenu) {
        label(page==UiPage::BatteryMenu?"Batteries":"Setup",12,48,22);
        auto menuRow=[&](int y,const char *name,const char *value,uint8_t id){
            const int h=designPortrait?59:38;
            auto panel=makePanel(12,y,kScreenWidth-24,h,cBlack,cLine,0);
            lv_obj_set_style_border_side(panel,LV_BORDER_SIDE_BOTTOM,0);
            auto title=makeLabel(panel,name,0,(h-18)/2,16,cWhite);
            boundedLabel(title,kScreenWidth-118);
            auto v=makeLabel(panel,value,0,0,14,cCyan);lv_obj_align(v,LV_ALIGN_RIGHT_MID,-18,0);
            makeLabel(panel,LV_SYMBOL_RIGHT,kScreenWidth-36,(h-12)/2,12,cCyan);
            addPageActionTarget(12,y,kScreenWidth-24,h,&TransmitterUi::controlDesign,id);
        };
        if(page==UiPage::BatteryMenu){
            menuRow(94,"Transmitter","TX",145);menuRow(94+(designPortrait?59:38),"Receiver","RX",146);
        }else{
            auto tab=[&](const char *name,int x,int w,uint8_t id,bool selected){
                auto p=button(x,43,w,34,name,id,selected);
                lv_obj_set_style_bg_color(p,colour(cBlack),0);lv_obj_set_style_border_width(p,0,0);
                if(selected){auto line=lv_obj_create(p);position(line,0,31,w,2);lv_obj_set_style_bg_color(line,colour(cCyan),0);lv_obj_set_style_border_width(line,0,0);}
            };
            tab("Model",kScreenWidth-176,80,140,setupBank_==0);
            tab("Handset",kScreenWidth-94,84,141,setupBank_==1);
            const char *names[2][5]={{"Channels","Control map","RF module","Drive mode","ESC settings"},{"Display","Sounds / alerts","Batteries","Input monitor","Firmware info"}};
            const uint8_t ids[2][5]={{41,42,44,43,46},{142,47,143,48,144}};
            const char *values[2][5]={{"CH1-16","","ELRS",driveModeSettings_.activeMode==DriveMode::Walk?"Walk":"Normal","Preview"},{"","","TX / RX","",""}};
            const uint8_t section=setupBank_?1:0;
            for(int i=0;i<5;++i)menuRow(85+i*(designPortrait?59:38),names[section][i],values[section][i],ids[section][i]);
        }
    } else if(page==UiPage::Channels) {
        label("Channels",12,48,22);button(kScreenWidth-150,43,138,34,"Channel roles",86);
        for(int i=0;i<4;++i){int ch=designBank_*4+i+1;char name[36];snprintf(name,sizeof(name),"CH%u  %s",ch,ch==channelRoleSettings_.steeringChannel?"Steering":ch==channelRoleSettings_.throttleChannel?"Throttle":"Auxiliary");row(86+i*(designPortrait?67:40),name,"Configure",90+i,designPortrait?56:37);}
        button(10,designFooterY-34,60,28,"<",80);button(kScreenWidth-70,designFooterY-34,60,28,">",81);
    } else if(page==UiPage::ModelSelect) {
        label("Models",12,48,22);
        for(int i=0;i<5;++i){int slot=modelSelectBank_*5+i;row(78+i*(designPortrait?56:29),modelNames_[slot],slot==activeModelSlot_?"Active":"Load",100+i,designPortrait?48:29);}
        boundedLabel(label(modelSelectStatus_[0]?modelSelectStatus_:"Change model in CUT with throttle centred",12,designFooterY-49,12,cDim),kScreenWidth-24);
        button(10,designFooterY-32,60,27,"<",80);button(kScreenWidth-70,designFooterY-32,60,27,">",81);
    } else if(page==UiPage::WidgetSelect) {
        label("Drive layout",12,48,22);
        const char *positions[]={"Top left","Top right","Bottom left","Bottom right"};
        for(int i=0;i<4;++i){
            const auto &w=dashboardLayouts_[activeModelSlot_].cells[i];
            char description[40];
            bool covered=false;for(int j=0;j<4;++j)if(j!=i && (dashboardWidgetMask(j,dashboardLayouts_[activeModelSlot_].cells[j])&(1<<i)))covered=true;
            snprintf(description,sizeof(description),"%s",covered?"Covered":dashboardKindName(w.kind));
            row(86+i*(designPortrait?65:39),positions[i],description,110+i,designPortrait?56:36);
        }
        if(designPortrait)button(10,designFooterY-48,300,40,"Use suggested layout",147);
        else label("Select a position to add or edit a widget",12,designFooterY-25,12,cDim);
    } else if(page==UiPage::WidgetEditor) {
        label("Widget settings",12,47,20);
        const bool speed=widgetDraft_.kind==DashboardWidgetKind::Sensor && widgetDraft_.source==0;
        const char *names[]={"Type","Size","Style","Source",speed?"Large value":"Second channel"};
        char values[5][32];
        snprintf(values[0],32,"%s",dashboardKindName(widgetDraft_.kind));
        snprintf(values[1],32,"%u x %u",widgetDraft_.width,widgetDraft_.height);
        snprintf(values[2],32,"%s",dashboardStyleName(widgetDraft_.style));
        if(widgetDraft_.kind==DashboardWidgetKind::Sensor)snprintf(values[3],32,"%s",designSensorNames[widgetDraft_.source%6]);
        else snprintf(values[3],32,"CH%u",widgetDraft_.source+1);
        if(widgetDraft_.secondSource==16)snprintf(values[4],32,"None");else snprintf(values[4],32,"CH%u",widgetDraft_.secondSource+1);
        if(speed)snprintf(values[4],32,"%s",speedValueNames[static_cast<uint8_t>(widgetDraft_.speedValue)]);
        for(int i=0;i<5;++i){
            if(i>=3 && widgetDraft_.kind!=DashboardWidgetKind::Sensor && widgetDraft_.kind!=DashboardWidgetKind::Channels)break;
            if(i==4 && widgetDraft_.kind!=DashboardWidgetKind::Channels && !speed)break;
            row(77+i*(designPortrait?51:30),names[i],values[i],120+i,designPortrait?47:28);
        }
        label("Apply replaces any overlapping panels",12,designFooterY-(designPortrait?65:45),12,cDim);
        button(10,designFooterY-(designPortrait?44:23),90,designPortrait?40:23,"Cancel",125);button(kScreenWidth-110,designFooterY-(designPortrait?44:23),100,designPortrait?40:23,"Apply",126,true);
    }
    finishPageFocus();
    updateDesign(designTelemetry_,designSafety_,designTxVoltage_,designTxValid_);
    uiMetrics.buildUs = uint32_t(micros()) - uiMetrics.pageStartedUs;
}

void TransmitterUi::controlDesign(uint8_t item, PageControlEvent event)
{
    playKeyClick();
    if(item<=3){designBank_=0;if(item==3)setupBank_=0;drawDesignPage(item==0?UiPage::Dashboard:item==1?UiPage::Tune:item==2?UiPage::Telemetry:UiPage::MainMenu);return;}
    if(item==4){drawModelSelectPage();return;}
    if(item==5){
        // CUT is immediate. Touch arming requires a deliberate 800 ms hold.
        if(armRequested_){armRequested_=false;safetyResetRequested_=true;}
        else if(designTouchDispatch_ && lastTouchDurationMs_>=kDashboardArmHoldMs){armRequested_=true;safetyResetRequested_=true;}
        return;
    }
    if(item==6){driveModesReturnPage_=UiPage::Dashboard;drawDriveModesPage();return;}
    if(item==7){toggleRunTimer();return;}
    if(item==8){runTimerElapsedMs_=0;runTimerStartedMs_=millis();return;}

    if(item>=10 && item<=12){tuneRole_=item-10;drawDesignPage(UiPage::Tune);return;}
    if(item>=20 && item<=26){
        if(activePage_==UiPage::Dashboard){tuneRole_=item==20||item==22?0:2;selectedChannel_=tuneRole_==0?channelRoleSettings_.steeringChannel:channelRoleSettings_.throttleChannel;openDesignEditor(item>=22?5:0,UiPage::Dashboard);}
        else openDesignEditor(item-20,UiPage::Tune);
        return;
    }
    if(item==28){tuneBank_=!tuneBank_;drawDesignPage(UiPage::Tune);return;}
    if(item==29){drawChannelPlaceholder();return;}
    if((item==150 || item==151) && steeringEndpointEditor()) {
        if(event==PageControlEvent::Activate) selectSteeringEndpoint(item-150);
        return;
    }
    if(item>=30 && item<=35){
        if(item==34){drawDesignPage(editorReturn_);return;}
        if(item==35){applyDesignValue(editorDraft_);drawDesignPage(editorReturn_);return;}
        if(item==32)editorFine_=!editorFine_;
        else if(item==33)editorDraft_=editorField_==0||editorField_==2?100:0;
        else {int step=editorField_==3?(editorFine_?10:50):editorField_==4?1:(editorFine_?1:5);int direction=item==30?-1:1;if(event==PageControlEvent::Decrease)direction=-1;editorDraft_+=direction*step;}
        int low=editorField_==1?-100:editorField_>=5?-20:editorField_==2?50:0;
        int high=editorField_==3?2000:editorField_==4?1:editorField_>=5?20:100;
        editorDraft_=constrain(editorDraft_,low,high);
        if(steeringEndpointEditor()) steeringEndpointDrafts_[steeringEndpointSide_]=editorDraft_;
        drawDesignPage(UiPage::ValueEditor);return;
    }
    switch(item){
        case 40:drawModelSelectPage();return;
        case 41:designBank_=0;drawChannelsPage();return;
        case 42:drawControlAssignmentsPage();return;
        case 43:driveModesReturnPage_=UiPage::MainMenu;drawDriveModesPage();return;
        case 44:txConfig_.start(millis());txConfigFolder_=txConfigOffset_=txConfigSelected_=0;drawElrsTxConfigPage();return;
        case 45:drawElrsBindingPage();return;
        case 46:drawEscProgrammingPage();return;
        case 47:drawSetupPage();return;
        case 48:drawInputMonitorPage();return;
        case 49:designBank_=0;drawDesignPage(UiPage::WidgetSelect);return;
        case 63:drawRadioDiagnostics();return;
        case 60:drawTelemetryPage();return;
        case 61:drawDesignPage(UiPage::Gps);return;
        case 62:designBank_=0;drawDesignPage(UiPage::ChannelMonitor);return;
    }
    if(item>=70 && item<=75){sensorSelected_=item-70;drawDesignPage(UiPage::SensorDetail);return;}
    if(item==80 || item==81){
        int direction=item==80?-1:1;
        if(activePage_==UiPage::ModelSelect)modelSelectBank_=constrain(modelSelectBank_+direction,0,1);
        else if(activePage_==UiPage::MainMenu)setupBank_=constrain(setupBank_+direction,0,designPortrait?1:2);
        else designBank_=constrain(designBank_+direction,0,3);
        drawDesignPage(activePage_);return;
    }
    if(item>=82 && item<=85){if(item==82)gpsZoom_=max<int>(1,gpsZoom_-1);if(item==83)gpsZoom_=min<int>(8,gpsZoom_+1);if(item==84)gpsTrailVisible_=!gpsTrailVisible_;if(item==85)gpsCount_=0;drawDesignPage(UiPage::Gps);return;}
    if(item==86){drawChannelRolesPage();return;}
    if(item>=90 && item<=93){selectedChannel_=designBank_*4+item-90+1;drawChannelPlaceholder();return;}
    if(item>=100 && item<=104){selectModelMemory(modelSelectBank_*5+item-100);drawModelSelectPage();return;}
    if(item>=110 && item<=113){widgetCell_=item-110;widgetDraft_=dashboardLayouts_[activeModelSlot_].cells[widgetCell_];drawDesignPage(UiPage::WidgetEditor);return;}
    if(item>=120 && item<=124){
        int direction=event==PageControlEvent::Decrease?-1:1;
        if(item==120)widgetDraft_.kind=static_cast<DashboardWidgetKind>((static_cast<int>(widgetDraft_.kind)+direction+7)%7);
        if(item==121){int size=widgetDraft_.height==2?2:widgetDraft_.width==2?1:0;int count=widgetCell_==0?3:widgetCell_==2?2:1;size=(size+direction+count)%count;widgetDraft_.width=size?2:1;widgetDraft_.height=size==2?2:1;}
        if(item==122){int style=static_cast<int>(widgetDraft_.style);for(int n=0;n<5;++n){style=(style+direction+5)%5;if(dashboardStyleAllowed(widgetDraft_.kind,static_cast<DashboardWidgetStyle>(style)))break;}widgetDraft_.style=static_cast<DashboardWidgetStyle>(style);}
        if(item==123)widgetDraft_.source=(widgetDraft_.source+direction+(widgetDraft_.kind==DashboardWidgetKind::Sensor?6:16))%(widgetDraft_.kind==DashboardWidgetKind::Sensor?6:16);
        if(item==124){
            if(widgetDraft_.kind==DashboardWidgetKind::Sensor && widgetDraft_.source==0)
                widgetDraft_.speedValue=static_cast<SpeedWidgetValue>((static_cast<int>(widgetDraft_.speedValue)+direction+3)%3);
            else widgetDraft_.secondSource=(widgetDraft_.secondSource+direction+17)%17;
        }
        widgetDraft_=sanitizedDashboardWidget(widgetCell_,widgetDraft_);drawDesignPage(UiPage::WidgetEditor);return;
    }
    if(item==125){drawDesignPage(UiPage::WidgetSelect);return;}
    if(item==126){placeDashboardWidget(dashboardLayouts_[activeModelSlot_],widgetCell_,widgetDraft_);saveModelMemorySettings();drawDashboard();return;}
    if(item==140 || item==141){setupBank_=item-140;drawMainMenu();return;}
    if(item==142){displaySetupBank_=0;drawDisplayPage();return;}
    if(item==143){drawDesignPage(UiPage::BatteryMenu);return;}
    if(item==144){drawFirmwareInfoPage();return;}
    if(item==145){drawTxBatteryPage();return;}
    if(item==146){drawRxBatteryPage();return;}
    if(item==148){resetSpeedStatistics();return;}
    if(item==147){dashboardLayouts_[activeModelSlot_]=defaultDashboardLayout();saveModelMemorySettings();drawDashboard();return;}
    if(item>=130 && item<=133){const auto &w=dashboardLayouts_[activeModelSlot_].cells[item-130];
        if(w.kind==DashboardWidgetKind::Map)drawDesignPage(UiPage::Gps);
        else if(w.kind==DashboardWidgetKind::Channels){designBank_=w.source/4;drawDesignPage(UiPage::ChannelMonitor);}
        else if(w.kind==DashboardWidgetKind::Sensor){sensorSelected_=w.source;drawDesignPage(UiPage::SensorDetail);}
        else if(w.kind==DashboardWidgetKind::Power)drawRadioDiagnostics();
        else if(w.kind==DashboardWidgetKind::Model)drawModelSelectPage();
        return;
    }
}

void TransmitterUi::updateDesign(const CrsfTelemetryState &t, OutputSafetyStatus safety, uint16_t tx, bool txValid)
{
    const uint32_t now=millis();
    // Histories only contain real decoded readings; storage is fixed and bounded.
    if(static_cast<int32_t>(now-nextSensorSampleMs_)>=0){
        nextSensorSampleMs_=now+1000;
        for(int sensor=0;sensor<6;++sensor){
            for(int j=0;j<29;++j){sensorHistory_[sensor][j]=sensorHistory_[sensor][j+1];sensorHistoryValid_[sensor][j]=sensorHistoryValid_[sensor][j+1];}
            uint32_t stamp=0;sensorHistoryValid_[sensor][29]=sensorReading(t,sensor,sensorHistory_[sensor][29],stamp);
        }
        for(int ch=0;ch<16;++ch){for(int i=0;i<29;++i)channelHistory_[ch][i]=channelHistory_[ch][i+1];channelHistory_[ch][29]=dashboardChannelValues_[ch];}
        sensorHistoryCount_=min<int>(30,sensorHistoryCount_+1);
    }
    if(t.gpsValid && t.satellites>=4 && t.gpsUpdatedMs!=gpsLastSampleMs_){
        gpsLastSampleMs_=t.gpsUpdatedMs;
        if(gpsCount_==48){for(int i=0;i<47;++i){gpsLat_[i]=gpsLat_[i+1];gpsLon_[i]=gpsLon_[i+1];}--gpsCount_;}
        gpsLat_[gpsCount_]=t.latitudeE7;gpsLon_[gpsCount_]=t.longitudeE7;++gpsCount_;
    }
    const bool lowTx=txValid && tx<=txBatterySettings_.lowAlarmDecivolts;
    const bool lowRx=t.receiverVoltageValid && rxBatterySettings_.alarmEnabled && t.receiverVoltageDecivolts<=rxBatterySettings_.lowAlarmDecivolts;
    if(designStatus_){
        const char *text="CUT";uint32_t col=cAmber;
        switch(safety){
            case OutputSafetyStatus::Armed:text="LIVE";col=cGreen;break;
            case OutputSafetyStatus::StartupWarning:text="CHECK";col=cRed;break;
            case OutputSafetyStatus::LinkRequired:text="LINK WAIT";break;
            case OutputSafetyStatus::NeutralRequired:text="NEUTRAL";break;
            case OutputSafetyStatus::Calibration:text="CALIBRATE";break;
            case OutputSafetyStatus::InputFault:text="INPUT FAULT";col=cRed;break;
            case OutputSafetyStatus::ControlStale:text="STALE";col=cRed;break;
            case OutputSafetyStatus::ChannelDisabled:text="CH OFF";break;
            case OutputSafetyStatus::Cut:break;
        }
        uiSetText(designStatus_,text);
        uiSetTextFont(designStatus_,fontFor(strlen(text)>7?10:14),0);
        uiSetTextColor(designStatus_,lv_color_hex(col==cGreen?0xbcebc7:col==cRed?0xffd9d3:0xffdfa2),0);
        uiSetBgColor(lv_obj_get_parent(designStatus_),lv_color_hex(col==cGreen?0x205239:col==cRed?0x692e28:0x604918),0);
    }
    if(designLink_){
        if(lowTx || lowRx)uiSetText(designLink_,lowTx?"TX LOW":"RX LOW");
        else if(t.linkValid)uiSetTextFmt(designLink_,"%u LQ",t.uplinkLinkQuality);
        else uiSetText(designLink_,t.linkUpdatedMs?"LQ stale":"LQ --");
        uiSetTextColor(designLink_,lv_color_hex(lowTx||lowRx?0xff746a:t.linkValid?0xa4d6ae:0xa9bbc5),0);
    }
    auto reading=[&](uint8_t sensor,lv_obj_t *valueLabel,lv_obj_t *detailLabel){
        if(!valueLabel)return;
        int32_t value=0;uint32_t stamp=0;bool valid=sensorReading(t,sensor,value,stamp);char text[32];sensorText(sensor,value,text,sizeof(text));
        uiSetText(valueLabel,valid?text:"--");
        uiSetTextColor(valueLabel,colour(valid?cWhite:cDim),0);
        if(detailLabel){
            if(!valid)uiSetText(detailLabel,stamp?"Stale":"No data");
            else uiSetTextFmt(detailLabel,"%lus",static_cast<unsigned long>((now-stamp)/1000));
        }
    };
    if(activePage_==UiPage::Dashboard){
        updateDashboardWidgets(t,tx,txValid);
    } else if(activePage_==UiPage::Telemetry){for(int i=0;i<6;++i)reading(i,designValues_[i],designDetails_[i]);}
    else if(activePage_==UiPage::SensorDetail){
        reading(sensorSelected_,designValues_[0],designDetails_[0]);
        if(sensorSelected_==0){
            if(speedStatistics_.available())uiSetTextFmt(designDetails_[2],"Max %u.%u / Avg %u.%u km/h",
                speedStatistics_.maximum()/10,speedStatistics_.maximum()%10,speedStatistics_.average()/10,speedStatistics_.average()%10);
            else uiSetText(designDetails_[2],"Max -- / Avg -- km/h");
        }
        int32_t lo=INT32_MAX,hi=INT32_MIN;int count=0;
        // Draw only the most recent contiguous valid segment, never across gaps.
        int start=30;while(start>0 && sensorHistoryValid_[sensorSelected_][start-1])--start;
        for(int i=start;i<30;++i){lo=min(lo,sensorHistory_[sensorSelected_][i]);hi=max(hi,sensorHistory_[sensorSelected_][i]);++count;}
        if(count>=2){
            int height=lv_obj_get_height(lv_obj_get_parent(designCurve_))-8;
            for(int i=start;i<30;++i)designPoints_[i-start]={static_cast<lv_coord_t>(4+(i-start)*(kScreenWidth-36)/max(1,count-1)),static_cast<lv_coord_t>(height-4-(static_cast<int64_t>(sensorHistory_[sensorSelected_][i])-lo)*(height-8)/max<int64_t>(1,static_cast<int64_t>(hi)-lo))};
            lv_line_set_points(designCurve_,designPoints_,count);lv_obj_clear_flag(designCurve_,LV_OBJ_FLAG_HIDDEN);
            char a[20],b[20];sensorText(sensorSelected_,lo,a,sizeof(a));sensorText(sensorSelected_,hi,b,sizeof(b));uiSetTextFmt(designDetails_[1],"Min %s / Max %s %s  |  %us",a,b,designSensorUnits[sensorSelected_],count);
        } else {lv_obj_add_flag(designCurve_,LV_OBJ_FLAG_HIDDEN);uiSetText(designDetails_[1],"Waiting for fresh sensor history");}
    } else if(activePage_==UiPage::ChannelMonitor){
        for(int i=0;i<4;++i){int ch=designBank_*4+i;int output=permilleFromCrsfValue(dashboardChannelValues_[ch]);
            uiSetTextFmt(designValues_[i],"%+d / %+d%% %s",permilleFromCrsfValue(designRequested_[ch])/10,output/10,(dashboardEnabledChannelMask_&(1U<<ch))?"":"off");lv_bar_set_value(designBars_[i],output,LV_ANIM_OFF);
        }
    } else if(activePage_==UiPage::Gps){
        if(!t.gpsValid || t.satellites<4)uiSetText(designDetails_[0],t.gpsUpdatedMs?"GPS stale / no fix":"Waiting for GPS telemetry");
        else uiSetTextFmt(designDetails_[0],"%u sats  %u.%u km/h  %u deg",t.satellites,t.groundSpeedDeciKmh/10,t.groundSpeedDeciKmh%10,t.headingCentiDegrees/100);
        if(gpsCount_){
            const int width=kScreenWidth-20,height=designFooterY-(designPortrait?212:168);
            updateGpsBasemap(lv_obj_get_parent(designCurve_),designMapViewport_,gpsLat_[gpsCount_-1],gpsLon_[gpsCount_-1],0.5*gpsZoom_);
            const double lonScale=cos(gpsLat_[gpsCount_-1]/1e7*3.141592653589793/180.0);
            const double pixelsPerMetre=0.5*gpsZoom_;
            for(int i=0;i<gpsCount_;++i){
                double east=(static_cast<double>(gpsLon_[i])-gpsLon_[gpsCount_-1])*0.01113195*lonScale;
                double north=(static_cast<double>(gpsLat_[i])-gpsLat_[gpsCount_-1])*0.01113195;
                gpsPoints_[i]={static_cast<lv_coord_t>(constrain(width/2+east*pixelsPerMetre,-10000,10000)),static_cast<lv_coord_t>(constrain(height/2-north*pixelsPerMetre,-10000,10000))};
            }
            if(gpsCount_>1 && gpsTrailVisible_){lv_line_set_points(designCurve_,gpsPoints_,gpsCount_);lv_obj_clear_flag(designCurve_,LV_OBJ_FLAG_HIDDEN);}else lv_obj_add_flag(designCurve_,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(designMarker_,width/2-9,height/2-9);lv_obj_clear_flag(designMarker_,LV_OBJ_FLAG_HIDDEN);
            uiSetTextColor(designMarker_,colour(t.gpsValid && t.satellites>=4?0x00707d:0x53636c),0);
            uiSetTextFmt(designDetails_[1],"%um | %s",static_cast<unsigned>(40/pixelsPerMetre),GpsMap::covers(t.latitudeE7,t.longitudeE7)?"(c) OpenStreetMap":"No local map");
        }
    }
    // Retain the existing voltage warning cadence on every primary page.
    if(lowTx){if(static_cast<int32_t>(now-nextTxLowBatteryAlarmMs_)>=0){playBuzzer(300);nextTxLowBatteryAlarmMs_=now+kLowBatteryRepeatMs;}}else nextTxLowBatteryAlarmMs_=now;
    if(lowRx){if(static_cast<int32_t>(now-nextRxLowBatteryAlarmMs_)>=0){playBuzzer(300);nextRxLowBatteryAlarmMs_=now+kLowBatteryRepeatMs;}}else nextRxLowBatteryAlarmMs_=now;
}

void TransmitterUi::drawDashboardWidgets()
{
    const auto &layout=dashboardLayouts_[activeModelSlot_];
    constexpr int gap=6, top=83;
    const int cellW=(kScreenWidth-20-gap)/2, cellH=(designFooterY-top-gap-6)/2;
    uint8_t occupied=0;
    for(uint8_t i=0;i<4;++i)occupied |= dashboardWidgetMask(i,layout.cells[i]);
    for(uint8_t cell=0;cell<4;++cell){
        const auto &w=layout.cells[cell];
        int x=10+cell%2*(cellW+gap),y=top+cell/2*(cellH+gap);
        if(w.kind==DashboardWidgetKind::Empty){
            if(!(occupied&(1<<cell)))makePageActionButton(x,y,cellW,cellH,"+ Add widget",14,cPanel,cDim,cLine,&TransmitterUi::controlDesign,110+cell);
            continue;
        }
        auto &view=widgetViews_[cell];
        const int width=cellW*w.width+gap*(w.width-1),height=cellH*w.height+gap*(w.height-1);
        view.panel=makePanel(x,y,width,height,cPanel,cLine,0);
        auto title=makeLabel(view.panel,dashboardKindName(w.kind),8,5,12,cDim);
        boundedLabel(title,width-16);
        if(w.kind!=DashboardWidgetKind::Timer)addPageActionTarget(x,y,width,height,&TransmitterUi::controlDesign,130+cell);
        if(w.kind==DashboardWidgetKind::Timer){
            view.values[0]=makeLabel(view.panel,"00:00",8,30,width>220&&height>130?48:32);
            makePageActionButton(x+8,y+height-(designPortrait?45:31),width/2-12,designPortrait?40:26,runTimerRunning_?"Stop":"Start",12,cPanel2,cCyan,cLine,&TransmitterUi::controlDesign,7);
            makePageActionButton(x+width/2+4,y+height-(designPortrait?45:31),width/2-12,designPortrait?40:26,"Reset",12,cPanel2,cWhite,cLine,&TransmitterUi::controlDesign,8);
        } else if(w.kind==DashboardWidgetKind::Power || w.kind==DashboardWidgetKind::Channels){
            bool power=w.kind==DashboardWidgetKind::Power;
            const int count=power || w.secondSource<16?2:1;
            if(w.style==DashboardWidgetStyle::Graph){
                view.plot=lv_line_create(view.panel);lv_obj_set_style_line_color(view.plot,colour(cCyan),0);lv_obj_set_style_line_width(view.plot,2,0);
                if(count==2){view.secondPlot=lv_line_create(view.panel);lv_obj_set_style_line_color(view.secondPlot,colour(cAmber),0);lv_obj_set_style_line_width(view.secondPlot,2,0);}
                view.detail=makeLabel(view.panel,"Output history / 30 s",8,height-16,10,cDim);
            }
            for(int j=0;j<count;++j){
                int channel=(j?w.secondSource:w.source)+1;
                char text[20];snprintf(text,sizeof(text),power?(j?"RX":"TX"):"CH%u",channel);
                if(w.style==DashboardWidgetStyle::Dials){
                    int diameter=min(width/count-12,height-24);
                    int ax=width/(2*count)+j*width/count-diameter/2;
                    auto arc=lv_arc_create(view.panel);view.arcs[j]=arc;
                    position(arc,ax,22,diameter,diameter);lv_obj_clear_flag(arc,LV_OBJ_FLAG_CLICKABLE);
                    lv_arc_set_rotation(arc,135);lv_arc_set_bg_angles(arc,0,270);
                    lv_obj_set_style_arc_width(arc,4,LV_PART_MAIN);lv_obj_set_style_arc_width(arc,4,LV_PART_INDICATOR);
                    lv_obj_set_style_arc_color(arc,colour(cLine),LV_PART_MAIN);lv_obj_set_style_arc_color(arc,colour(cCyan),LV_PART_INDICATOR);
                    lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,LV_PART_KNOB);lv_obj_set_style_pad_all(arc,0,LV_PART_KNOB);
                    auto name=makeLabel(view.panel,text,0,0,10,cDim);lv_obj_set_pos(name,ax+diameter/2-12,22+diameter-14);
                    view.values[j]=makeLabel(view.panel,"--",ax+5,22+diameter/2-9,14,cWhite);
                    lv_obj_set_width(view.values[j],diameter-10);lv_obj_set_style_text_align(view.values[j],LV_TEXT_ALIGN_CENTER,0);
                } else {
                    int yy=25+j*(height-27)/count;
                    if(w.style==DashboardWidgetStyle::Graph){
                        makeLabel(view.panel,text,8+j*width/count,22,10,j?cAmber:cCyan);
                        view.values[j]=makeLabel(view.panel,"--",50+j*width/count,22,10);
                        continue;
                    }
                    makeLabel(view.panel,text,8,yy,12,cDim);
                    view.values[j]=makeLabel(view.panel,"--",power&&height>130?8:width/2,power&&height>130?yy+18:yy,power&&height>130?24:16);
                    if(w.style==DashboardWidgetStyle::Bars){view.bars[j]=makeBar(view.panel,8,yy+(power&&height>130?48:19),width-16,5,power?0:-1000,power?160:1000);if(!power)lv_bar_set_mode(view.bars[j],LV_BAR_MODE_SYMMETRICAL);}
                }
            }
        } else if(w.kind==DashboardWidgetKind::Sensor){
            lv_label_set_text(title,designSensorNames[w.source]);
            const bool speed=w.source==0;
            if(speed)lv_label_set_text(title,speedValueTitles[static_cast<uint8_t>(w.speedValue)]);
            const int valueFont=speed?(height>130 && w.style!=DashboardWidgetStyle::Graph?(width>220?48:32):24):32;
            view.values[0]=makeLabel(view.panel,"--",8,speed?24:33,valueFont);
            view.detail=makeLabel(view.panel,designSensorUnits[w.source],speed?width-43:8,speed?5:height-20,speed?10:12,cDim);
            if(speed){
                // Two compact rows fit even a 1x1 landscape panel.
                view.values[1]=makeLabel(view.panel,"Max --\nAvg --",8,height-36,height>130?14:12,cDim);
                lv_obj_set_size(view.values[1],width-16,36);
                lv_label_set_long_mode(view.values[1],LV_LABEL_LONG_CLIP);
            }
            if(w.style==DashboardWidgetStyle::Graph){view.plot=lv_line_create(view.panel);lv_obj_set_style_line_color(view.plot,colour(cCyan),0);lv_obj_set_style_line_width(view.plot,2,0);}
        } else if(w.kind==DashboardWidgetKind::Map){
            lv_label_set_text(title,"GPS / North up");
            view.basemap=createGpsBasemap(view.panel,0,24,width,height-46,view.mapViewport);
            view.plot=lv_line_create(view.basemap);lv_obj_set_style_line_color(view.plot,colour(0x00707d),0);lv_obj_set_style_line_width(view.plot,2,0);
            view.marker=makeLabel(view.basemap,LV_SYMBOL_GPS,width/2-8,(height-46)/2-8,16,cCyan);
            lv_obj_add_flag(view.marker,LV_OBJ_FLAG_HIDDEN);
            view.detail=makeLabel(view.panel,"Waiting for GPS",8,height-20,12,cDim);
        } else if(w.kind==DashboardWidgetKind::Model){
            lv_label_set_text(title,"Model");
            if(activeModelSlot_==0){
                // Fit the artwork between the title and name with an outer margin.
                constexpr int margin=10, imageTop=25, nameSpace=24;
                const int availableW=width-2*margin, availableH=height-imageTop-nameSpace;
                const int sourceW=hyper7_widget_icon.header.w, sourceH=hyper7_widget_icon.header.h;
                const int zoom=min(availableW*256/sourceW,availableH*256/sourceH);
                auto image=lv_img_create(view.panel);lv_img_set_src(image,&hyper7_widget_icon);
                lv_img_set_pivot(image,0,0);lv_img_set_zoom(image,zoom);
                lv_obj_set_pos(image,(width-sourceW*zoom/256)/2,
                               imageTop+(availableH-sourceH*zoom/256)/2);
            }
            else {auto icon=makeLabel(view.panel,LV_SYMBOL_IMAGE,0,0,28,cDim);lv_obj_center(icon);}
            auto name=makeLabel(view.panel,modelNames_[activeModelSlot_],0,0,12,cWhite);lv_obj_align(name,LV_ALIGN_BOTTOM_MID,0,-4);
        }
    }
}

void TransmitterUi::updateDashboardWidgets(const CrsfTelemetryState &t,uint16_t tx,bool txValid)
{
    const auto &layout=dashboardLayouts_[activeModelSlot_];
    for(uint8_t cell=0;cell<4;++cell){
        const auto &w=layout.cells[cell];auto &view=widgetViews_[cell];if(!view.panel)continue;
        if(w.kind==DashboardWidgetKind::Timer){
            uint32_t seconds=(runTimerElapsedMs_+(runTimerRunning_?millis()-runTimerStartedMs_:0))/1000;
            uiSetTextFmt(view.values[0],"%02lu:%02lu",static_cast<unsigned long>(seconds/60),static_cast<unsigned long>(seconds%60));
        } else if(w.kind==DashboardWidgetKind::Power || w.kind==DashboardWidgetKind::Channels){
            bool power=w.kind==DashboardWidgetKind::Power;
            const int count=power || w.secondSource<16?2:1;
            for(int j=0;j<count;++j){
                bool valid=power?(j?t.receiverVoltageValid:txValid):true;
                int value=power?(j?t.receiverVoltageDecivolts:tx):permilleFromCrsfValue(dashboardChannelValues_[j?w.secondSource:w.source]);
                bool low=power && valid && (j?(rxBatterySettings_.alarmEnabled && value<=rxBatterySettings_.lowAlarmDecivolts):value<=txBatterySettings_.lowAlarmDecivolts);
                if(!valid)uiSetText(view.values[j],"--");
                else if(power)uiSetTextFmt(view.values[j],"%d.%d%s",value/10,value%10,w.style==DashboardWidgetStyle::Dials?"":" V");
                else if(w.style==DashboardWidgetStyle::Switches)uiSetText(view.values[j],value>100?"HIGH":value< -100?"LOW":"MID");
                else uiSetTextFmt(view.values[j],"%+d%%",value/10);
                uiSetTextColor(view.values[j],colour(low?cRed:valid?cWhite:cDim),0);
                int maximum=1000;
                if(power){auto chem=j?rxBatterySettings_.chemistry:txBatterySettings_.chemistry;int cells=j?rxBatterySettings_.cellCount:txBatterySettings_.cellCount;maximum=cells*(chem==BatteryChemistry::NiMH?15:chem==BatteryChemistry::LiFe?37:42);}
                if(w.style==DashboardWidgetStyle::Graph && sensorHistoryCount_>=2){
                    auto *points=j?view.secondPoints:view.points;int ch=j?w.secondSource:w.source;
                    int width=lv_obj_get_width(view.panel),height=lv_obj_get_height(view.panel),count=min<int>(30,sensorHistoryCount_);
                    for(int i=0;i<count;++i)points[i]={static_cast<lv_coord_t>(8+i*(width-16)/(count-1)),static_cast<lv_coord_t>(37+(height-58)/2-permilleFromCrsfValue(channelHistory_[ch][30-count+i])*(height-58)/2000)};
                    lv_line_set_points(j?view.secondPlot:view.plot,points,count);
                }
                if(view.arcs[j]){lv_arc_set_range(view.arcs[j],power?0:-1000,maximum);lv_arc_set_value(view.arcs[j],valid?value:0);uiSetArcColor(view.arcs[j],colour(low?cRed:valid?cCyan:cDim),LV_PART_INDICATOR);}
                if(view.bars[j]){lv_bar_set_range(view.bars[j],power?0:-1000,maximum);lv_bar_set_value(view.bars[j],valid?value:0,LV_ANIM_OFF);uiSetBgColor(view.bars[j],colour(low?cRed:cCyan),LV_PART_INDICATOR);}
            }
        } else if(w.kind==DashboardWidgetKind::Sensor){
            int32_t value=0;uint32_t stamp=0;bool valid=sensorReading(t,w.source,value,stamp);char text[24];sensorText(w.source,value,text,sizeof(text));
            if(w.source!=0){uiSetText(view.values[0],valid?text:"--");uiSetTextFmt(view.detail,"%s%s",designSensorUnits[w.source],valid?"":stamp?" / stale":" / no data");}
            if(w.source==0){
                uiSetText(view.detail,"km/h");
                char readings[3][16];
                snprintf(readings[0],sizeof(readings[0]),"%s",valid?text:"--");
                for(uint8_t i=1;i<3;++i){
                    const uint16_t stat=i==1?speedStatistics_.maximum():speedStatistics_.average();
                    if(speedStatistics_.available())snprintf(readings[i],sizeof(readings[i]),"%u.%u",stat/10,stat%10);
                    else snprintf(readings[i],sizeof(readings[i]),"--");
                }
                const uint8_t primary=static_cast<uint8_t>(w.speedValue);
                uiSetText(view.values[0],readings[primary]);
                // Keep the remaining values in Current / Maximum / Average order.
                uint8_t secondary[2],count=0;
                for(uint8_t i=0;i<3;++i)if(i!=primary)secondary[count++]=i;
                uiSetTextFmt(view.values[1],"%s %s\n%s %s",
                    speedValueShortNames[secondary[0]],readings[secondary[0]],
                    speedValueShortNames[secondary[1]],readings[secondary[1]]);
            }
            if(view.plot){
                int start=30;while(start>0 && sensorHistoryValid_[w.source][start-1])--start;
                if(30-start<2){lv_obj_add_flag(view.plot,LV_OBJ_FLAG_HIDDEN);continue;}
                int32_t lo=INT32_MAX,hi=INT32_MIN;for(int i=start;i<30;++i){lo=min(lo,sensorHistory_[w.source][i]);hi=max(hi,sensorHistory_[w.source][i]);}
                int height=lv_obj_get_height(view.panel),width=lv_obj_get_width(view.panel);
                const int plotBottom=height-(w.source==0?43:24), plotHeight=max(1,plotBottom-23);
                for(int i=start;i<30;++i)view.points[i-start]={static_cast<lv_coord_t>(width/2+(i-start)*(width/2-10)/(29-start)),static_cast<lv_coord_t>(plotBottom-(static_cast<int64_t>(sensorHistory_[w.source][i])-lo)*plotHeight/max<int64_t>(1,static_cast<int64_t>(hi)-lo))};
                lv_line_set_points(view.plot,view.points,30-start);lv_obj_clear_flag(view.plot,LV_OBJ_FLAG_HIDDEN);
            }
        } else if(w.kind==DashboardWidgetKind::Map){
            bool fix=t.gpsValid && t.satellites>=4;int width=lv_obj_get_width(view.basemap),height=lv_obj_get_height(view.basemap);
            uiSetTextFont(view.detail,fontFor(10),0);
            uiSetText(view.detail,!gpsCount_?"(c) OpenStreetMap":!fix?"GPS stale / no fix":GpsMap::covers(t.latitudeE7,t.longitudeE7)?"(c) OpenStreetMap":"No local map");
            if(!gpsCount_)continue;
            double lonScale=cos(gpsLat_[gpsCount_-1]/1e7*3.141592653589793/180.0),extent=20;
            for(int i=0;i<gpsCount_;++i){double east=(static_cast<double>(gpsLon_[i])-gpsLon_[gpsCount_-1])*0.01113195*lonScale;double north=(static_cast<double>(gpsLat_[i])-gpsLat_[gpsCount_-1])*0.01113195;extent=max(extent,max(fabs(east),fabs(north)));}
            double scale=min(width-20,height-10)/2.0/extent;
            scale=max(0.15,min(2.0,scale));
            updateGpsBasemap(view.basemap,view.mapViewport,gpsLat_[gpsCount_-1],gpsLon_[gpsCount_-1],scale);
            for(int i=0;i<gpsCount_;++i){double east=(static_cast<double>(gpsLon_[i])-gpsLon_[gpsCount_-1])*0.01113195*lonScale;double north=(static_cast<double>(gpsLat_[i])-gpsLat_[gpsCount_-1])*0.01113195;view.points[i]={static_cast<lv_coord_t>(width/2+east*scale),static_cast<lv_coord_t>(height/2-north*scale)};}
            if(gpsCount_>1)lv_line_set_points(view.plot,view.points,gpsCount_);
            lv_obj_clear_flag(view.marker,LV_OBJ_FLAG_HIDDEN);uiSetTextColor(view.marker,colour(fix?0x00707d:0x53636c),0);
        }
    }
}
