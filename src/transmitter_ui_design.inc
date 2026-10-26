// Shared Drive / Tune / Data / Setup presentation. Included by the LVGL
// translation unit so all screens use one colour, font and focus system.
namespace {
const char *const designSensorNames[] = {"SPEED", "TEMPERATURE", "RPM", "RX VOLTAGE", "LINK QUALITY", "SATELLITES", "DRIVE PACK", "LOWEST CELL", "CELL 1", "CELL 2", "CELL 3", "CELL 4", "FAS VOLTAGE", "FAS CURRENT"};
const char *const speedValueNames[] = {"Current", "Maximum", "Average"};
const char *const speedValueTitles[] = {"SPEED", "MAX SPEED", "AVG SPEED"};
const char *const speedValueShortNames[] = {"Now", "Max", "Avg"};
const char *const designSensorUnits[] = {"km/h", "C", "rpm", "V", "%", "sats", "V", "V", "V", "V", "V", "V", "V", "A"};
static_assert(sizeof(designSensorNames)/sizeof(*designSensorNames)==DashboardSensorCount, "Sensor names must cover persisted sources");
static_assert(sizeof(designSensorUnits)/sizeof(*designSensorUnits)==DashboardSensorCount, "Sensor units must cover persisted sources");
const char *const designWidgetNames[] = {"Run timer", "Speed", "Temperature", "RPM", "RX voltage", "Link quality", "Satellites", "GPS trail"};
const char *const designFieldNames[] = {"Rate", "Expo", "Endpoint", "Response speed", "Direction", "Trim", "Subtrim", "Response offset"};
const char *const designRoleNames[] = {"Steering", "Throttle", "Brake", "Channel", "Channel"};
const char *dashboardKindName(DashboardWidgetKind kind) {
    const char *names[]={"Empty","Timer","TX / RX power","Channels","Sensor","GPS map / trail","Model image","All channels","Battery cells"};
    return names[static_cast<uint8_t>(kind)<static_cast<uint8_t>(DashboardWidgetKind::Count)?static_cast<uint8_t>(kind):0];
}
const char *dashboardStyleName(DashboardWidgetStyle style) {
    const char *names[]={"Numbers","Bars","Dials","Graph","Switches","Vertical bars"};
    return names[static_cast<uint8_t>(style)<static_cast<uint8_t>(DashboardWidgetStyle::Count)?static_cast<uint8_t>(style):0];
}
const bool &designPortrait = portraitUi;
int designFooterY = kScreenHeight - (designPortrait ? 60 : 40);

bool sensorReading(const CrsfTelemetryState &t, uint8_t sensor, int32_t &value, uint32_t &stamp)
{
    switch (sensor) {
        case 0: value = t.groundSpeedDeciKmh; stamp = t.gpsUpdatedMs; return t.gpsValid && t.satellites >= 4 && uint32_t(millis()-stamp)<=SpeedStatistics::FreshnessMs;
        case 1: value = t.temperatureDeciC; stamp = t.temperatureUpdatedMs; return t.temperatureValid;
        case 2: value = t.rpm; stamp = t.rpmUpdatedMs; return t.rpmValid;
        case 3: value = t.receiverVoltageDecivolts; stamp = t.batteryUpdatedMs; return t.receiverVoltageValid;
        case 4: value = t.uplinkLinkQuality; stamp = t.linkUpdatedMs; return t.linkValid;
        case 5: value = t.satellites; stamp = t.gpsUpdatedMs; return t.gpsValid;
        case 6: value = t.vehiclePackMv; stamp = t.vehicleCellsUpdatedMs; return t.vehicleCellsValid;
        case 7: value = t.vehicleMinimumCellMv; stamp = t.vehicleCellsUpdatedMs; return t.vehicleCellsValid;
        case 8: case 9: case 10: case 11:
            value = t.vehicleCellsMv[sensor-8]; stamp = t.vehicleCellsUpdatedMs; return t.vehicleCellsValid && sensor-8 < t.vehicleCellCount;
        case 12: value = t.fasVoltageMv; stamp = t.fasVoltageUpdatedMs; return t.fasVoltageValid;
        case 13: value = t.fasCurrentDeciA; stamp = t.fasCurrentUpdatedMs; return t.fasCurrentValid;
        default: value = 0; stamp = 0; return false;
    }
}
void sensorText(uint8_t sensor, int32_t value, char *text, size_t capacity)
{
    if (sensor >= 6 && sensor <= 12)
        snprintf(text, capacity, "%ld.%03ld", static_cast<long>(value / 1000), static_cast<long>(value % 1000));
    else if (sensor == 0 || sensor == 1 || sensor == 3 || sensor == 13)
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
        case UiPage::OutputStatus: case UiPage::ModeChooser: case UiPage::ChannelPlaceholder:
        case UiPage::ChannelInput: case UiPage::ChannelResponseSetup: case UiPage::ParameterControlPicker: case UiPage::ResponseProfile: case UiPage::MixingOverview:
        case UiPage::NetworkOptions: case UiPage::Setup: case UiPage::SystemMenu: case UiPage::DriveModes:
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
    const bool negative = tuneRole_ == 2 || tuneRole_ == 4;
    switch (editorField_) {
        case 0: return negative ? p.negativeRatePercent : p.positiveRatePercent;
        case 1: return negative ? p.negativeExpoPercent : p.positiveExpoPercent;
        case 2: return negative ? p.negativeEndpointPercent : p.positiveEndpointPercent;
        case 3: return negative ? p.negativeRampMs : p.positiveRampMs;
        case 4: return p.reversed;
        case 5: return channelParameterValue(selectedChannel_, ChannelParameterKind::Trim) / 10;
        case 6: return channelParameterValue(selectedChannel_, ChannelParameterKind::Subtrim) / 10;
        case 7: return p.subtrimPercent;
        default: return 0;
    }
}

void TransmitterUi::applyDesignValue(int value)
{
    if (editorField_ == 7) {
        auto profile = profileForChannel(selectedChannel_); profile.subtrimPercent = value;
        channelModel_.setResponse(selectedChannel_, profile); scheduleSettingsSave(); return;
    }
    if (editorField_ >= 5) {
        applyChannelParameterAction(encodeChannelParameterTarget(selectedChannel_,
            editorField_ == 5 ? ChannelParameterKind::Trim : ChannelParameterKind::Subtrim), value * 10);
        return;
    }
    auto p = profileForChannel(selectedChannel_);
    if (editorField_ == 2 && tuneRole_ == 0) {
        p.negativeEndpointPercent = steeringEndpointDrafts_[0];
        p.positiveEndpointPercent = steeringEndpointDrafts_[1];
        channelModel_.setResponse(selectedChannel_, p);
        scheduleSettingsSave();
        return;
    }
    // Other steering edits remain symmetric; throttle and brake are independent.
    const bool positive = tuneRole_ != 2 && tuneRole_ != 4;
    const bool negative = tuneRole_ == 0 || tuneRole_ == 2 || tuneRole_ == 4;
    switch (editorField_) {
        case 0: if (positive) p.positiveRatePercent = value; if (negative) p.negativeRatePercent = value; break;
        case 1: if (positive) p.positiveExpoPercent = value; if (negative) p.negativeExpoPercent = value; break;
        case 2: if (positive) p.positiveEndpointPercent = value; if (negative) p.negativeEndpointPercent = value; break;
        case 3: if (positive) p.positiveRampMs = value; if (negative) p.negativeRampMs = value; break;
        case 4: p.reversed = value != 0; break;
    }
    channelModel_.setResponse(selectedChannel_, p);
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

void TransmitterUi::updateGpsPresentation(uint32_t now)
{
    if(activePage_==UiPage::Gps&&!gpsFullScreen_&&!touchWasDown_&&now-lastInteractionMs_>=4000){
        gpsFullScreen_=true;drawDesignPage(UiPage::Gps);
    }
}
void TransmitterUi::drawGpsMapView(bool fullScreen)
{
    const int x=fullScreen?0:10,y=fullScreen?0:84;
    const int width=fullScreen?kScreenWidth:kScreenWidth-20;
    const int height=fullScreen?kScreenHeight:designFooterY-y-4;
    designMapViewport_=GpsMapViewport{};
    auto map=createGpsBasemap(lv_scr_act(),x,y,width,height,designMapViewport_);
    designCurve_=lv_line_create(map);lv_obj_set_style_line_color(designCurve_,colour(0x00707d),0);lv_obj_set_style_line_width(designCurve_,2,0);
    designMarker_=makeLabel(map,LV_SYMBOL_GPS,0,0,18,cCyan);lv_obj_add_flag(designMarker_,LV_OBJ_FLAG_HIDDEN);
    makeLabel(map,"N ^",8,fullScreen?6:27,12,0x405442);
    designDetails_[1]=makeLabel(map,"",width-70,fullScreen?6:27,10,0x405442);
    if(!fullScreen){
        auto status=makePanel(x+4,y+4,width-8,19,cPanel,cPanel,0);
        lv_obj_set_style_bg_opa(status,LV_OPA_90,0);
        designDetails_[0]=makeLabel(status,"Waiting for GPS telemetry",5,2,12,cWhite);
        boundedLabel(designDetails_[0],width-18);
        const int controlsY=y+height-50,left=x+width-220;
        makePageActionButton(left,controlsY,32,30,"-",18,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,82);
        makePageActionButton(left+36,controlsY,32,30,"+",18,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,83);
        makePageActionButton(left+72,controlsY,76,30,gpsTrailVisible_?"Trail on":"Trail off",12,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,84);
        makePageActionButton(left+152,controlsY,68,30,"Reset",12,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,85);
    }
}

void TransmitterUi::drawDesignPage(UiPage page)
{
    if(page==UiPage::Gps&&activePage_!=UiPage::Gps){gpsFullScreen_=false;lastInteractionMs_=millis();}
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
    if(page==UiPage::Gps&&gpsFullScreen_){
        drawGpsMapView(true);finishPageFocus();
        updateDesign(designTelemetry_,designSafety_,designTxVoltage_,designTxValid_);return;
    }
    makePanel(0,0,kScreenWidth,38,cHeader,cHeader,0);
    auto logo = lv_img_create(lv_scr_act());
    if (const auto *art=modelImage(modelImages_[activeModelSlot_],modelColours_[activeModelSlot_],modelAccents_[activeModelSlot_],modelBases_[activeModelSlot_])) {
        lv_img_set_src(logo,art); lv_img_set_zoom(logo,min(36*256/int(art->header.w),30*256/int(art->header.h)));
        lv_img_set_pivot(logo,0,0); lv_obj_set_pos(logo,5,4);
    } else { lv_obj_del(logo); label(LV_SYMBOL_IMAGE,10,10,18,cDim); }
    auto modelLabel = label(modelNames_[activeModelSlot_],47,10,14,0xe7edef);
    boundedLabel(modelLabel,kScreenWidth-155);
    addPageActionTarget(0,0,kScreenWidth-110,38,&TransmitterUi::controlDesign,4);
    designLink_ = label("LQ --",kScreenWidth-104,12,12,cDim);
    boundedLabel(designLink_,60);
    makeWifiStatusIcon(kScreenWidth-33,11);
    addPageActionTarget(kScreenWidth-110,0,72,38,&TransmitterUi::controlDesign,63);
    const bool data = page == UiPage::Telemetry || page == UiPage::Gps || page == UiPage::ChannelMonitor || page == UiPage::SensorDetail;
    const UiPage sectionPage=page==UiPage::OutputStatus?statusReturnPage_:page;
    const bool dataSection=sectionPage==UiPage::Telemetry||sectionPage==UiPage::Gps||sectionPage==UiPage::ChannelMonitor||sectionPage==UiPage::SensorDetail;
    const uint8_t nav=(sectionPage==UiPage::Dashboard||sectionPage==UiPage::ModeChooser)?0:(sectionPage==UiPage::Tune||sectionPage==UiPage::ValueEditor)?1:dataSection?2:3;
    const char *const navNames[] = {"Drive","Tune","Data","Setup"};
    if (page != UiPage::ValueEditor) {
        for (uint8_t i=0;i<4;++i) {
            auto tab=button(i*kScreenWidth/4,designFooterY,kScreenWidth/4,40,navNames[i],i,i==nav);
            lv_obj_set_style_text_font(lv_obj_get_child(tab,0),fontFor(16),0);
            if(i==0){
                designStatus_=lv_obj_get_child(tab,0);lv_obj_align(designStatus_,LV_ALIGN_CENTER,0,0);
                lv_obj_set_style_text_font(designStatus_,fontFor(16),0);
                driveIcon_=makeDriveIcon(tab,0,0);lv_obj_align(driveIcon_,LV_ALIGN_LEFT_MID,4,0);
                driveProgress_=makeBar(tab,0,37,kScreenWidth/4,3,0,800);
            }
            if(i==nav){auto mark=lv_obj_create(tab);position(mark,0,35,kScreenWidth/4,i==0?2:5);lv_obj_set_style_bg_color(mark,colour(cCyan),0);lv_obj_set_style_border_width(mark,0,0);}
        }
    }
    if (page == UiPage::Dashboard) {
        label("Drive",12,48,22);
        char mode[24];snprintf(mode,sizeof(mode),driveModeSettings_.activeMode==DriveMode::Walk?"Walk %u%%":"Normal",driveModeSettings_.walkLimitPercent);
        button(kScreenWidth-128,44,116,32,mode,6);
        addPageActionTarget(8,42,100,36,&TransmitterUi::controlDesign,180);
        drawDashboardWidgets();
    } else if(page==UiPage::OutputStatus) {
        label("Output status",12,48,22);
        designDetails_[0]=label("",12,94,18,cCyan);boundedLabel(designDetails_[0],kScreenWidth-24);
        designDetails_[1]=label("",12,designPortrait?138:126,14,cWhite);boundedLabel(designDetails_[1],kScreenWidth-24);
        label("Physical handset only. Unlocking never arms.",12,designPortrait?190:158,12,cDim);
        auto arm=button(12,designPortrait?238:184,kScreenWidth-24,48,"Enable output",181);
        armProgress_=makeBar(arm,4,41,kScreenWidth-32,4,0,800);
        button(12,designFooterY-42,100,34,"Back",209);
    } else if(page==UiPage::ModeChooser || page==UiPage::DriveModes) {
        label(page==UiPage::ModeChooser?"Choose mode":"Drive modes",12,48,22);
        char walk[24];snprintf(walk,sizeof(walk),"Walk %u%%",driveModeSettings_.walkLimitPercent);
        button(12,96,(kScreenWidth-32)/2,48,"Normal",183,driveModeSettings_.activeMode==DriveMode::Normal);
        button(kScreenWidth/2+4,96,(kScreenWidth-32)/2,48,walk,182,driveModeSettings_.activeMode==DriveMode::Walk);
        label("Mode changes CUT output. Re-arm at neutral.",12,designPortrait?174:154,12,cAmber);
        if(page==UiPage::ModeChooser)button(12,designPortrait?224:184,kScreenWidth-24,40,"Mode settings",184);
        else {
            label("Walk forward limit",12,designPortrait?228:185,14);
            button(kScreenWidth-120,designPortrait?222:176,48,40,"-",219);button(kScreenWidth-64,designPortrait?222:176,48,40,"+",220);
            label("Brake / reverse retain full range.",12,designPortrait?296:230,12,cDim);
        }
        button(12,designFooterY-42,100,34,"Back",209);
    } else if(page==UiPage::ChannelPlaceholder) {
        char title[40];snprintf(title,sizeof(title),"CH%u - %s",selectedChannel_,selectedChannel_==steeringChannel()?"Steering":selectedChannel_==throttleChannel()?"Throttle":"Auxiliary");label(title,12,48,22);
        const char *names[]={"Role / physical input","Response / trim","Mixing / output limits"};
        for(int i=0;i<3;++i)row(90+i*(designPortrait?72:44),names[i],"",186+i,designPortrait?62:40);
        button(12,designFooterY-42,100,34,"Back",209);
    } else if(page==UiPage::ChannelInput) {
        char title[36];snprintf(title,sizeof(title),"CH%u input",selectedChannel_);label(title,12,48,22);
        const bool steering=selectedChannel_==steeringChannel(),throttle=selectedChannel_==throttleChannel();
        if(!channelInputPage_) {
            row(90,"Role",steering?"Steering":throttle?"Throttle":"Auxiliary",189,designPortrait?58:34);
            row(designPortrait?158:126,"Physical control",steering?"Steering wheel":throttle?"Throttle trigger":"Assignments",190,designPortrait?58:34);
            row(designPortrait?226:162,"Input enabled",steering?(steeringEnabled()?"On":"Off"):throttle?(throttleEnabled()?"On":"Off"):"Per assignment",191,designPortrait?58:34);
            row(designPortrait?294:198,"Calibration / deadband",channelHasProportionalControl(selectedChannel_)?"Configure":"Not applicable",192,designPortrait?58:34);
        }else{
            button(12,90,kScreenWidth-24,44,"Assign steering to this channel",193);
            button(12,144,kScreenWidth-24,44,"Assign throttle to this channel",194);
            label(channelRoleConflict_?"Channel already owned; choose a free channel.":"Moves the role from its previous channel; forces CUT.",12,designPortrait?222:202,12,channelRoleConflict_?cAmber:cDim);
            boundedLabel(label("One channel per role. Auxiliary channels use control assignments.",12,designPortrait?264:224,12,cDim),kScreenWidth-24);
        }
        button(12,designFooterY-42,100,34,"Back",209);
    } else if(page==UiPage::ChannelResponseSetup) {
        char title[36];snprintf(title,sizeof(title),"CH%u response",selectedChannel_);label(title,12,48,22);
        const char *names[]={"Quick adjustments","Curve / detailed response","Trim control","Subtrim control"};
        for(int i=0;i<4;++i){char value[48]="";
            if(i>=2){auto kind=i==2?ChannelParameterKind::Trim:ChannelParameterKind::Subtrim;int index=bindingIndexForChannelParameter(selectedChannel_,kind);snprintf(value,sizeof(value),"%s",index>=0?physicalControlDescriptor(static_cast<size_t>(bindings_[index].input)).name:"None");}
            row(88+i*(designPortrait?68:38),names[i],value,195+i,designPortrait?58:36);
        }
        button(12,designFooterY-42,100,34,"Back",209);
    } else if(page==UiPage::ResponseProfile) {
        char title[32];snprintf(title,sizeof(title),"CH%u response",selectedChannel_);label(title,12,48,22);
        const bool steering=selectedChannel_==steeringChannel(),throttle=selectedChannel_==throttleChannel();
        button(12,82,kScreenWidth/2-20,34,steering?"Right":throttle?"Forward":"Positive",231,tuneRole_!=4);
        button(kScreenWidth/2+4,82,kScreenWidth/2-16,34,steering?"Left":throttle?"Brake":"Negative",232,tuneRole_==4);
        const uint8_t fields[2][3]={{0,1,2},{3,4,7}};
        for(uint8_t i=0;i<3;++i){
            const auto field=fields[responseDetailBank_][i];editorField_=field;char value[32];
            if(field==4)snprintf(value,sizeof(value),"%s",designValue()?"Reversed":"Normal");
            else snprintf(value,sizeof(value),"%d%s",designValue(),field==3?" ms":"%");
            row(126+i*(designPortrait?68:36),designFieldNames[field],value,233+i,designPortrait?58:34);
        }
        button(12,designFooterY-42,100,34,"Back",209);
        button(kScreenWidth-152,designFooterY-42,140,34,responseDetailBank_?"< Basic":"More >",236);
    } else if(page==UiPage::ParameterControlPicker) {
        char title[48];snprintf(title,sizeof(title),"CH%u / %s control",selectedChannel_,parameterControlKind_==ChannelParameterKind::Trim?"trim":"subtrim");
        label(title,12,48,18);
        for(uint8_t i=0;i<6;++i){
            auto input=kParameterControls[i];const int assigned=input==PhysicalControlId::Count?-1:bindingIndexForInput(input);
            const int current=bindingIndexForChannelParameter(selectedChannel_,parameterControlKind_);
            const bool busy=assigned>=0&&assigned!=current;
            const int columns=designPortrait?1:2,w=(kScreenWidth-24-(columns-1)*8)/columns;
            const int x=12+(i%columns)*(w+8),y=(designPortrait?100:94)+(i/columns)*44;
            auto choice=button(x,y,w,40,input==PhysicalControlId::Count?"None":physicalControlDescriptor(static_cast<size_t>(input)).name,224+i,input==parameterControlDraft_);
            lv_obj_set_style_text_font(lv_obj_get_child(choice,0),fontFor(12),0);
            if(busy)uiSetTextColor(lv_obj_get_child(choice,0),colour(cDim),0);
        }
        boundedLabel(label(parameterControlError_?"Control is already assigned; choose a free one.":"Grey controls are in use. Changes wait for Apply.",12,designFooterY-56,10,parameterControlError_?cAmber:cDim),kScreenWidth-24);
        button(12,designFooterY-42,100,34,"Cancel",209);
        button(kScreenWidth-124,designFooterY-42,112,34,"Apply",230,true);
    } else if(page==UiPage::MixingOverview) {
        char title[36];snprintf(title,sizeof(title),"CH%u mixing",selectedChannel_);label(title,12,48,22);
        const char *names[]={"Mix sources","Activation","Switched rates","Baseline / limits","Preview / graph"};
        for(int i=0;i<5;++i)row(83+i*(designPortrait?57:30),names[i],"",199+i,designPortrait?50:28);
        button(12,designFooterY-42,100,34,"Cancel",209);button(kScreenWidth-124,designFooterY-42,112,34,mixingSaveError_?"Retry apply":"Apply",204);
    } else if(page==UiPage::Setup) {
        label("Sounds / alerts",12,48,22);
        char pitch[24],length[24];snprintf(pitch,sizeof(pitch),"%u Hz",systemSettings_.buzzerPitchHz);snprintf(length,sizeof(length),"%u ms",systemSettings_.keyClickDurationMs);
        const char *names[]={"Buzzer","Key clicks","Pitch","Click length"};const char *values[]={systemSettings_.buzzerEnabled?"On":"Off",systemSettings_.keyClicksEnabled?"On":"Off",pitch,length};
        for(int i=0;i<4;++i)row(85+i*(designPortrait?64:36),names[i],values[i],213+i,designPortrait?56:34);
        button(kScreenWidth-124,designFooterY-42,112,34,"Test sound",217);button(12,designFooterY-42,100,34,"Back",209);
    } else if(page==UiPage::NetworkOptions || page==UiPage::SystemMenu) {
        const bool network=page==UiPage::NetworkOptions;label(network?"Network / web":"System",12,48,22);
        if(network){row(94,"Home Wi-Fi","Connection",210,designPortrait?64:44);row(designPortrait?170:148,"Web access","Address / credentials",211,designPortrait?64:44);}
        else{row(designPortrait?90:82,"Input diagnostics","Monitor",48,designPortrait?60:34);row(designPortrait?162:120,"Firmware information","",144,designPortrait?60:34);row(designPortrait?234:158,"PIN","Change / recovery",237,designPortrait?60:34);row(designPortrait?306:196,"Restart handset","",212,designPortrait?60:34);}
        button(12,designFooterY-42,100,34,"Back",209);
    } else if (page == UiPage::Tune) {
        if (tuneRole_ < 3) selectedChannel_ = tuneRole_ == 0 ? channelRoleSettings_.steeringChannel : channelRoleSettings_.throttleChannel;
        if(tuneRole_<3)for (uint8_t i=0;i<3;++i) button(10+i*(kScreenWidth-20)/3,44,(kScreenWidth-20)/3,34,designRoleNames[i],10+i,tuneRole_==i);
        else {button(10,44,(kScreenWidth-20)/2,34,"Positive",221,tuneRole_==3);button(kScreenWidth/2,44,(kScreenWidth-20)/2-10,34,"Negative",222,tuneRole_==4);}
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
        char heading[48];
        if(editorReturn_==UiPage::ResponseProfile)snprintf(heading,sizeof(heading),"CH%u / %s",selectedChannel_,designFieldNames[editorField_]);
        else snprintf(heading,sizeof(heading),"%s / %s",designRoleNames[tuneRole_],designFieldNames[editorField_]);
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
        const bool pos=tuneRole_!=2&&tuneRole_!=4, neg=tuneRole_==0 || tuneRole_==2 || tuneRole_==4;
        switch(editorField_) {
            case 0: if(pos)draft.positiveRatePercent=editorDraft_;if(neg)draft.negativeRatePercent=editorDraft_;break;
            case 1: if(pos)draft.positiveExpoPercent=editorDraft_;if(neg)draft.negativeExpoPercent=editorDraft_;break;
            case 2:
                if(steeringEndpoints){draft.negativeEndpointPercent=steeringEndpointDrafts_[0];draft.positiveEndpointPercent=steeringEndpointDrafts_[1];}
                else {if(pos)draft.positiveEndpointPercent=editorDraft_;if(neg)draft.negativeEndpointPercent=editorDraft_;}
                break;
            case 4: draft.reversed=editorDraft_!=0;break;
            case 7: draft.subtrimPercent=editorDraft_;break;
        }
        for(int i=0;i<33;++i) {
            int input=-1000+i*2000/32;
            int output=applyChannelResponse(draft,input);
            designPoints_[i]={static_cast<lv_coord_t>(4+i*(kScreenWidth-34)/32),static_cast<lv_coord_t>(graphH/2-output*(graphH-8)/2400)};
        }
        lv_line_set_points(designCurve_,designPoints_,33);
        lv_obj_set_style_line_color(designCurve_,colour(cCyan),0); lv_obj_set_style_line_width(designCurve_,2,0);
        if(editorField_==3 || (editorField_>=5&&editorField_<=6)) { lv_obj_add_flag(designCurve_,LV_OBJ_FLAG_HIDDEN); makeLabel(graph,editorField_==3 ? "Time to reach requested output" : "Offset from channel neutral",10,graphH/2-7,12,cDim); }
        if (steeringEndpoints) label("Steer or tap a side, then use + / -",12,designPortrait?282:190,10,cDim);
        const int y=designPortrait ? 302 : 204, w=(kScreenWidth-44)/4;
        button(10,y,w,38,"-",30); button(18+w,y,w,38,editorFine_ ? "Fine" : "Coarse",32);
        button(26+w*2,y,w,38,"+",31); button(34+w*3,y,w,38,"Reset",33);
        boundedLabel(label(editorField_==7?"Response offset is separate from channel trim / subtrim.":"Changes take effect only after Apply",12,designPortrait ? 352 : 249,12,cDim),kScreenWidth-24);
        button(10,designFooterY,kScreenWidth/2-15,36,"Cancel",34);
        button(kScreenWidth/2+5,designFooterY,kScreenWidth/2-15,36,"Apply",35,true);
    } else if (data) {
        const char *tabs[]={"Sensors","GPS","Channels","Inputs"};
        for(uint8_t i=0;i<4;++i)button(10+i*(kScreenWidth-20)/4,44,(kScreenWidth-20)/4,34,tabs[i],i==3?223:60+i,(i==0 && page!=UiPage::Gps && page!=UiPage::ChannelMonitor)||(i==1 && page==UiPage::Gps)||(i==2 && page==UiPage::ChannelMonitor));
        if(page==UiPage::Telemetry) {
            label(sensorBank_==2 ? "FAS300 ADV  3 / 3" : sensorBank_ ? "Drive battery  2 / 3" : "Radio / motion  1 / 3",12,88,12,cCyan);
            button(kScreenWidth-90,82,36,26,LV_SYMBOL_LEFT,80);
            button(kScreenWidth-48,82,36,26,LV_SYMBOL_RIGHT,81);
            const int cols=designPortrait?2:3, w=(kScreenWidth-20+kPanelGap)/cols, h=(designFooterY-120+kPanelGap)/(6/cols);
            for(uint8_t i=0;i<6;++i) {
                const uint8_t sensor=sensorBank_*6+i;
                if(sensor>=DashboardSensorCount)break;
                int x=10+i%cols*w,y=114+i/cols*h;
                auto panel=makePanel(x,y,w-kPanelGap,h-kPanelGap,cPanel,cLine,0);
                makeLabel(panel,designSensorNames[sensor],8,9,12,cDim);
                designValues_[i]=makeLabel(panel,"--",8,30,24);
                makeLabel(panel,designSensorUnits[sensor],8,58,12,cDim);
                designDetails_[i]=makeLabel(panel,"No data",w-78,58,12,cDim);
                addPageActionTarget(x,y,w-kPanelGap,h-kPanelGap,&TransmitterUi::controlDesign,70+i);
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
            drawGpsMapView(false);
        }
    } else if(page==UiPage::MainMenu || page==UiPage::BatteryMenu) {
        label(page==UiPage::BatteryMenu?"Batteries":"Setup",12,48,22);
        auto menuRow=[&](int y,const char *name,const char *value,uint8_t id){
            const int h=designPortrait?(setupBank_?52:59):(setupBank_?32:38);
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
            const char *names[2][8]={{"Channels","Control map","Dashboard","Drive modes","RF module","RX battery","ESC settings",""},{"Display","Sounds / alerts","TX battery","Network / web","System","","",""}};
            const uint8_t ids[2][8]={{41,42,49,43,44,146,46,0},{142,47,145,205,206,0,0,0}};
            const char *values[2][8]={{"CH1-16","","",driveModeSettings_.activeMode==DriveMode::Walk?"Walk":"Normal","ELRS","","Preview only",""},{"","","","","","","",""}};
            const uint8_t section=setupBank_?1:0,total=section?5:7;
            for(int i=0;i<4;++i){int n=setupPage_*4+i;if(n>=total)break;menuRow(85+i*(designPortrait?67:38),names[section][n],values[section][n],ids[section][n]);}
            button(10,designFooterY-34,60,28,"<",207);button(kScreenWidth-70,designFooterY-34,60,28,">",208);
            char position[16];snprintf(position,sizeof(position),"%u / 2",setupPage_+1);label(position,kScreenWidth/2-20,designFooterY-28,12,cDim);
        }
    } else if(page==UiPage::Channels) {
        label("Channels",12,48,22);
        for(int i=0;i<4;++i){int ch=designBank_*4+i+1;char name[36];snprintf(name,sizeof(name),"CH%u  %s",ch,ch==channelRoleSettings_.steeringChannel?"Steering":ch==channelRoleSettings_.throttleChannel?"Throttle":"Auxiliary");row(86+i*(designPortrait?67:40),name,"Configure",90+i,designPortrait?56:37);}
        button(10,designFooterY-34,60,28,"<",80);button(kScreenWidth-70,designFooterY-34,60,28,">",81);
    } else if(page==UiPage::ModelSelect) {
        label("Models",12,48,22);
        if(modelAppearanceEditing_){
            label(modelImageName(modelImages_[activeModelSlot_]),12,85,16,cCyan);
            button(10,113,70,32,"<",161);button(designPortrait?kScreenWidth-80:160,113,70,32,">",162);
            if(const auto *art=modelImage(modelImages_[activeModelSlot_],modelColours_[activeModelSlot_],modelAccents_[activeModelSlot_],modelBases_[activeModelSlot_])){
                auto image=lv_img_create(lv_scr_act());lv_img_set_src(image,art);
                const int zoom=min(200*256/int(art->header.w),100*256/int(art->header.h));
                lv_img_set_pivot(image,0,0);lv_img_set_zoom(image,zoom);
                lv_obj_set_pos(image,(kScreenWidth-art->header.w*zoom/256)/2,150);
                if(!designPortrait)lv_obj_add_flag(image,LV_OBJ_FLAG_HIDDEN);
            }
            auto &paint=modelPaintEditing_==0?modelColours_[activeModelSlot_]:modelPaintEditing_==1?modelAccents_[activeModelSlot_]:modelBases_[activeModelSlot_];
            char value[32];if(paint==kModelOriginalColour)snprintf(value,sizeof(value),"Original colour");
            else snprintf(value,sizeof(value),"Colour #%06lX",static_cast<unsigned long>(paint));
            const int paintX=designPortrait?10:kScreenWidth/2,paintW=designPortrait?kScreenWidth-20:kScreenWidth/2-10;
            button(paintX,designFooterY-150,paintW,32,modelPaintEditing_==0?"Primary paint":modelPaintEditing_==1?"Accent paint":"Base paint",164);
            button(paintX,designFooterY-112,paintW,32,value,163);
            button(paintX,designFooterY-74,paintW,32,"Original colours",165);
            button(paintX,designFooterY-38,paintW,32,"Done",160);
            finishPageFocus();
            updateDesign(designTelemetry_,designSafety_,designTxVoltage_,designTxValid_);
            uiMetrics.buildUs = uint32_t(micros()) - uiMetrics.pageStartedUs;
            return;
        }
        for(int i=0;i<5;++i){int slot=modelSelectBank_*5+i;row(78+i*(designPortrait?56:29),modelNames_[slot],slot==activeModelSlot_?"Active":"Load",100+i,designPortrait?48:29);}
        boundedLabel(label(modelSelectStatus_[0]?modelSelectStatus_:"Change model in CUT with throttle centred",12,designFooterY-49,12,cDim),kScreenWidth-24);
        button(10,designFooterY-32,60,27,"<",80);button(78,designFooterY-32,kScreenWidth-156,27,"Appearance",160);button(kScreenWidth-70,designFooterY-32,60,27,">",81);
    } else if(page==UiPage::WidgetSelect) {
        label("Drive layout",12,48,22);
        const char *positions[]={"Top left","Top right","Bottom left","Bottom right"};
        for(int i=0;i<4;++i){
            const auto &w=dashboardLayouts_[activeModelSlot_].cells[i];
            char description[40];
            bool covered=w.kind!=DashboardWidgetKind::Empty && !(visibleDashboardWidgets(dashboardLayouts_[activeModelSlot_])&(1<<i));
            snprintf(description,sizeof(description),"%s%s",dashboardKindName(w.kind),covered?" (hidden)":"");
            row(86+i*(designPortrait?65:39),positions[i],description,110+i,designPortrait?56:36);
        }
        if(designPortrait)button(10,designFooterY-48,300,40,"Use suggested layout",147);
        else label("Select a position to add or edit a widget",12,designFooterY-25,12,cDim);
    } else if(page==UiPage::WidgetEditor) {
        if(widgetInputsEditing_){
            label("Visible inputs",12,47,20);
            uint8_t entries[DashboardMonitorMaxEntries];
            const int total=dashboardMonitorEntries(0xffff,widgetDraft_.source,entries);
            const int pages=(total+4)/5;widgetInputsBank_%=pages;
            for(int i=0;i<5&&widgetInputsBank_*5+i<total;++i){
                const uint8_t entry=entries[widgetInputsBank_*5+i];char name[32];
                if(!(entry&0x80))snprintf(name,sizeof(name),"CH%u%s",entry+1,(dashboardEnabledChannelMask_&(1U<<entry))?"":" (unconfigured)");
                else snprintf(name,sizeof(name),"%s",entry==(0x80|3)?"Drive mode":entry==(0x80|9)?"Idle up":entry==(0x80|2)?"Timer":entry==(0x80|4)?"Grip A":"Grip B");
                row(77+i*(designPortrait?51:30),name,(widgetHiddenDraft_&dashboardMonitorBit(entry))?"Hide":"Show",170+i,designPortrait?47:28);
            }
            const int y=designFooterY-(designPortrait?78:50);
            button(10,y,45,24,"<",175);button(60,y,65,24,"All",178);button(130,y,65,24,"None",179);button(kScreenWidth-55,y,45,24,">",176);
            char pageText[32];snprintf(pageText,sizeof(pageText),"Page %u/%u",widgetInputsBank_+1,pages);
            label(pageText,12,designFooterY-22,12,cDim);
            button(kScreenWidth-110,designFooterY-25,100,25,"Done",177,true);
        }else{
        label("Widget settings",12,47,20);
        const bool speed=widgetDraft_.kind==DashboardWidgetKind::Sensor && widgetDraft_.source==0;
        const char *names[]={"Type","Size","Style",widgetDraft_.kind==DashboardWidgetKind::BatteryCells?"Cell count":"Source",speed?"Large value":"Second channel"};
        char values[5][32];
        snprintf(values[0],32,"%s",dashboardKindName(widgetDraft_.kind));
        snprintf(values[1],32,"%u x %u",widgetDraft_.width,widgetDraft_.height);
        snprintf(values[2],32,"%s",dashboardStyleName(widgetDraft_.style));
        if(widgetDraft_.kind==DashboardWidgetKind::AllChannels && widgetDraft_.style==DashboardWidgetStyle::Bars)snprintf(values[2],32,"Horizontal bars");
        if(widgetDraft_.kind==DashboardWidgetKind::Sensor)snprintf(values[3],32,"%s",designSensorNames[widgetDraft_.source%DashboardSensorCount]);
        else if(widgetDraft_.kind==DashboardWidgetKind::BatteryCells){if(widgetDraft_.source)snprintf(values[3],32,"%uS",widgetDraft_.source);else snprintf(values[3],32,"Auto (telemetry)");}
        else if(widgetDraft_.kind==DashboardWidgetKind::AllChannels)snprintf(values[3],32,"%s",widgetDraft_.source==1?"Hardware inputs":widgetDraft_.source==2?"Channels + inputs":"Channels");
        else snprintf(values[3],32,"CH%u",widgetDraft_.source+1);
        if(widgetDraft_.secondSource==16)snprintf(values[4],32,"None");else snprintf(values[4],32,"CH%u",widgetDraft_.secondSource+1);
        if(speed)snprintf(values[4],32,"%s",speedValueNames[static_cast<uint8_t>(widgetDraft_.speedValue)]);
        for(int i=0;i<5;++i){
            if(i>=3 && widgetDraft_.kind!=DashboardWidgetKind::Sensor && widgetDraft_.kind!=DashboardWidgetKind::Channels && widgetDraft_.kind!=DashboardWidgetKind::AllChannels && widgetDraft_.kind!=DashboardWidgetKind::BatteryCells)break;
            if(i==4 && widgetDraft_.kind!=DashboardWidgetKind::Channels && !speed)break;
            row(77+i*(designPortrait?51:30),names[i],values[i],120+i,designPortrait?47:28);
        }
        if(widgetDraft_.kind==DashboardWidgetKind::AllChannels)row(77+4*(designPortrait?51:30),"Visible inputs","Choose",169,designPortrait?47:28);
        label("Covered widgets are kept for later",12,designFooterY-(designPortrait?65:45),12,cDim);
        button(10,designFooterY-(designPortrait?44:23),90,designPortrait?40:23,"Cancel",125);button(kScreenWidth-110,designFooterY-(designPortrait?44:23),100,designPortrait?40:23,"Apply",126,true);
        }
    }
    finishPageFocus();
    // Resolve the width of initially empty status labels before LONG_DOT sees
    // their first live text; otherwise it can replace the entire reason with dots.
    if(page==UiPage::OutputStatus)lv_obj_update_layout(lv_scr_act());
    updateDesign(designTelemetry_,designSafety_,designTxVoltage_,designTxValid_);
    uiMetrics.buildUs = uint32_t(micros()) - uiMetrics.pageStartedUs;
}

void TransmitterUi::controlDesign(uint8_t item, PageControlEvent event)
{
    playKeyClick();
    if(item==231||item==232){tuneRole_=item==231?3:4;drawDesignPage(UiPage::ResponseProfile);return;}
    if(item>=233&&item<=235){const uint8_t fields[2][3]={{0,1,2},{3,4,7}};openDesignEditor(fields[responseDetailBank_][item-233],UiPage::ResponseProfile);return;}
    if(item==236){responseDetailBank_^=1;drawDesignPage(UiPage::ResponseProfile);return;}
    if(item>=224&&item<=229){
        const auto candidate=kParameterControls[item-224];const int assigned=candidate==PhysicalControlId::Count?-1:bindingIndexForInput(candidate);
        parameterControlError_=assigned>=0&&assigned!=bindingIndexForChannelParameter(selectedChannel_,parameterControlKind_);
        if(!parameterControlError_)parameterControlDraft_=candidate;
        drawDesignPage(UiPage::ParameterControlPicker);return;
    }
    if(item==230){
        parameterControlError_=!assignChannelParameterControl(selectedChannel_,parameterControlKind_,parameterControlDraft_);
        drawDesignPage(parameterControlError_?UiPage::ParameterControlPicker:UiPage::ChannelResponseSetup);return;
    }
    if(item==223){inputMonitorReturnPage_=UiPage::Telemetry;inputMonitorBank_=0;drawInputMonitorPage();return;}
    if(item==221||item==222){tuneRole_=item==221?3:4;drawDesignPage(UiPage::Tune);return;}
    if(item==237){beginPin(PinPurpose::ChangeCurrent,UiPage::SystemMenu);return;}
    if(item>=180 && item<=220){
        if(item>=213&&item<=217){controlSetup(item-213,event);return;}
        if(item==218){drawDesignPage(UiPage::MixingOverview);return;}
        if(item==180){if(designTouchDispatch_&&lastTouchDurationMs_>=800)drawDesignPage(UiPage::WidgetSelect);else{statusReturnPage_=UiPage::Dashboard;drawDesignPage(UiPage::OutputStatus);}return;}
        if(item==181){
            if(designTouchDispatch_&&armGestureValid_&&lastTouchDurationMs_>=kDashboardArmHoldMs){
                if(armRequested_){requestConfigurationCut();}
                else if(!SetupPortal::active()&&(!remoteTouchActive_||remoteArmHoldDispatch_)&&!calibrationActive()){armRequested_=true;safetyResetRequested_=true;activePage_=statusReturnPage_;redrawActivePage();}
            }
            return;
        }
        if(item==182||item==183){if(!requestDriveMode(item==182?DriveMode::Walk:DriveMode::Normal))return;requestConfigurationCut();saveDriveModeSettings();if(activePage_==UiPage::ModeChooser)drawDashboard();else drawDesignPage(UiPage::DriveModes);return;}
        if(item==184){driveModesReturnPage_=UiPage::Dashboard;drawDriveModesPage();return;}
        if(item==185||item==219||item==220){driveModeSettings_.walkLimitPercent=constrain(driveModeSettings_.walkLimitPercent+(item==219||event==PageControlEvent::Decrease?-5:5),10,80);requestConfigurationCut();saveDriveModeSettings();drawDriveModesPage();return;}
        if(item==186){channelInputPage_=0;drawDesignPage(UiPage::ChannelInput);return;}
        if(item==187){drawDesignPage(UiPage::ChannelResponseSetup);return;}
        if(item==188){mixingDraft_=channelModel_.mixing[selectedChannel_-1];mixingSaveError_=false;drawDesignPage(UiPage::MixingOverview);return;}
        if(item==189){channelInputPage_=1;channelRoleConflict_=false;drawDesignPage(UiPage::ChannelInput);return;}
        if(item==190){assignmentChannelContext_=selectedChannel_;assignmentBank_=0;drawControlAssignmentsPage();return;}
        if(item==191){if(selectedChannel_==steeringChannel())channelRoleSettings_.steeringEnabled=!steeringEnabled();else if(selectedChannel_==throttleChannel())channelRoleSettings_.throttleEnabled=!throttleEnabled();else{assignmentChannelContext_=selectedChannel_;drawControlAssignmentsPage();return;}requestConfigurationCut();channelRolesChanged_=true;saveChannelRoleSettings();drawDesignPage(UiPage::ChannelInput);return;}
        if(item==192){if(selectedChannel_==steeringChannel())startCalibration(latestSteeringRawAdc_,true);else if(selectedChannel_==throttleChannel())drawPage();return;}
        if(item==193||item==194){bool steer=item==193;channelRoleConflict_=selectedChannel_==(steer?throttleChannel():steeringChannel())||bindingUsesChannel(selectedChannel_,PhysicalControlId::Count);if(!channelRoleConflict_){if(steer)channelRoleSettings_.steeringChannel=selectedChannel_;else channelRoleSettings_.throttleChannel=selectedChannel_;requestConfigurationCut();channelRolesChanged_=true;saveChannelRoleSettings();channelInputPage_=0;}drawDesignPage(UiPage::ChannelInput);return;}
        if(item==195){tuneRole_=selectedChannel_==steeringChannel()?0:selectedChannel_==throttleChannel()?1:3;tuneBank_=0;drawDesignPage(UiPage::Tune);return;}
        if(item==196){responseDetailBank_=0;tuneRole_=3;drawDesignPage(UiPage::ResponseProfile);return;}
        if(item==197||item==198){
            parameterControlKind_=item==197?ChannelParameterKind::Trim:ChannelParameterKind::Subtrim;
            const int current=bindingIndexForChannelParameter(selectedChannel_,parameterControlKind_);
            parameterControlDraft_=current>=0?bindings_[current].input:PhysicalControlId::Count;parameterControlError_=false;
            drawDesignPage(UiPage::ParameterControlPicker);return;
        }
        if(item>=199&&item<=203){const uint8_t banks[]={0,3,2,6,5};mixingBank_=banks[item-199];drawChannelMixing();return;}
        if(item==204){mixingSaveError_=!saveMixing(selectedChannel_,mixingDraft_);if(!mixingSaveError_)drawChannelPlaceholder();return;}
        if(item==205){drawDesignPage(UiPage::NetworkOptions);return;}
        if(item==206){drawDesignPage(UiPage::SystemMenu);return;}
        if(item==207||item==208){setupPage_=!setupPage_;drawMainMenu();return;}
        if(item==209){
            if(activePage_==UiPage::OutputStatus){activePage_=statusReturnPage_;redrawActivePage();return;}
            if(activePage_==UiPage::ParameterControlPicker||activePage_==UiPage::ResponseProfile){drawDesignPage(UiPage::ChannelResponseSetup);return;}
            if(activePage_==UiPage::ChannelInput&&channelInputPage_){channelInputPage_=0;drawDesignPage(UiPage::ChannelInput);return;}
            if(activePage_==UiPage::ChannelInput||activePage_==UiPage::ChannelResponseSetup||activePage_==UiPage::MixingOverview){drawChannelPlaceholder();return;}
            if(activePage_==UiPage::ChannelPlaceholder){drawChannelsPage();return;}
            if(activePage_==UiPage::ModeChooser||activePage_==UiPage::DriveModes){drawDesignPage(driveModesReturnPage_);return;}
            drawMainMenu();return;
        }
        if(item==210||item==211||item==212){requestConfigurationCut();SetupPortal::start();wifiWebAccess_=item==211;wifiEdit_=item==212?5:0;snprintf(wifiSsid_,sizeof(wifiSsid_),"%s",SetupPortal::state().ssid);drawWifiPage();return;}
    }
    if(item>=169&&item<=179){
        uint8_t entries[DashboardMonitorMaxEntries];const int total=dashboardMonitorEntries(0xffff,widgetDraft_.source,entries),pages=(total+4)/5;
        if(item==169){widgetInputsEditing_=true;widgetInputsBank_=0;}
        else if(item>=170&&item<=174){int i=widgetInputsBank_*5+item-170;if(i<total)widgetHiddenDraft_^=dashboardMonitorBit(entries[i]);}
        else if(item==175)widgetInputsBank_=(widgetInputsBank_+pages-1)%pages;
        else if(item==176)widgetInputsBank_=(widgetInputsBank_+1)%pages;
        else if(item==177)widgetInputsEditing_=false;
        else for(int i=0;i<total;++i){if(item==178)widgetHiddenDraft_&=~dashboardMonitorBit(entries[i]);else widgetHiddenDraft_|=dashboardMonitorBit(entries[i]);}
        drawDesignPage(UiPage::WidgetEditor);return;
    }
    if(item>=160 && item<=165){
        if(armRequested_)return;
        if(item==160)modelAppearanceEditing_=!modelAppearanceEditing_;
        else if(item==161 || item==162)modelImages_[activeModelSlot_]=(modelImages_[activeModelSlot_]+(item==161?kModelImageCount-1:1))%kModelImageCount;
        else if(item==164)modelPaintEditing_=(modelPaintEditing_+1)%3;
        else if(item==165)modelColours_[activeModelSlot_]=modelAccents_[activeModelSlot_]=modelBases_[activeModelSlot_]=kModelOriginalColour;
        else {
            auto &paint=modelPaintEditing_==0?modelColours_[activeModelSlot_]:modelPaintEditing_==1?modelAccents_[activeModelSlot_]:modelBases_[activeModelSlot_];
            static const uint32_t swatches[]={kModelOriginalColour,0xe53935,0xff8c00,0xffd600,0x43a047,0x00bcd4,0x1976d2,0x8e24aa,0xffffff,0x303030};
            unsigned i=0;for(;i<10;++i)if(swatches[i]==paint)break;
            paint=swatches[(i+1)%10];
        }
        saveModelMemorySettings();drawModelSelectPage();return;
    }
    if(item==152){
        wifiWebAccess_=false;
        requestConfigurationCut();wifiEdit_=0;
        snprintf(wifiSsid_,sizeof(wifiSsid_),"%s",SetupPortal::state().ssid);
        wifiPassword_[0]=0;SetupPortal::start();drawWifiPage();return;
    }
    if(item==0&&designTouchDispatch_&&armGestureValid_&&lastTouchDurationMs_>=kDashboardArmHoldMs){
        // Manual output changes require the full hold in either direction.
        if(armRequested_){requestConfigurationCut();}
        else if(!SetupPortal::active()&&(!remoteTouchActive_||remoteArmHoldDispatch_)&&!calibrationActive()){armRequested_=true;safetyResetRequested_=true;}
        drawDashboard();return;
    }
    if(item<=3){
        if(item==0&&calibrationActive()){
            (calibratingSteering_?steeringInput_:triggerInput_).cancelCalibration();
            requestConfigurationCut();
        }
        designBank_=0;if(item==3)setupBank_=0;drawDesignPage(item==0?UiPage::Dashboard:item==1?UiPage::Tune:item==2?UiPage::Telemetry:UiPage::MainMenu);return;}
    if(item==4){drawModelSelectPage();return;}
    if(item==5){statusReturnPage_=activePage_;drawDesignPage(UiPage::OutputStatus);return;}
    if(item==6){driveModesReturnPage_=UiPage::Dashboard;drawDesignPage(UiPage::ModeChooser);return;}
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
        case 42:assignmentChannelContext_=0;drawControlAssignmentsPage();return;
        case 43:driveModesReturnPage_=UiPage::MainMenu;drawDriveModesPage();return;
        case 44:txConfig_.start(millis());txConfigFolder_=txConfigOffset_=txConfigSelected_=0;drawElrsTxConfigPage();return;
        case 45:drawElrsBindingPage();return;
        case 46:drawEscProgrammingPage();return;
        case 47:drawSetupPage();return;
        case 48:inputMonitorReturnPage_=UiPage::SystemMenu;inputMonitorBank_=0;drawInputMonitorPage();return;
        case 49:designBank_=0;drawDesignPage(UiPage::WidgetSelect);return;
        case 63:drawRadioDiagnostics();return;
        case 60:drawTelemetryPage();return;
        case 61:drawDesignPage(UiPage::Gps);return;
        case 62:designBank_=0;drawDesignPage(UiPage::ChannelMonitor);return;
    }
    if(item>=70 && item<=75){sensorSelected_=sensorBank_*6+item-70;drawDesignPage(UiPage::SensorDetail);return;}
    if(item==80 || item==81){
        int direction=item==80?-1:1;
        if(activePage_==UiPage::ModelSelect)modelSelectBank_=constrain(modelSelectBank_+direction,0,1);
        else if(activePage_==UiPage::MainMenu)setupBank_=constrain(setupBank_+direction,0,designPortrait?1:2);
        else if(activePage_==UiPage::Telemetry)sensorBank_=constrain(sensorBank_+direction,0,(DashboardSensorCount-1)/6);
        else designBank_=constrain(designBank_+direction,0,3);
        drawDesignPage(activePage_);return;
    }
    if(item>=82 && item<=85){if(item==82)gpsZoom_=max<int>(1,gpsZoom_-1);if(item==83)gpsZoom_=min<int>(8,gpsZoom_+1);if(item==84)gpsTrailVisible_=!gpsTrailVisible_;if(item==85)gpsCount_=0;drawDesignPage(UiPage::Gps);return;}
    if(item==86){drawChannelRolesPage();return;}
    if(item>=90 && item<=93){selectedChannel_=designBank_*4+item-90+1;drawChannelPlaceholder();return;}
    if(item>=100 && item<=104){selectModelMemory(modelSelectBank_*5+item-100);drawModelSelectPage();return;}
    if(item>=110 && item<=113){widgetCell_=item-110;widgetDraft_=dashboardLayouts_[activeModelSlot_].cells[widgetCell_];widgetHiddenDraft_=dashboardHidden_[activeModelSlot_][widgetCell_];widgetInputsEditing_=false;drawDesignPage(UiPage::WidgetEditor);return;}
    if(item>=120 && item<=124){
        int direction=event==PageControlEvent::Decrease?-1:1;
        if(item==120){widgetDraft_.kind=static_cast<DashboardWidgetKind>((static_cast<int>(widgetDraft_.kind)+direction+static_cast<int>(DashboardWidgetKind::Count))%static_cast<int>(DashboardWidgetKind::Count));if(widgetDraft_.kind==DashboardWidgetKind::BatteryCells)widgetDraft_.source=0;}
        if(item==121)cycleDashboardWidgetSize(widgetCell_,widgetDraft_,direction);
        if(item==122){int style=static_cast<int>(widgetDraft_.style);const int styles=static_cast<int>(DashboardWidgetStyle::Count);for(int n=0;n<styles;++n){style=(style+direction+styles)%styles;if(dashboardStyleAllowed(widgetDraft_.kind,static_cast<DashboardWidgetStyle>(style)))break;}widgetDraft_.style=static_cast<DashboardWidgetStyle>(style);}
        if(item==123 && widgetDraft_.kind==DashboardWidgetKind::AllChannels)widgetDraft_.source=(widgetDraft_.source+direction+3)%3;
        else if(item==123 && widgetDraft_.kind==DashboardWidgetKind::BatteryCells)widgetDraft_.source=(widgetDraft_.source+direction+13)%13;
        else if(item==123)widgetDraft_.source=widgetDraft_.kind==DashboardWidgetKind::Sensor ? nextDashboardSensorSource(widgetDraft_.source,direction) : (widgetDraft_.source+direction+16)%16;
        if(item==124){
            if(widgetDraft_.kind==DashboardWidgetKind::Sensor && widgetDraft_.source==0)
                widgetDraft_.speedValue=static_cast<SpeedWidgetValue>((static_cast<int>(widgetDraft_.speedValue)+direction+3)%3);
            else widgetDraft_.secondSource=(widgetDraft_.secondSource+direction+17)%17;
        }
        widgetDraft_=sanitizedDashboardWidget(widgetCell_,widgetDraft_);drawDesignPage(UiPage::WidgetEditor);return;
    }
    if(item==125){drawDesignPage(UiPage::WidgetSelect);return;}
    if(item==126){dashboardHidden_[activeModelSlot_][widgetCell_]=widgetHiddenDraft_;channelWidgetPages_[widgetCell_]=0;placeDashboardWidget(dashboardLayouts_[activeModelSlot_],widgetCell_,widgetDraft_);saveModelMemorySettings();drawDashboard();return;}
    if(item==140 || item==141){setupBank_=item-140;setupPage_=0;drawMainMenu();return;}
    if(item==142){displaySetupBank_=0;drawDisplayPage();return;}
    if(item==143){drawDesignPage(UiPage::BatteryMenu);return;}
    if(item==144){drawFirmwareInfoPage();return;}
    if(item==145){drawTxBatteryPage();return;}
    if(item==146){drawRxBatteryPage();return;}
    if(item==148){resetSpeedStatistics();return;}
    if(item==147){dashboardLayouts_[activeModelSlot_]=defaultDashboardLayout();saveModelMemorySettings();drawDashboard();return;}
    if(item>=130 && item<=133){const auto &w=dashboardLayouts_[activeModelSlot_].cells[item-130];
        if(w.kind==DashboardWidgetKind::AllChannels){++channelWidgetPages_[item-130];drawDashboard();}
        else if(w.kind==DashboardWidgetKind::Map)drawDesignPage(UiPage::Gps);
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
        for(int sensor=0;sensor<DashboardSensorCount;++sensor){
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
    {
        uint32_t col=safety==OutputSafetyStatus::Armed?cGreen:cAmber;
        const char *reason="Outputs inhibited. Hold Drive to enable when ready.";
        switch(safety){
            case OutputSafetyStatus::Armed:reason="Outputs LIVE. Hold Drive to CUT.";break;
            case OutputSafetyStatus::StartupWarning:reason="Return controls to their safe positions.";col=cRed;break;
            case OutputSafetyStatus::LinkRequired:reason="Waiting for receiver link.";break;
            case OutputSafetyStatus::NeutralRequired:reason="Release throttle to neutral before arming.";break;
            case OutputSafetyStatus::Calibration:reason="Calibration active; output remains CUT.";break;
            case OutputSafetyStatus::InputFault:reason="Input fault. Check connections and assignments.";col=cRed;break;
            case OutputSafetyStatus::ConfigurationFault:reason="Model settings invalid. Review channels and save before enabling output.";col=cRed;break;
            case OutputSafetyStatus::ControlStale:reason="Input data stale; output remains CUT.";col=cRed;break;
            case OutputSafetyStatus::ChannelDisabled:reason="Required channel disabled; output remains CUT.";break;
            case OutputSafetyStatus::Cut:break;
        }
        if(activePage_==UiPage::OutputStatus){
            uiSetText(designDetails_[0],safety==OutputSafetyStatus::Armed?"LIVE - output enabled":
                safety==OutputSafetyStatus::LinkRequired?"LINK WAIT":
                safety==OutputSafetyStatus::NeutralRequired?"NEUTRAL WAIT":"CUT - output inhibited");
            uiSetText(designDetails_[1],hardwareRecovery_?"Hardware fault; arming inhibited.":SetupPortal::active()?"Setup session active; close it on the handset to arm.":reason);
            if(armProgress_)uiSetText(lv_obj_get_child(lv_obj_get_parent(armProgress_),0),armRequested_?"Hold to cut (0.8 s)":"Enable output");
        }
        if(hardwareRecovery_&&!rfBenchMode_)col=cRed;
        if(designStatus_)uiSetText(designStatus_,"Drive");
        updateDriveIcon(driveIcon_,safety==OutputSafetyStatus::Armed,col==cRed);
    }

    if(designLink_){
        if(safety==OutputSafetyStatus::LinkRequired)uiSetText(designLink_,"Link wait");
        else if(lowTx || lowRx)uiSetText(designLink_,lowTx?"TX LOW":"RX LOW");
        else if(t.linkValid)uiSetTextFmt(designLink_,"%u LQ",t.uplinkLinkQuality);
        else uiSetText(designLink_,t.linkUpdatedMs?"LQ stale":"LQ --");
        uiSetTextColor(designLink_,safety==OutputSafetyStatus::LinkRequired?colour(cAmber):lv_color_hex(lowTx||lowRx?0xff746a:t.linkValid?0xa4d6ae:0xa9bbc5),0);
    }
    auto reading=[&](uint8_t sensor,lv_obj_t *valueLabel,lv_obj_t *detailLabel){
        if(!valueLabel)return;
        int32_t value=0;uint32_t stamp=0;bool valid=sensorReading(t,sensor,value,stamp);char text[32];sensorText(sensor,value,text,sizeof(text));
        uiSetText(valueLabel,valid?text:"--");
        uiSetTextColor(valueLabel,colour(valid?cWhite:cDim),0);
        if(detailLabel){
            if(!valid)uiSetText(detailLabel,stamp?"Stale":"No data");
            else if(sensor==13 && value>3000)uiSetText(detailLabel,"Over 300A");
            else uiSetTextFmt(detailLabel,"%lus",static_cast<unsigned long>((now-stamp)/1000));
        }
    };
    if(activePage_==UiPage::Dashboard){
        updateDashboardWidgets(t,tx,txValid);
    } else if(activePage_==UiPage::Telemetry){for(int i=0;i<6;++i)reading(sensorBank_*6+i,designValues_[i],designDetails_[i]);}
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
        if(!gpsFullScreen_){if(!t.gpsValid || t.satellites<4)uiSetText(designDetails_[0],t.gpsUpdatedMs?"GPS stale / no fix":"Waiting for GPS telemetry");
        else uiSetTextFmt(designDetails_[0],"%u sats  %u.%u km/h  %u deg",t.satellites,t.groundSpeedDeciKmh/10,t.groundSpeedDeciKmh%10,t.headingCentiDegrees/100);}
        auto mapObject=lv_obj_get_parent(designCurve_);
        const double pixelsPerMetre=gpsBasemapPreviewScale(mapObject,true)*gpsZoom_;
        if(!gpsCount_)previewGpsBasemap(mapObject,designMapViewport_,true,gpsZoom_);
        if(gpsCount_){
            const int width=lv_obj_get_width(mapObject),height=lv_obj_get_height(mapObject);
            updateGpsBasemap(lv_obj_get_parent(designCurve_),designMapViewport_,gpsLat_[gpsCount_-1],gpsLon_[gpsCount_-1],pixelsPerMetre);
            const double lonScale=cos(gpsLat_[gpsCount_-1]/1e7*3.141592653589793/180.0);
            for(int i=0;i<gpsCount_;++i){
                double east=(static_cast<double>(gpsLon_[i])-gpsLon_[gpsCount_-1])*0.01113195*lonScale;
                double north=(static_cast<double>(gpsLat_[i])-gpsLat_[gpsCount_-1])*0.01113195;
                gpsPoints_[i]={static_cast<lv_coord_t>(constrain(width/2+east*pixelsPerMetre,-10000,10000)),static_cast<lv_coord_t>(constrain(height/2-north*pixelsPerMetre,-10000,10000))};
            }
            if(gpsCount_>1 && gpsTrailVisible_){lv_line_set_points(designCurve_,gpsPoints_,gpsCount_);lv_obj_clear_flag(designCurve_,LV_OBJ_FLAG_HIDDEN);}else lv_obj_add_flag(designCurve_,LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(designMarker_,width/2-9,height/2-9);lv_obj_clear_flag(designMarker_,LV_OBJ_FLAG_HIDDEN);
            uiSetTextColor(designMarker_,colour(t.gpsValid && t.satellites>=4?0x00707d:0x53636c),0);
            uiSetTextFmt(designDetails_[1],"%um",static_cast<unsigned>(40/pixelsPerMetre));
        }
    }
    // Retain the existing voltage warning cadence on every primary page.
    if(lowTx){if(static_cast<int32_t>(now-nextTxLowBatteryAlarmMs_)>=0){playBuzzer(300);nextTxLowBatteryAlarmMs_=now+kLowBatteryRepeatMs;}}else nextTxLowBatteryAlarmMs_=now;
    if(lowRx){if(static_cast<int32_t>(now-nextRxLowBatteryAlarmMs_)>=0){playBuzzer(300);nextRxLowBatteryAlarmMs_=now+kLowBatteryRepeatMs;}}else nextRxLowBatteryAlarmMs_=now;
}

void TransmitterUi::drawDashboardWidgets()
{
    const auto &layout=dashboardLayouts_[activeModelSlot_];
    constexpr int gap=kPanelGap, top=83;
    const int cellW=(kScreenWidth-20-gap)/2, cellH=(designFooterY-top-gap-6)/2;
    const uint8_t visible=visibleDashboardWidgets(layout);
    uint8_t occupied=0;
    for(uint8_t i=0;i<4;++i)if(visible&(1<<i))occupied |= dashboardWidgetMask(i,layout.cells[i]);
    for(uint8_t cell=0;cell<4;++cell){
        const auto &w=layout.cells[cell];
        int x=10+cell%2*(cellW+gap),y=top+cell/2*(cellH+gap);
        if(w.kind==DashboardWidgetKind::Empty){
            if(!(occupied&(1<<cell)))makePageActionButton(x,y,cellW,cellH,"+ Add widget",14,cPanel,cDim,cLine,&TransmitterUi::controlDesign,110+cell);
            continue;
        }
        if(!(visible&(1<<cell)))continue;
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
        } else if(w.kind==DashboardWidgetKind::AllChannels){
            uint8_t channels[DashboardMonitorMaxEntries];const int total=dashboardMonitorEntries(dashboardEnabledChannelMask_,w.source,channels,dashboardHidden_[activeModelSlot_][cell]);
            const bool vertical=w.style==DashboardWidgetStyle::VerticalBars;
            const int capacity=dashboardChannelCapacity(width,height,w.style),columns=vertical?dashboardVerticalColumns(width):(width>=220?2:1);
            const int pages=max(1,(total+capacity-1)/capacity);
            channelWidgetPages_[cell]%=pages;
            const int first=channelWidgetPages_[cell]*capacity,count=min(capacity,total-first);
            const int rows=(capacity+columns-1)/columns,cw=(width-16)/columns;
            const int rowHeight=vertical?(height-42)/rows:28;
            for(int j=0;j<count;++j){
                const int xx=8+(vertical?j%columns:j/rows)*cw,yy=25+(vertical?j/columns:j%rows)*rowHeight;
                view.values[j]=makeLabel(view.panel,"",xx,yy,12,cWhite);
                int off=0,on=0;const int entry=channels[first+j];
                const bool trim=entry==(0x80|4)||entry==(0x80|5);
                const bool binary=(entry&0x80)?!trim:dashboardBinaryChannel(entry,off,on);
                boundedLabel(view.values[j],vertical?cw-4:(binary?cw-43:cw-6));
                if(vertical){lv_label_set_long_mode(view.values[j],LV_LABEL_LONG_CLIP);lv_obj_set_height(view.values[j],28);lv_obj_set_style_text_align(view.values[j],LV_TEXT_ALIGN_CENTER,0);}
                if(binary){
                    view.switches[j]=lv_switch_create(view.panel);position(view.switches[j],vertical?xx+(cw-34)/2:xx+cw-42,vertical?yy+32:yy+1,34,15);
                    lv_obj_clear_flag(view.switches[j],LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_set_style_bg_color(view.switches[j],colour(cCyan),LV_PART_INDICATOR|LV_STATE_CHECKED);
                }
                else if(w.style==DashboardWidgetStyle::Bars){view.bars[j]=makeBar(view.panel,xx,yy+18,cw-8,4,-1000,1000);lv_bar_set_mode(view.bars[j],LV_BAR_MODE_SYMMETRICAL);}
                else if(vertical){
                    const int barHeight=max(12,rowHeight-34),barX=xx+(cw-10)/2;
                    view.bars[j]=makeBar(view.panel,barX,yy+30,10,barHeight,-1000,1000);
                    lv_bar_set_mode(view.bars[j],LV_BAR_MODE_SYMMETRICAL);
                    auto zero=lv_obj_create(view.panel);position(zero,barX-3,yy+30+barHeight/2,16,1);
                    lv_obj_set_style_bg_color(zero,colour(cDim),0);lv_obj_set_style_border_width(zero,0,0);
                    lv_obj_set_style_pad_all(zero,0,0);
                }
            }
            char caption[40];
            if(!total)snprintf(caption,sizeof(caption),"No selected inputs");
            else if(pages>1)snprintf(caption,sizeof(caption),"%u/%u - tap for more",channelWidgetPages_[cell]+1,pages);
            else snprintf(caption,sizeof(caption),"%u %s",total,w.source==1?"physical inputs":w.source==2?"outputs / inputs":"configured outputs");
            view.detail=makeLabel(view.panel,caption,8,height-16,10,cDim);boundedLabel(view.detail,width-16);
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
                    const bool tall=w.width==1&&w.height==2;
                    int diameter=tall?min(width-16,(height-28)/count-6):min(width/count-12,height-24);
                    int ax=tall?(width-diameter)/2:width/(2*count)+j*width/count-diameter/2;
                    int ay=tall?25+j*(height-28)/count:22;
                    auto arc=lv_arc_create(view.panel);view.arcs[j]=arc;
                    position(arc,ax,ay,diameter,diameter);lv_obj_clear_flag(arc,LV_OBJ_FLAG_CLICKABLE);
                    lv_arc_set_rotation(arc,135);lv_arc_set_bg_angles(arc,0,270);
                    lv_obj_set_style_arc_width(arc,4,LV_PART_MAIN);lv_obj_set_style_arc_width(arc,4,LV_PART_INDICATOR);
                    lv_obj_set_style_arc_color(arc,colour(cLine),LV_PART_MAIN);lv_obj_set_style_arc_color(arc,colour(cCyan),LV_PART_INDICATOR);
                    lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,LV_PART_KNOB);lv_obj_set_style_pad_all(arc,0,LV_PART_KNOB);
                    auto name=makeLabel(view.panel,text,0,0,10,cDim);lv_obj_set_pos(name,ax+diameter/2-12,ay+diameter-14);
                    view.values[j]=makeLabel(view.panel,"--",ax+5,ay+diameter/2-9,14,cWhite);
                    lv_obj_set_width(view.values[j],diameter-10);lv_obj_set_style_text_align(view.values[j],LV_TEXT_ALIGN_CENTER,0);
                } else {
                    int yy=25+j*(height-27)/count;
                    if(w.style==DashboardWidgetStyle::Graph){
                        const bool tall=w.width==1&&w.height==2;
                        makeLabel(view.panel,text,tall?8:8+j*width/count,tall?22+j*15:22,10,j?cAmber:cCyan);
                        view.values[j]=makeLabel(view.panel,"--",tall?50:50+j*width/count,tall?22+j*15:22,10);
                        continue;
                    }
                    makeLabel(view.panel,text,8,yy,12,cDim);
                    view.values[j]=makeLabel(view.panel,"--",power&&height>130?8:width/2,power&&height>130?yy+18:yy,power&&height>130?24:16);
                    if(w.style==DashboardWidgetStyle::Bars){view.bars[j]=makeBar(view.panel,8,yy+(power&&height>130?48:19),width-16,5,power?0:-1000,power?160:1000);if(!power)lv_bar_set_mode(view.bars[j],LV_BAR_MODE_SYMMETRICAL);}
                }
            }
        } else if(w.kind==DashboardWidgetKind::BatteryCells){
            for(uint8_t i=0;i<CrsfTelemetryState::MaxVehicleCells;++i){
                view.values[i]=makeLabel(view.panel,"--",8,24,12,cWhite);
                lv_obj_add_flag(view.values[i],LV_OBJ_FLAG_HIDDEN);
            }
            view.detail=makeLabel(view.panel,"No cell telemetry",8,height-(height<120?16:29),10,cDim);
            lv_obj_set_size(view.detail,width-16,height<120?14:28);
            lv_label_set_long_mode(view.detail,LV_LABEL_LONG_CLIP);
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
            if(speed && w.style==DashboardWidgetStyle::Dials){
                const bool compact=height<130;
                const int diameter=compact?min(width/2-12,height-28):min(width-16,height-64);
                const int ax=compact?8:(width-diameter)/2,ay=23;
                if(compact){lv_obj_set_pos(view.values[1],width/2,36);lv_obj_set_width(view.values[1],width/2-8);}
                auto arc=lv_arc_create(view.panel);view.arcs[0]=arc;
                position(arc,ax,ay,diameter,diameter);lv_obj_clear_flag(arc,LV_OBJ_FLAG_CLICKABLE);
                lv_arc_set_rotation(arc,135);lv_arc_set_bg_angles(arc,0,270);
                lv_arc_set_range(arc,0,1000);
                lv_obj_set_style_arc_width(arc,4,LV_PART_MAIN);lv_obj_set_style_arc_width(arc,4,LV_PART_INDICATOR);
                lv_obj_set_style_arc_color(arc,colour(cLine),LV_PART_MAIN);lv_obj_set_style_arc_color(arc,colour(cCyan),LV_PART_INDICATOR);
                lv_obj_set_style_bg_opa(arc,LV_OPA_TRANSP,LV_PART_KNOB);lv_obj_set_style_pad_all(arc,0,LV_PART_KNOB);
                lv_obj_set_pos(view.values[0],ax+4,ay+diameter/2-12);
                lv_obj_set_width(view.values[0],diameter-8);lv_obj_set_style_text_align(view.values[0],LV_TEXT_ALIGN_CENTER,0);
                lv_obj_set_style_text_font(view.values[0],fontFor(diameter>=110?24:14),0);
                lv_obj_move_foreground(view.values[0]);
                view.marker=makeLabel(view.panel,"0-100",ax,ay+diameter-12,10,cDim);
                lv_obj_set_width(view.marker,diameter);lv_obj_set_style_text_align(view.marker,LV_TEXT_ALIGN_CENTER,0);
            }
            if(w.style==DashboardWidgetStyle::Graph){view.plot=lv_line_create(view.panel);lv_obj_set_style_line_color(view.plot,colour(cCyan),0);lv_obj_set_style_line_width(view.plot,2,0);}
        } else if(w.kind==DashboardWidgetKind::Map){
            lv_obj_add_flag(title,LV_OBJ_FLAG_HIDDEN);
            view.basemap=createGpsBasemap(view.panel,0,0,width-2,height-2,view.mapViewport);
            view.plot=lv_line_create(view.basemap);lv_obj_set_style_line_color(view.plot,colour(0x00707d),0);lv_obj_set_style_line_width(view.plot,2,0);
            view.marker=makeLabel(view.basemap,LV_SYMBOL_GPS,width/2-8,height/2-8,16,cCyan);
            lv_obj_add_flag(view.marker,LV_OBJ_FLAG_HIDDEN);
            auto north=makeLabel(view.basemap,LV_SYMBOL_UP "\nN",0,0,10,0x17343c);
            lv_obj_set_style_text_align(north,LV_TEXT_ALIGN_CENTER,0);
            lv_obj_set_style_bg_color(north,colour(0xf5f7f3),0);
            lv_obj_set_style_bg_opa(north,LV_OPA_90,0);
            lv_obj_set_style_pad_all(north,3,0);
            lv_obj_align(north,LV_ALIGN_TOP_RIGHT,-4,4);
            view.detail=makeLabel(view.basemap,"",4,4,10,0x17343c);
            lv_obj_set_style_bg_color(view.detail,colour(0xf5f7f3),0);
            lv_obj_set_style_bg_opa(view.detail,LV_OPA_90,0);
        } else if(w.kind==DashboardWidgetKind::Model){
            lv_label_set_text(title,"Model");
            if(const auto *art=modelImage(modelImages_[activeModelSlot_],modelColours_[activeModelSlot_],modelAccents_[activeModelSlot_],modelBases_[activeModelSlot_])){
                // Fit the artwork between the title and name with an outer margin.
                constexpr int margin=10, imageTop=25, nameSpace=24;
                const int availableW=width-2*margin, availableH=height-imageTop-nameSpace;
                const int sourceW=art->header.w, sourceH=art->header.h;
                const int zoom=min(availableW*256/sourceW,availableH*256/sourceH);
                auto image=lv_img_create(view.panel);lv_img_set_src(image,art);
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
        } else if(w.kind==DashboardWidgetKind::AllChannels){
            uint8_t channels[DashboardMonitorMaxEntries];const int total=dashboardMonitorEntries(dashboardEnabledChannelMask_,w.source,channels,dashboardHidden_[activeModelSlot_][cell]);
            const bool vertical=w.style==DashboardWidgetStyle::VerticalBars;
            const int capacity=dashboardChannelCapacity(lv_obj_get_width(view.panel),lv_obj_get_height(view.panel),w.style);
            const int first=channelWidgetPages_[cell]*capacity,count=min(capacity,total-first);
            for(int j=0;j<count;++j){
                const int entry=channels[first+j];bool valid=true,high=false;int off=0,on=0;
                if(entry&0x80){
                    const auto &state=physicalInputStates_[entry&0x7f];valid=state.mapped&&!state.faulted;high=state.stablePrimary;
                    const int id=entry&0x7f;const bool trim=id==4||id==5;
                    const char *name=id==3?"Drive":id==9?"Idle up":id==2?"Timer":id==4?"Grip A":"Grip B";
                    if(trim&&state.stablePrimary&&state.stableSecondary)valid=false;
                    const int position=state.stableSecondary?1000:state.stablePrimary?-1000:0;
                    const char *status=!valid?"--":trim?(position>0?"+":position<0?"-":"0"):id==2?(high?"DOWN":"UP"):(high?"ON":"OFF");
                    uiSetTextFmt(view.values[j],vertical?"%s\n%s":"%s %s",name,status);
                    if(view.bars[j]){lv_bar_set_value(view.bars[j],valid?position:0,LV_ANIM_OFF);lv_obj_set_style_opa(view.bars[j],valid?LV_OPA_COVER:LV_OPA_30,0);}
                }else{
                    const int value=permilleFromCrsfValue(dashboardChannelValues_[entry]);
                    if(view.switches[j] && dashboardBinaryChannel(entry,off,on)){
                        // Neutral/CUT or an intermediate output is not a false OFF.
                        valid=abs(value-off)<=25||abs(value-on)<=25;high=abs(value-on)<abs(value-off);
                        uiSetTextFmt(view.values[j],vertical?"CH%u\n%s":"CH%u %s",entry+1,valid?(high?"ON":"OFF"):"MID");
                    }else uiSetTextFmt(view.values[j],vertical?"CH%u\n%+d%%":"CH%u %+d%%",entry+1,value/10);
                    if(view.bars[j])lv_bar_set_value(view.bars[j],value,LV_ANIM_OFF);
                }
                if(view.switches[j]){
                    if(valid&&high)lv_obj_add_state(view.switches[j],LV_STATE_CHECKED);else lv_obj_clear_state(view.switches[j],LV_STATE_CHECKED);
                    lv_obj_set_style_opa(view.switches[j],valid?LV_OPA_COVER:LV_OPA_30,0);
                }
            }
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
                    const int plotTop=w.width==1&&w.height==2&&w.secondSource<16?55:37;
                    const int plotHeight=max(1,height-plotTop-21);
                    for(int i=0;i<count;++i)points[i]={static_cast<lv_coord_t>(8+i*(width-16)/(count-1)),static_cast<lv_coord_t>(plotTop+plotHeight/2-permilleFromCrsfValue(channelHistory_[ch][30-count+i])*plotHeight/2000)};
                    lv_line_set_points(j?view.secondPlot:view.plot,points,count);
                }
                if(view.arcs[j]){lv_arc_set_range(view.arcs[j],power?0:-1000,maximum);lv_arc_set_value(view.arcs[j],valid?value:0);uiSetArcColor(view.arcs[j],colour(low?cRed:valid?cCyan:cDim),LV_PART_INDICATOR);}
                if(view.bars[j]){lv_bar_set_range(view.bars[j],power?0:-1000,maximum);lv_bar_set_value(view.bars[j],valid?value:0,LV_ANIM_OFF);uiSetBgColor(view.bars[j],colour(low?cRed:cCyan),LV_PART_INDICATOR);}
            }
        } else if(w.kind==DashboardWidgetKind::BatteryCells){
            lv_obj_update_layout(view.panel);
            const uint8_t reported=min<uint8_t>(t.vehicleCellCount,CrsfTelemetryState::MaxVehicleCells);
            const uint8_t count=max<uint8_t>(w.source,reported);
            const int width=lv_obj_get_width(view.panel),height=lv_obj_get_height(view.panel);
            const int columns=width>=200?3:2,rows=max(1,(count+columns-1)/columns);
            const int rowHeight=max(1,(height-(height<120?43:53))/rows),columnWidth=(width-16)/columns;
            for(uint8_t i=0;i<CrsfTelemetryState::MaxVehicleCells;++i){
                if(i>=count){lv_obj_add_flag(view.values[i],LV_OBJ_FLAG_HIDDEN);continue;}
                lv_obj_clear_flag(view.values[i],LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_pos(view.values[i],8+(i%columns)*columnWidth,24+(i/columns)*rowHeight);
                lv_obj_set_size(view.values[i],columnWidth,rowHeight);
                lv_label_set_long_mode(view.values[i],LV_LABEL_LONG_CLIP);
                lv_obj_set_style_text_font(view.values[i],fontFor(rowHeight>=16?12:10),0);
                if(t.vehicleCellsValid && i<reported)uiSetTextFmt(view.values[i],"%u: %u.%03u",i+1,t.vehicleCellsMv[i]/1000,t.vehicleCellsMv[i]%1000);
                else uiSetTextFmt(view.values[i],"%u: --",i+1);
            }
            if(!t.vehicleCellsValid)uiSetText(view.detail,t.vehicleCellsUpdatedMs?"Cell telemetry stale":"No cell telemetry");
            else if(w.source && w.source!=reported)uiSetTextFmt(view.detail,"Expected %uS; received %uS\nCheck pack / telemetry",w.source,reported);
            else if(height<120)uiSetTextFmt(view.detail,"%uS  %u.%02uV  Low %u.%02uV",reported,t.vehiclePackMv/1000,(t.vehiclePackMv%1000)/10,t.vehicleMinimumCellMv/1000,(t.vehicleMinimumCellMv%1000)/10);
            else uiSetTextFmt(view.detail,"%uS  %u.%03u V\nMin %u.%03u V",reported,t.vehiclePackMv/1000,t.vehiclePackMv%1000,t.vehicleMinimumCellMv/1000,t.vehicleMinimumCellMv%1000);
        } else if(w.kind==DashboardWidgetKind::Sensor){
            int32_t value=0;uint32_t stamp=0;bool valid=sensorReading(t,w.source,value,stamp);char text[24];sensorText(w.source,value,text,sizeof(text));
            if(w.source!=0){uiSetText(view.values[0],valid?text:"--");uiSetTextFmt(view.detail,"%s%s",designSensorUnits[w.source],valid?(w.source==13 && value>3000?" / check range":""):stamp?" / stale":" / no data");}
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
                if(view.arcs[0]){
                    const bool available=primary==0?valid:speedStatistics_.available();
                    const int selected=primary==0?value:primary==1?speedStatistics_.maximum():speedStatistics_.average();
                    const int peak=max(int(speedStatistics_.maximum()),available?selected:0);
                    const int maximum=max(1000,((peak+999)/1000)*1000);
                    lv_arc_set_range(view.arcs[0],0,1000);
                    lv_arc_set_value(view.arcs[0],available?int(int64_t(selected)*1000/maximum):0);
                    lv_obj_set_style_arc_opa(view.arcs[0],available?LV_OPA_COVER:LV_OPA_TRANSP,LV_PART_INDICATOR);
                    uiSetTextFmt(view.marker,"0-%d",maximum/10);
                }
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
            const char *status=!gpsCount_?"No GPS fix":!fix?"GPS stale / no fix":"";
            uiSetText(view.detail,status);
            if(*status)lv_obj_clear_flag(view.detail,LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(view.detail,LV_OBJ_FLAG_HIDDEN);
            if(!gpsCount_){previewGpsBasemap(view.basemap,view.mapViewport,true);continue;}
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

bool TransmitterUi::dashboardBinaryChannel(uint8_t ch,int &off,int &on) const {
    if(ch>=16||ch+1==channelRoleSettings_.steeringChannel||ch+1==channelRoleSettings_.throttleChannel||channelHasMix(channelModel_.mixing[ch]))return false;
    const ControlBinding *found=nullptr;
    for(size_t i=0;i<bindingCount_;++i){const auto &b=bindings_[i];
        if(!b.enabled||b.target!=AssignmentTarget::CrsfChannel||b.crsfChannel!=ch+1)continue;
        const auto kind=physicalControlDescriptor(static_cast<size_t>(b.input)).kind;
        if(found||kind==PhysicalControlKind::SpringCentredTrim||kind==PhysicalControlKind::Proportional)return false;
        if(b.behaviour!=ControlBehaviour::Toggle&&b.behaviour!=ControlBehaviour::TwoPosition&&b.behaviour!=ControlBehaviour::Momentary&&b.behaviour!=ControlBehaviour::Pulse)return false;
        found=&b;
    }
    if(!found)return false;
    const auto transform=[&](int value){return constrain(int(applyChannelResponse(channelModel_.responses[ch],value))+channelModel_.parameters[ch].trimPermille+channelModel_.parameters[ch].subtrimPermille,-1000,1000);};
    off=transform(found->inverted?found->maximum:found->minimum);on=transform(found->inverted?found->minimum:found->maximum);
    return abs(on-off)>50;
}
