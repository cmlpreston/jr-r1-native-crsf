// Shared Drive / Tune / Data / Setup presentation. Included by the LVGL
// translation unit so all screens use one colour, font and focus system.
#include "drive_pack_widget.h"
namespace {
// Temporarily keep GPS controls/navigation visible, including after idle.
constexpr bool kGpsAutoFullscreenEnabled = false;
const char *const designSensorNames[] = {"SPEED", "TEMPERATURE", "RPM", "RX VOLTAGE", "LINK QUALITY", "SATELLITES", "DRIVE PACK", "LOWEST CELL", "CELL 1", "CELL 2", "CELL 3", "CELL 4", "FAS VOLTAGE", "FAS CURRENT", "ERS PACK", "ERS CURRENT", "ERS USED", "ERS REMAINING"};
const char *const speedValueNames[] = {"Current", "Maximum", "Average"};
const char *const speedValueTitles[] = {"SPEED", "MAX SPEED", "AVG SPEED"};
const char *const speedValueShortNames[] = {"Now", "Max", "Avg"};
const char *const designSensorUnits[] = {"km/h", "C", "rpm", "V", "%", "sats", "V", "V", "V", "V", "V", "V", "V", "A", "V", "A", "mAh", "%"};
static_assert(sizeof(designSensorNames)/sizeof(*designSensorNames)==DashboardSensorCount, "Sensor names must cover persisted sources");
static_assert(sizeof(designSensorUnits)/sizeof(*designSensorUnits)==DashboardSensorCount, "Sensor units must cover persisted sources");
const char *const designFieldNames[] = {"Rate", "Expo", "Endpoint", "Response speed", "Direction", "Trim", "Subtrim", "Response offset"};
const char *const designRoleNames[] = {"Steering", "Throttle", "Brake", "Channel", "Channel"};
const char *dashboardKindName(DashboardWidgetKind kind) {
    const char *names[]={"Empty","Timer","TX / RX power","Channels","Sensor","GPS map / speed","Model image","All channels","Battery cells","Reserved","Controls","Trim / rates","RF margin","Drive pack","Power / energy","Control history","Trip computer","Run summary","Battery load / balance","Speed / motor RPM"};
    static_assert(sizeof(names)/sizeof(*names)==static_cast<unsigned>(DashboardWidgetKind::Count),"Widget names must cover stable IDs");
    return names[static_cast<uint8_t>(kind)<static_cast<uint8_t>(DashboardWidgetKind::Count)?static_cast<uint8_t>(kind):0];
}
const char *dashboardStyleName(DashboardWidgetStyle style) {
    const char *names[]={"Numbers","Bars","Dials","Graph","Switches","Vertical bars"};
    return names[static_cast<uint8_t>(style)<static_cast<uint8_t>(DashboardWidgetStyle::Count)?static_cast<uint8_t>(style):0];
}
const bool &designPortrait = portraitUi;
int designFooterY = kScreenHeight - (designPortrait ? 60 : 40);

const lv_font_t *drivePackReadingFont(const char *text,int width,int height) {
    for(auto style:{UiText::Reading,UiText::Heading,UiText::Menu,UiText::Secondary}) {
        const auto font=UiText::font(style);
        if(font->line_height<=height&&lv_txt_get_width(text,strlen(text),font,0,LV_TEXT_FLAG_NONE)<=width)return font;
    }
    return UiText::font(UiText::Caption);
}

lv_obj_t *gpsPositionDot(lv_obj_t *parent){
    auto dot=makeLabel(parent,"",0,0,UiText::Caption,cCyan);
    lv_obj_set_size(dot,16,16);lv_obj_set_style_radius(dot,8,0);
    lv_obj_set_style_bg_color(dot,colour(0x00707d),0);lv_obj_set_style_bg_opa(dot,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(dot,lv_color_white(),0);lv_obj_set_style_border_width(dot,2,0);
    return dot;
}

bool sensorReading(const CrsfTelemetryState &t, uint8_t sensor, int32_t &value, uint32_t &stamp)
{
    switch (sensor) {
        case 0: value = t.groundSpeedDeciKmh; stamp = t.gpsSpeedUpdatedMs; return gpsSpeedAvailable(t,millis());
        case 1: value = t.temperatureDeciC; stamp = t.temperatureUpdatedMs; return t.temperatureValid;
        case 2: value = t.rpm; stamp = t.rpmUpdatedMs; return t.rpmValid;
        case 3: value = t.receiverVoltageDecivolts; stamp = t.batteryUpdatedMs; return t.receiverVoltageValid;
        case 4: value = t.uplinkLinkQuality; stamp = t.linkUpdatedMs; return t.linkValid;
        case 5: value = t.satellites; stamp = t.gpsSatellitesUpdatedMs; return t.gpsSatellitesValid && t.satellites>0;
        case 6: value = t.vehiclePackMv; stamp = t.vehicleCellsUpdatedMs; return t.vehicleCellsValid;
        case 7: value = t.vehicleMinimumCellMv; stamp = t.vehicleCellsUpdatedMs; return t.vehicleCellsValid;
        case 8: case 9: case 10: case 11:
            value = t.vehicleCellsMv[sensor-8]; stamp = t.vehicleCellsUpdatedMs; return t.vehicleCellsValid && sensor-8 < t.vehicleCellCount;
        case 12: value = t.fasVoltageMv; stamp = t.fasVoltageUpdatedMs; return t.fasVoltageValid;
        case 13: value = t.fasCurrentDeciA; stamp = t.fasCurrentUpdatedMs; return t.fasCurrentValid;
        case 14: value=t.ersVoltageDeciV;stamp=t.ersBatteryUpdatedMs;return t.ersBatteryValid;
        case 15: value=t.ersCurrentDeciA;stamp=t.ersBatteryUpdatedMs;return t.ersBatteryValid;
        case 16: value=t.ersCapacityMah;stamp=t.ersBatteryUpdatedMs;return t.ersBatteryValid;
        case 17: value=t.ersRemainingPercent;stamp=t.ersBatteryUpdatedMs;return t.ersBatteryValid && t.ersRemainingValid;
        default: value = 0; stamp = 0; return false;
    }
}
void sensorText(uint8_t sensor, int32_t value, char *text, size_t capacity)
{
    if (sensor >= 6 && sensor <= 12)
        snprintf(text, capacity, "%ld.%03ld", static_cast<long>(value / 1000), static_cast<long>(value % 1000));
    // The selected S.Port temperature source reports whole degrees C; CRSF
    // transports tenths, but that scaling does not add sensor resolution.
    else if (sensor == 1)
        snprintf(text, capacity, "%ld", static_cast<long>(value / 10));
    else if (sensor == 0 || sensor == 3 || sensor == 13 || sensor == 14 || sensor == 15)
        snprintf(text, capacity, "%s%ld.%ld", value < 0 ? "-" : "", labs(value) / 10, labs(value) % 10);
    else snprintf(text, capacity, "%ld", static_cast<long>(value));
}
void boundedLabel(lv_obj_t *label, int width)
{
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}
lv_point_t gpsViewportPoint(const GpsMapViewport &view,int32_t lat,int32_t lon,int width,int height)
{
    return {static_cast<lv_coord_t>(constrain(width/2+GpsMap::east(view.latitude,lon,view.longitude)*view.pixelsPerMetre,-10000,10000)),
            static_cast<lv_coord_t>(constrain(height/2-GpsMap::north(lat,view.latitude)*view.pixelsPerMetre,-10000,10000))};
}
void updateGpsTrail(lv_obj_t *line,lv_point_t *points,uint8_t &renderedCount,
                    const int32_t *lat,const int32_t *lon,uint8_t count,
                    const GpsMapViewport &view,int width,int height,bool visible,
                    GpsMap::TrailProjectionCache &cache,uint32_t generation)
{
    if(!visible || count<2){uiSetHidden(line,true);return;}
    if(renderedCount==count&&cache.unchanged(generation,count,view.latitude,view.longitude,
            view.pixelsPerMetre,view.mapRevision,width,height)){
        uiSetHidden(line,false);return;
    }
    lv_point_t projected[48];
    lv_coord_t left=10000,top=10000;
    const double longitudeScale=GpsMap::longitudeScale(view.latitude);
    for(uint8_t i=0;i<count;++i){
        // Preserve the original operation order while sharing cosine across
        // points. The source generation/viewport key avoids this loop entirely
        // for unchanged trails, including paused/static map displays.
        projected[i]={static_cast<lv_coord_t>(constrain(width/2+(double(lon[i])-view.longitude)*GpsMap::MetresPerE7*longitudeScale*view.pixelsPerMetre,-10000,10000)),
            static_cast<lv_coord_t>(constrain(height/2-GpsMap::north(lat[i],view.latitude)*view.pixelsPerMetre,-10000,10000))};
        left=min(left,projected[i].x);top=min(top,projected[i].y);
    }
    bool changed=renderedCount!=count;
    for(uint8_t i=0;i<count;++i){
        const lv_point_t point={lv_coord_t(projected[i].x-left),lv_coord_t(projected[i].y-top)};
        changed|=point.x!=points[i].x || point.y!=points[i].y;
        points[i]=point;
    }
    // Bound the line to the trail itself, rather than including the empty
    // rectangle from the map's top-left corner to the trail.
    lv_obj_set_pos(line,left,top);
    // LVGL invalidates the entire line bounds on every set_points call.
    if(changed){lv_line_set_points(line,points,count);renderedCount=count;}
    cache.remember(generation,count,view.latitude,view.longitude,view.pixelsPerMetre,view.mapRevision,width,height);
    uiSetHidden(line,false);
}
}

bool TransmitterUi::designPage() const
{
    switch (activePage_) {
        case UiPage::RfWarningSetup: case UiPage::DriveBatterySetup: case UiPage::ErsSensors: case UiPage::TimerControl: case UiPage::QuickNavigation: case UiPage::FieldResetConfirmation: case UiPage::WidgetMetricsReset:
        case UiPage::Dashboard: case UiPage::Tune: case UiPage::ValueEditor: case UiPage::RunReview: case UiPage::BrakeAbs:
        case UiPage::Telemetry: case UiPage::Gps: case UiPage::ChannelMonitor:
        case UiPage::SensorDetail: case UiPage::MainMenu: case UiPage::Channels:
        case UiPage::ChannelRoles: case UiPage::Channel2: case UiPage::ThrottleResponse: case UiPage::ChannelSpeed:
        case UiPage::FirmwareInfo: case UiPage::ElrsBinding: case UiPage::EscProgramming: case UiPage::Calibration:
        case UiPage::RxBatterySetup: case UiPage::TxBatterySetup: case UiPage::DisplaySetup: case UiPage::ModuleLink: case UiPage::OutputStatus: case UiPage::ModeChooser: case UiPage::ChannelPlaceholder:
        case UiPage::ChannelInput: case UiPage::ChannelResponseSetup: case UiPage::ParameterControlPicker: case UiPage::ResponseProfile: case UiPage::MixingOverview:
        case UiPage::ControlAssignments: case UiPage::BindingEditor: case UiPage::InputMonitor: case UiPage::ColourScheme: case UiPage::ElrsTxConfig:
        case UiPage::NetworkOptions: case UiPage::Setup: case UiPage::SystemMenu: case UiPage::DriveModes:
        case UiPage::ModelSelect: case UiPage::WidgetSelect: case UiPage::WidgetEditor: case UiPage::BatteryMenu: return true;
        default: return false;
    }
}

void TransmitterUi::resetSpeedStatistics()
{
    speedStatistics_.reset(designTelemetry_.gpsSpeedUpdatedMs);
    if(!telemetryHistory_)return;
    memset(sensorHistoryValid_[0], 0, sizeof(sensorHistoryValid_[0]));
    sensorStatistics_[0].reset(designTelemetry_.gpsSpeedUpdatedMs);
}

void TransmitterUi::resetSensorStatistics(uint8_t sensor){
    if(sensor>=DashboardSensorCount)return;
    if(!telemetryHistory_){if(sensor==0)resetSpeedStatistics();return;}
    int32_t value=0;uint32_t stamp=0;sensorReading(designTelemetry_,sensor,value,stamp);
    sensorStatistics_[sensor].reset(stamp);memset(sensorHistoryValid_[sensor],0,sizeof(sensorHistoryValid_[sensor]));
    if(sensor==0)resetSpeedStatistics();
}

void TransmitterUi::refreshTuneParameters()
{
    if(activePage_!=UiPage::Tune || tuneBank_!=1)return;
    for(uint8_t field=5;field<=6;++field){
        if(!designValues_[field])continue;
        const int value=channelParameterValue(selectedChannel_,field==5?ChannelParameterKind::Trim:ChannelParameterKind::Subtrim);
        uiSetTextFmt(designValues_[field],"%+.1f%%",value/10.0);
        auto bar=designBars_[field-5];
        if(bar && lv_bar_get_value(bar)!=value)lv_bar_set_value(bar,value,LV_ANIM_OFF);
    }
}

void TransmitterUi::toggleRunTimer()
{
    timerAction(0);
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
        setChannelEndpointForOutput(p, false, steeringEndpointDrafts_[0]);
        setChannelEndpointForOutput(p, true, steeringEndpointDrafts_[1]);
        channelModel_.setResponse(selectedChannel_, p);
        if (selectedChannel_ == steeringChannel()) {
            // Commit the live preview without re-arming or blocking on flash.
            // Storage is deferred until the existing acknowledged-CUT save path.
            settingsSavePending_ = true;
            settingsSaveDueMs_ = millis() + kSettingsSaveDelayMs;
        } else scheduleSettingsSave();
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
    scheduleSettingsSave(editorField_!=1);
}

void TransmitterUi::openDesignEditor(uint8_t field, UiPage returnPage)
{
    editorField_ = field;
    editorReturn_ = returnPage;
    editorDraft_ = designValue();
    expoPreviewActive_ = false;
    editorFine_ = false;
    if (field == 2 && tuneRole_ == 0) {
        const auto &profile = profileForChannel(selectedChannel_);
        steeringEndpointDrafts_[0] = channelEndpointForOutput(profile, false);
        steeringEndpointDrafts_[1] = channelEndpointForOutput(profile, true);
        steeringEndpointInputSide_ = 0;
        int input = latestSteeringRawAdc_ <= 4095 ? steeringInput_.mapToPermille(latestSteeringRawAdc_) : 0;
        if (profile.reversed) input = -input;
        if (input < -150) steeringEndpointInputSide_ = -1;
        else if (input > 150) steeringEndpointInputSide_ = 1;
        if (steeringEndpointInputSide_) steeringEndpointSide_ = steeringEndpointInputSide_ > 0;
        editorDraft_ = steeringEndpointDrafts_[steeringEndpointSide_];
        editorFine_ = true;
    }
    drawDesignPage(UiPage::ValueEditor);
}

bool TransmitterUi::liveExpoEditor() const
{
    return activePage_==UiPage::ValueEditor&&editorField_==1&&tuneRole_<=4&&
        selectedChannel_>=1&&selectedChannel_<=CRSF_CHANNEL_COUNT&&
        editorDraft_>=CHANNEL_RESPONSE_MIN_EXPO_PERCENT&&editorDraft_<=CHANNEL_RESPONSE_MAX_EXPO_PERCENT;
}

bool TransmitterUi::steeringEndpointEditor() const
{
    return activePage_ == UiPage::ValueEditor && editorField_ == 2 && tuneRole_ == 0 &&
           selectedChannel_ == steeringChannel();
}

void TransmitterUi::selectSteeringEndpoint(uint8_t side)
{
    if (!steeringEndpointEditor() || side > 1 || side == steeringEndpointSide_) return;
    steeringEndpointSide_ = side;
    editorDraft_ = steeringEndpointDrafts_[side];
    lastInteractionMs_ = millis();
    refreshSteeringEndpointEditor(false);
}

void TransmitterUi::refreshSteeringEndpointEditor(bool curveChanged)
{
    if(steeringEndpointEditor())refreshValueEditor(curveChanged);
}

void TransmitterUi::refreshValueEditor(bool curveChanged)
{
    if(activePage_!=UiPage::ValueEditor)return;
    // Keep buttons and touch targets alive while editing; only dirty pixels
    // for changed labels, the selected side and the response curve.
    for (uint8_t side=0; side<2; ++side) {
        auto button=steeringEndpointButtons_[side];
        if (!button) continue;
        uiSetTextFmt(lv_obj_get_child(button,0), "%s%s  %d%%", side?"Right":"Left",
            side==steeringEndpointSide_?" *":"",steeringEndpointDrafts_[side]);
        uiSetBgColor(button,colour(side==steeringEndpointSide_?cPanel2:cPanel),0);
        uiSetTextColor(lv_obj_get_child(button,0),colour(side==steeringEndpointSide_?cCyan:cWhite),0);
    }
    if(editorValueLabel_){
        if(editorField_==4)uiSetText(editorValueLabel_,editorDraft_?"Reversed":"Normal");
        else uiSetTextFmt(editorValueLabel_,"%d%s",editorDraft_,editorField_==3?" ms":"%");
    }
    if (editorStepButton_) uiSetText(lv_obj_get_child(editorStepButton_,0),editorFine_?"Fine":"Coarse");
    if (curveChanged && designCurve_) {
        auto draft=profileForChannel(selectedChannel_);
        const bool endpoints=steeringEndpointEditor();
        const bool pos=tuneRole_!=2&&tuneRole_!=4,neg=tuneRole_==0||tuneRole_==2||tuneRole_==4;
        switch(editorField_){
            case 0:if(pos)draft.positiveRatePercent=editorDraft_;if(neg)draft.negativeRatePercent=editorDraft_;break;
            case 1:if(expoPreviewActive_){if(pos)draft.positiveExpoPercent=editorDraft_;if(neg)draft.negativeExpoPercent=editorDraft_;}break;
            case 2:
                if(endpoints){setChannelEndpointForOutput(draft,false,steeringEndpointDrafts_[0]);setChannelEndpointForOutput(draft,true,steeringEndpointDrafts_[1]);}
                else{if(pos)draft.positiveEndpointPercent=editorDraft_;if(neg)draft.negativeEndpointPercent=editorDraft_;}
                break;
            case 4:draft.reversed=editorDraft_!=0;break;
            case 7:draft.subtrimPercent=editorDraft_;break;
        }
        const int graphH=designPortrait?(endpoints?118:150):60;
        lv_point_t points[33];
        for (int i=0;i<33;++i) {
            const int output=applyChannelResponse(draft,-1000+i*2000/32);
            points[i]={static_cast<lv_coord_t>(4+i*(kScreenWidth-34)/32),static_cast<lv_coord_t>(graphH/2-output*(graphH-8)/2400)};
        }
        uiSetLinePoints(designCurve_,designPoints_,points,33);
    }
    lastInteractionMs_=millis();
}

void TransmitterUi::updateSteeringEndpointSelection()
{
    if (!channelRoleSettings_.steeringEnabled || !steeringEndpointEditor() || latestSteeringRawAdc_ > 4095) return;
    // Follow the calibrated direction after reversal, independent of trim/rate.
    // Hysteresis rejects centre noise. A tap overrides a held wheel until the
    // wheel enters a new side; returning to centre retains the selected side.
    int input = steeringInput_.mapToPermille(latestSteeringRawAdc_);
    if (profileForChannel(selectedChannel_).reversed) input = -input;
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
    if(kGpsAutoFullscreenEnabled&&activePage_==UiPage::Gps&&!gpsFullScreen_&&!touchWasDown_&&now-lastInteractionMs_>=4000){
        gpsFullScreen_=true;drawDesignPage(UiPage::Gps);
    }
}
void TransmitterUi::drawGpsMapView(bool fullScreen)
{
    const int x=fullScreen?0:10,y=fullScreen?0:84;
    const int width=fullScreen?kScreenWidth:kScreenWidth-20;
    const int height=fullScreen?kScreenHeight:designFooterY-y-4;
    designMapViewport_=GpsMapViewport{};
    gpsRenderedCount_=0;
    auto map=createGpsBasemap(lv_scr_act(),x,y,width,height,designMapViewport_);
    designCurve_=lv_line_create(map);lv_obj_set_style_line_color(designCurve_,colour(0x00707d),0);lv_obj_set_style_line_width(designCurve_,2,0);
    designMarker_=gpsPositionDot(map);lv_obj_add_flag(designMarker_,LV_OBJ_FLAG_HIDDEN);
    makeLabel(map,"N ^",8,6,UiText::Caption,0x405442);
    designDetails_[1]=makeLabel(map,"",width-70,6,UiText::Caption,0x405442);
    if(!fullScreen){
        designDetails_[0]=makeLabel(map,"No GPS position",8,24,UiText::Caption,0x405442);
        boundedLabel(designDetails_[0],width-18);
        const int controlsY=y+height-50,left=x+width-220;
        makePageActionButton(left,controlsY,32,30,"-",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,82);
        makePageActionButton(left+36,controlsY,32,30,"+",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,83);
        makePageActionButton(left+72,controlsY,76,30,gpsTrailVisible_?"Trail on":"Trail off",UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,84);
        makePageActionButton(left+152,controlsY,68,30,"Reset",UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,85);
    }
}

// Shared list row for menus, choices, editable settings and read-only monitors.
// Rendering and focus/touch bounds are built together; portrait uses two lines.
lv_obj_t *TransmitterUi::drawMenuRow(const char *title,const char *value,int y,int height,
                                    PageControlHandler handler,uint8_t action,bool chevron,
                                    int trailing,const char *subtitle,lv_obj_t **subtitleLabel)
{
    const int width=kScreenWidth-20,line=UiText::font(UiText::Menu)->line_height;
    auto panel=makePanel(10,y,width,height,cBlack,cLine,0);
    lv_obj_set_style_border_side(panel,LV_BORDER_SIDE_BOTTOM,0);
    auto name=makeLabel(panel,title,10,designPortrait? (subtitle?2:6):subtitle?0:(height-line)/2,UiText::Menu,cWhite);
    boundedLabel(name,designPortrait||!*value?width-38:width/2-12);
    auto v=makeLabel(panel,value,0,0,UiText::Menu,cCyan);
    if(designPortrait){lv_obj_set_pos(v,10,subtitle?line+3:height-line-4);boundedLabel(v,width-trailing-12);}
    else{
        boundedLabel(v,width/2-trailing-12);lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_RIGHT,0);
        if(subtitle)lv_obj_set_pos(v,width/2+12,0);
        else lv_obj_align(v,LV_ALIGN_RIGHT_MID,-trailing,0);
    }
    if(subtitle){
        auto detail=makeLabel(panel,subtitle,10,height-UiText::font(UiText::Caption)->line_height-2,UiText::Caption,cDim);
        boundedLabel(detail,width-20);if(subtitleLabel)*subtitleLabel=detail;
    }
    if(chevron)makeLabel(panel,LV_SYMBOL_RIGHT,width-20,(height-16)/2,UiText::Caption,cDim);
    if(handler)addPageActionTarget(10,y,width,height,handler,action);
    return v;
}

lv_obj_t *TransmitterUi::drawSettingsRow(const char *title,const char *value,int y,int height,
                                    PageControlHandler handler,uint8_t action,bool numeric)
{
    auto valueLabel=drawMenuRow(title,value,y,height,numeric?nullptr:handler,action,false,numeric?104:10);
    if(numeric){
        const int bx=kScreenWidth-102,by=y+(height-32)/2;
        makeButton(bx,by,36,32,"-",UiText::Menu);
        makeButton(bx+44,by,36,32,"+",UiText::Menu);
        addPageEditorTarget(10,y,kScreenWidth-20,height,bx,by,36,32,bx+44,by,36,32,handler,action);
    }
    return valueLabel;
}

void TransmitterUi::drawDesignPage(UiPage page)
{
    if(page==UiPage::WifiSetup){drawWifiPage();return;}
    const bool leavingWifi=activePage_==UiPage::WifiSetup||
        (activePage_==UiPage::QuickNavigation&&keypadReturnPage_==UiPage::WifiSetup);
    if(leavingWifi&&page!=UiPage::QuickNavigation)leaveWifiPage();
    if(page!=UiPage::SensorDetail || activePage_!=UiPage::SensorDetail)v2GraphPaused_=false;
    if(page!=UiPage::Dashboard)maximizedWidget_=-1;
    if(page==UiPage::Gps&&activePage_!=UiPage::Gps){gpsFullScreen_=false;lastInteractionMs_=millis();}
    activePage_ = page;
    startScreen(); resetLvglObjects(); beginPageFocus(false);
    auto button = [this](int x, int y, int w, int h, const char *label, uint8_t action, bool selected = false) {
        return makePageActionButton(x,y,w,h,label,UiText::Menu, selected ? cPanel2 : cPanel,
            selected ? cCyan : cWhite,cLine,&TransmitterUi::controlDesign,action);
    };
    auto label = [](const char *text, int x, int y, UiText::Style style = UiText::Secondary, uint32_t col = cWhite) {
        return makeLabel(lv_scr_act(), text, x,y,style,col);
    };
    auto row = [&](int y, const char *title, const char *value, uint8_t action, int height = 39) {
        return drawMenuRow(title,value,y,height,&TransmitterUi::controlDesign,action);
    };
    if(page==UiPage::Gps&&gpsFullScreen_){
        drawGpsMapView(true);finishPageFocus();
        updateDesign(designTelemetry_,designSafety_,designTxVoltage_,designTxValid_);return;
    }
    makePanel(0,0,kScreenWidth,44,cHeader,cHeader,0);
    const bool rootPage=page==UiPage::Dashboard||page==UiPage::Tune||page==UiPage::Telemetry||page==UiPage::Gps||page==UiPage::ChannelMonitor||page==UiPage::MainMenu;
    if(!rootPage && (!navigationIsModal()||page==UiPage::ElrsBinding||page==UiPage::Calibration)){
        makePageActionButton(8,5,76,34,page==UiPage::Calibration?"Cancel":"Back",UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlNavigation,0);
    }
    const int modelX=rootPage?47:94;
    if(rootPage){
        if (const auto *art=modelImage(modelImages_[activeModelSlot_],modelColours_[activeModelSlot_],modelAccents_[activeModelSlot_],modelBases_[activeModelSlot_])) {
            auto logo=lv_img_create(lv_scr_act());lv_img_set_src(logo,art);lv_img_set_zoom(logo,min(36*256/int(art->header.w),30*256/int(art->header.h)));
            lv_img_set_pivot(logo,0,0);lv_obj_set_pos(logo,5,4);
        }
    }
    auto modelLabel=label(modelNames_[activeModelSlot_],modelX,10,UiText::Secondary,0xe7edef);
    boundedLabel(modelLabel,kScreenWidth-modelX-176);
    lv_obj_set_height(modelLabel,UiText::font(UiText::Secondary)->line_height);
    makeHeaderTxVoltage(kScreenWidth-168,12);
    makeHeaderGpsTime(kScreenWidth-168,26);
    if(brakeAbs_.enabled)label("ABS",kScreenWidth-84,26,UiText::Caption,cAmber);
    designLink_=label("LQ --",kScreenWidth-104,12,UiText::Caption,cDim);boundedLabel(designLink_,60);
    makeWifiStatusIcon(kScreenWidth-33,11);
    if(!navigationIsModal())addPageActionTarget(kScreenWidth-110,0,72,38,&TransmitterUi::controlDesign,63);
    const bool data = page == UiPage::Telemetry || page == UiPage::Gps || page == UiPage::ChannelMonitor || page == UiPage::SensorDetail;
    const UiPage sectionPage=page==UiPage::TimerControl?UiPage::Dashboard:page==UiPage::OutputStatus?statusReturnPage_:page;
    const bool dataSection=sectionPage==UiPage::Telemetry||sectionPage==UiPage::Gps||sectionPage==UiPage::ChannelMonitor||sectionPage==UiPage::SensorDetail||sectionPage==UiPage::RunReview||(sectionPage==UiPage::InputMonitor&&inputMonitorReturnPage_==UiPage::Telemetry);
    const uint8_t nav=(sectionPage==UiPage::Dashboard||sectionPage==UiPage::ModeChooser)?0:(sectionPage==UiPage::Tune||sectionPage==UiPage::ValueEditor||sectionPage==UiPage::BrakeAbs)?1:dataSection?2:3;
    if(!navigationIsModal())drawSectionNavigation(nav);
    if(page==UiPage::QuickNavigation||page==UiPage::FieldResetConfirmation||page==UiPage::TimerControl){drawKeypadPage();}
    else if(page==UiPage::WidgetMetricsReset){
        const bool trip=widgetMetricsResetKind_==DashboardWidgetKind::TripComputer || widgetMetricsResetKind_==DashboardWidgetKind::RunSummary;
        const bool battery=widgetMetricsResetKind_==DashboardWidgetKind::BatteryLoadBalance;
        label(trip?"Reset trip and run?":battery?"Reset battery extremes?":"Reset power / energy?",12,designPortrait?82:66,UiText::Heading);
        auto help=label(trip?"Clear both Trip computer and Run summary\nfor this model session?\n\nSensor counters and settings are unchanged.":
            battery?"Clear observed minimum voltage and peak\ncurrent for this model session?\n\nBattery warnings and settings are unchanged.":
            "Clear peak power, energy and efficiency\nfor this model session?\n\nCU01 readings and settings are unchanged.",12,designPortrait?138:108,UiText::Secondary,cDim);
        lv_obj_set_width(help,kScreenWidth-24);
        const int yy=designFooterY-(designPortrait?58:36);
        button(12,yy,(kScreenWidth-36)/2,designPortrait?44:30,"Cancel",158);
        button(24+(kScreenWidth-36)/2,yy,(kScreenWidth-36)/2,designPortrait?44:30,"Reset",159,true);
    }
    else if(page==UiPage::RunReview){drawV2RunReview();}
    else if(page==UiPage::ControlAssignments){drawControlAssignmentsContent();}
    else if(page==UiPage::BindingEditor){drawBindingEditorContent();}
    else if(page==UiPage::InputMonitor){drawInputMonitorContent();}
    else if(page==UiPage::ColourScheme){drawColourSchemeContent();}
    else if(page==UiPage::ElrsTxConfig){drawElrsTxConfigContent();}
    else if(page==UiPage::BrakeAbs||page==UiPage::ChannelRoles||page==UiPage::Channel2||page==UiPage::ThrottleResponse||page==UiPage::ChannelSpeed||page==UiPage::FirmwareInfo||page==UiPage::ElrsBinding||page==UiPage::EscProgramming||page==UiPage::Calibration){drawSetupDetailsContent();}
    else if (page == UiPage::Dashboard) {
        lv_obj_t *minimize=nullptr;
        if(maximizedWidget_>=0){
            // Register before the widget's broad touch target, then raise the
            // overlay after drawing so taps reach the visible chevron.
            minimize=button(kScreenWidth/2-20,50,40,22,LV_SYMBOL_DOWN,243);
            lv_obj_set_style_bg_opa(minimize,LV_OPA_80,0);
            lv_obj_set_style_border_width(minimize,0,0);
            lv_obj_set_style_radius(minimize,11,0);
        }
        drawDashboardWidgets();
        if(minimize)lv_obj_move_foreground(minimize);
    } else if(page==UiPage::RfWarningSetup){
        label("Model RF warnings",12,48,UiText::Heading);
        const char *names[]={"Warn while driving","2.4 GHz ELRS version","RSSI margin alarm","LQ alarm at / below"};
        const auto &s=rfWarningSettings_;
        for(uint8_t n=0;n<4;++n){char value[32];
            if(n==0)snprintf(value,sizeof(value),"%s",s.enabled?"On":"Off");
            if(n==1)snprintf(value,sizeof(value),"%u.x",s.elrsMajor);
            if(n==2)snprintf(value,sizeof(value),"%u dB",s.marginDb);
            if(n==3)snprintf(value,sizeof(value),"%u%%",s.minimumLq);
            drawSettingsRow(names[n],value,80+n*(designPortrait?70:40),designPortrait?68:38,&TransmitterUi::controlRfWarning,n,n>=2);
        }
        boundedLabel(label("1s dwell; unknown mode uses LQ only.",12,designPortrait?378:252,UiText::Caption,cDim),kScreenWidth-24);
    } else if(page==UiPage::DriveBatterySetup || page==UiPage::RxBatterySetup){
        drawVehicleBatteryContent();
    } else if(page==UiPage::TxBatterySetup) {
        label("TX battery",12,48,UiText::Heading);
        const char *names[]={"Chemistry","Cell count","Low alarm","Voltage sensing"};
        const int step=designPortrait?70:40;
        for(uint8_t i=0;i<4;++i){
            char value[32];
            if(i==0)snprintf(value,sizeof(value),"%s >",batteryChemistryName(txBatterySettings_.chemistry));
            else if(i==1)snprintf(value,sizeof(value),"%uS",txBatterySettings_.cellCount);
            else if(i==2)snprintf(value,sizeof(value),"%u.%u V",txBatterySettings_.lowAlarmDecivolts/10,txBatterySettings_.lowAlarmDecivolts%10);
            else snprintf(value,sizeof(value),"%s >",txBatterySettings_.voltageSenseEnabled?"On":"Off");
            drawSettingsRow(names[i],value,80+i*step,step-2,&TransmitterUi::controlTxBattery,i,i==1||i==2);
        }
        boundedLabel(label("Voltage sensing reads the TX battery feed.",20,designPortrait?378:252,UiText::Caption,cDim),kScreenWidth-40);
    } else if(page==UiPage::DisplaySetup) {
        label("Display",12,48,UiText::Heading);
        const char *names[]={"Screen timeout","Idle action","Brightness","Colour scheme","Activate now","Orientation"};
        const int first=displaySetupBank_*4,step=designPortrait?70:40;
        for(int i=first;i<min(first+4,6);++i){
            char value[40];
            if(i==0){if(systemSettings_.screenTimeoutSeconds)snprintf(value,sizeof(value),"%u sec",systemSettings_.screenTimeoutSeconds);else strcpy(value,"Never");}
            else if(i==1)snprintf(value,sizeof(value),"%s >",systemSettings_.logoScreensaverEnabled?"Logo saver":"Display off");
            else if(i==2)snprintf(value,sizeof(value),"%u%%",systemSettings_.displayBrightnessPercent);
            else if(i==3)snprintf(value,sizeof(value),"%s >",colourSchemeName(systemSettings_.nightMode,systemSettings_.darkColourScheme));
            else if(i==4)snprintf(value,sizeof(value),"%s >",systemSettings_.logoScreensaverEnabled?"Show logo":"Display off");
            else snprintf(value,sizeof(value),"%s >",designPortrait?"Portrait":"Landscape");
            drawSettingsRow(names[i],value,80+(i-first)*step,step-2,&TransmitterUi::controlDisplay,i,i==0||i==2);
        }
        if(displaySetupBank_){
            boundedLabel(label("Logo saver dims to 25%.",20,designPortrait?252:184,UiText::Caption,cDim),kScreenWidth-40);
            boundedLabel(label("Display off saves the most power.",20,designPortrait?278:207,UiText::Caption,cDim),kScreenWidth-40);
        }
        drawPageControls(displaySetupBank_,2);
    } else if(page==UiPage::ErsSensors) {
        drawErsSensorContent();
    } else if(page==UiPage::ModuleLink) {
        label("Module link",12,52,UiText::Heading);
        label(moduleLink_.enabled()?"Pause module communication?":moduleLink_.present()?"Enable module communication?":"TX battery not detected",12,100,UiText::Menu,cCyan);
        const bool live=armRequested_||designSafety_==OutputSafetyStatus::Armed;
        label(moduleLink_.enabled()&&live?"Apply CUT before pausing module link.":"Enabling starts CRSF. Outputs stay CUT.",12,142,UiText::Caption,cWhite);
        label("Pausing stops all CRSF traffic.",12,166,UiText::Caption,cWhite);
        label("Ranger RF stop needs bench verification.",12,190,UiText::Caption,cAmber);
        const int by=designPortrait?300:235;
        button(12,by,(kScreenWidth-32)/2,36,"Cancel",241);
        if(moduleLink_.enabled()&&!live)button(kScreenWidth/2+4,by,(kScreenWidth-32)/2,36,"Pause",242);
        else if(!moduleLink_.enabled()&&moduleLink_.present())button(kScreenWidth/2+4,by,(kScreenWidth-32)/2,36,"Enable",240);
    } else if(page==UiPage::OutputStatus) {
        label("Output status",12,48,UiText::Heading);
        designDetails_[0]=label("",12,94,UiText::Menu,cCyan);boundedLabel(designDetails_[0],kScreenWidth-24);
        designDetails_[1]=label("",12,designPortrait?138:126,UiText::Secondary,cWhite);boundedLabel(designDetails_[1],kScreenWidth-24);
        label("Physical handset only. Unlocking never arms.",12,designPortrait?190:158,UiText::Caption,cDim);
        auto arm=button(12,designPortrait?238:184,kScreenWidth-24,48,"Enable output",181);
        armProgress_=makeBar(arm,4,41,kScreenWidth-32,4,0,800);
        if(SetupPortal::active())button(12,designPortrait?296:238,kScreenWidth-24,32,"Finish Wi-Fi setup",238);
        else if(moduleLinkManaged_)button(12,designPortrait?296:238,kScreenWidth-24,32,"Module link",239);

    } else if(page==UiPage::ModeChooser || page==UiPage::DriveModes) {
        label(page==UiPage::ModeChooser?"Choose mode":"Drive modes",12,48,UiText::Heading);
        const char *modes[]={"Normal","Walk"};const uint8_t actions[]={183,182};
        drawPageTabs(modes,actions,2,driveModeSettings_.activeMode==DriveMode::Walk?1:0,&TransmitterUi::controlDesign,96);
        label("Mode changes CUT output. Re-arm at neutral.",12,designPortrait?174:154,UiText::Caption,cAmber);
        if(page==UiPage::ModeChooser)button(12,designPortrait?224:184,kScreenWidth-24,40,"Mode settings",184);
        else {
            char limit[32];snprintf(limit,sizeof(limit),"Walk forward limit %u%%",driveModeSettings_.walkLimitPercent);
            label(limit,12,designPortrait?228:185,UiText::Secondary);
            button(kScreenWidth-120,designPortrait?222:176,48,40,"-",219);button(kScreenWidth-64,designPortrait?222:176,48,40,"+",220);
            label("Brake / reverse retain full range.",12,designPortrait?296:230,UiText::Caption,cDim);
            button(12,designFooterY-31,kScreenWidth-24,26,"Output status",5);
        }

    } else if(page==UiPage::ChannelPlaceholder) {
        char title[40];snprintf(title,sizeof(title),"CH%u - %s",selectedChannel_,selectedChannel_==steeringChannel()?"Steering":selectedChannel_==throttleChannel()?"Throttle":"Auxiliary");label(title,12,48,UiText::Heading);
        const char *names[]={"Role / physical input","Response / trim","Mixing / output limits"};
        for(int i=0;i<3;++i)row(90+i*(designPortrait?72:44),names[i],"",186+i,designPortrait?62:40);

    } else if(page==UiPage::ChannelInput) {
        char title[36];snprintf(title,sizeof(title),"CH%u input",selectedChannel_);label(title,12,48,UiText::Heading);
        const bool steering=selectedChannel_==steeringChannel(),throttle=selectedChannel_==throttleChannel();
        if(!channelInputPage_) {
            row(90,"Role",steering?"Steering":throttle?"Throttle":"Auxiliary",189,designPortrait?58:34);
            row(designPortrait?158:126,"Physical control",steering?"Steering wheel":throttle?"Throttle trigger":"Assignments",190,designPortrait?58:34);
            row(designPortrait?226:162,"Input enabled",steering?(steeringEnabled()?"On":"Off"):throttle?(throttleEnabled()?"On":"Off"):"Per assignment",191,designPortrait?58:34);
            row(designPortrait?294:198,"Calibration / deadband",channelHasProportionalControl(selectedChannel_)?"Configure":"Not applicable",192,designPortrait?58:34);
        }else{
            button(12,90,kScreenWidth-24,44,"Assign steering to this channel",193);
            button(12,144,kScreenWidth-24,44,"Assign throttle to this channel",194);
            label(channelRoleConflict_?"Channel already owned; choose a free channel.":"Moves the role from its previous channel; forces CUT.",12,designPortrait?222:202,UiText::Caption,channelRoleConflict_?cAmber:cDim);
            boundedLabel(label("One channel per role. Auxiliary channels use control assignments.",12,designPortrait?264:224,UiText::Caption,cDim),kScreenWidth-24);
        }

    } else if(page==UiPage::ChannelResponseSetup) {
        char title[36];snprintf(title,sizeof(title),"CH%u response",selectedChannel_);label(title,12,48,UiText::Heading);
        const char *names[]={"Quick adjustments","Curve / detailed response","Trim control","Subtrim control"};
        for(int i=0;i<4;++i){char value[48]="";
            if(i>=2){auto kind=i==2?ChannelParameterKind::Trim:ChannelParameterKind::Subtrim;int index=bindingIndexForChannelParameter(selectedChannel_,kind);snprintf(value,sizeof(value),"%s",index>=0?physicalControlDescriptor(static_cast<size_t>(bindings_[index].input)).name:"None");}
            row(88+i*(designPortrait?68:38),names[i],value,195+i,designPortrait?58:36);
        }

    } else if(page==UiPage::ResponseProfile) {
        char title[32];snprintf(title,sizeof(title),"CH%u response",selectedChannel_);label(title,12,48,UiText::Heading);
        const bool steering=selectedChannel_==steeringChannel(),throttle=selectedChannel_==throttleChannel();
        const char *tabs[]={steering?"Right":throttle?"Forward":"Positive",steering?"Left":throttle?"Brake":"Negative"};
        const uint8_t actions[]={231,232};drawPageTabs(tabs,actions,2,tuneRole_==4?1:0,&TransmitterUi::controlDesign,82);
        const uint8_t fields[2][3]={{0,1,2},{3,4,7}};
        for(uint8_t i=0;i<3;++i){
            const auto field=fields[responseDetailBank_][i];editorField_=field;char value[32];
            if(field==4)snprintf(value,sizeof(value),"%s",designValue()?"Reversed":"Normal");
            else snprintf(value,sizeof(value),"%d%s",designValue(),field==3?" ms":"%");
            row(126+i*(designPortrait?68:36),designFieldNames[field],value,233+i,designPortrait?58:34);
        }

        button(kScreenWidth-152,designFooterY-42,140,34,responseDetailBank_?"< Basic":"More >",236);
    } else if(page==UiPage::ParameterControlPicker) {
        char title[48];snprintf(title,sizeof(title),"CH%u / %s control",selectedChannel_,parameterControlKind_==ChannelParameterKind::Trim?"trim":"subtrim");
        label(title,12,48,UiText::Menu);
        for(uint8_t i=0;i<6;++i){
            auto input=kParameterControls[i];const int assigned=input==PhysicalControlId::Count?-1:bindingIndexForInput(input);
            const int current=bindingIndexForChannelParameter(selectedChannel_,parameterControlKind_);
            const bool busy=assigned>=0&&assigned!=current;
            const int columns=designPortrait?1:2,w=(kScreenWidth-24-(columns-1)*8)/columns;
            const int x=12+(i%columns)*(w+8),y=(designPortrait?100:94)+(i/columns)*44;
            auto choice=button(x,y,w,40,input==PhysicalControlId::Count?"None":physicalControlDescriptor(static_cast<size_t>(input)).name,224+i,input==parameterControlDraft_);
            lv_obj_set_style_text_font(lv_obj_get_child(choice,0),UiText::font(UiText::Menu),0);
            if(busy)uiSetTextColor(lv_obj_get_child(choice,0),colour(cDim),0);
        }
        boundedLabel(label(parameterControlError_?"Control is already assigned; choose a free one.":"Grey controls are in use. Changes wait for Apply.",12,designFooterY-56,UiText::Caption,parameterControlError_?cAmber:cDim),kScreenWidth-24);
        button(12,designFooterY-42,100,34,"Cancel",209);
        button(kScreenWidth-124,designFooterY-42,112,34,"Apply",230,true);
    } else if(page==UiPage::MixingOverview) {
        char title[36];snprintf(title,sizeof(title),"CH%u mixing",selectedChannel_);label(title,12,48,UiText::Heading);
        const char *names[]={"Mix sources","Activation","Switched rates","Baseline / limits","Live inputs / graph"};
        for(int i=0;i<5;++i)row(83+i*(designPortrait?57:30),names[i],"",199+i,designPortrait?50:28);
        button(12,designFooterY-42,100,34,"Cancel",209);button(kScreenWidth-124,designFooterY-42,112,34,mixingSaveError_?"Retry apply":"Apply",204);
    } else if(page==UiPage::Setup) {
        label("Sounds / alerts",12,48,UiText::Heading);
        char pitch[24],length[24];snprintf(pitch,sizeof(pitch),"%u Hz",systemSettings_.buzzerPitchHz);snprintf(length,sizeof(length),"%u ms",systemSettings_.keyClickDurationMs);
        const char *names[]={"Buzzer","Key clicks","Pitch","Click length"};const char *values[]={systemSettings_.buzzerEnabled?"On":"Off",systemSettings_.keyClicksEnabled?"On":"Off",pitch,length};
        for(int i=0;i<4;++i)row(85+i*(designPortrait?64:36),names[i],values[i],213+i,designPortrait?56:34);
        button(kScreenWidth-124,designFooterY-42,112,34,"Test sound",217);
    } else if(page==UiPage::NetworkOptions || page==UiPage::SystemMenu) {
        const bool network=page==UiPage::NetworkOptions;label(network?"Network / web":"System",12,48,UiText::Heading);
        if(network){row(94,"Home Wi-Fi","Connection",210,designPortrait?64:44);row(designPortrait?170:148,"Web access","Address / credentials",211,designPortrait?64:44);}
        else{row(designPortrait?90:82,"Input diagnostics","Monitor",48,designPortrait?60:34);row(designPortrait?162:120,"Firmware information","",144,designPortrait?60:34);row(designPortrait?234:158,"PIN","Change / recovery",237,designPortrait?60:34);row(designPortrait?306:196,"Restart handset","",212,designPortrait?60:34);}

    } else if (page == UiPage::Tune) {
        if (tuneRole_ < 3) selectedChannel_ = tuneRole_ == 0 ? channelRoleSettings_.steeringChannel : channelRoleSettings_.throttleChannel;
        if(tuneRole_<3){const uint8_t actions[]={10,11,12};drawPageTabs(designRoleNames,actions,3,tuneRole_,&TransmitterUi::controlDesign);}
        else {const char *names[]={"Positive","Negative"};const uint8_t actions[]={221,222};drawPageTabs(names,actions,2,tuneRole_==3?0:1,&TransmitterUi::controlDesign);}
        const int gap = designPortrait ? 62 : 40;
        for (uint8_t i=0;i<4;++i) {
            const uint8_t field = tuneBank_*4+i;
            if (field > 6) break;
            editorField_ = field;
            char value[32]; int v=designValue();
            if (field == 2 && tuneRole_ == 0) {
                const auto &p = profileForChannel(selectedChannel_);
                snprintf(value,sizeof(value),"L %u / R %u%%",channelEndpointForOutput(p,false),channelEndpointForOutput(p,true));
            }
            else if (field == 4) snprintf(value,sizeof(value),"%s",v ? "Reversed" : "Normal");
            else snprintf(value,sizeof(value),"%d%s",v,field==3 ? " ms" : "%");
            auto valueLabel=row(86+i*gap,designFieldNames[field],value,20+field,designPortrait ? 54 : 37);
            if(field==5 || field==6){
                designValues_[field]=valueLabel;
                auto panel=lv_obj_get_parent(valueLabel);
                designBars_[field-5]=makeBar(panel,designPortrait?106:10,designPortrait?43:30,
                    designPortrait?kScreenWidth-146:kScreenWidth-60,4,-200,200);
                lv_bar_set_mode(designBars_[field-5],LV_BAR_MODE_SYMMETRICAL);
                auto zero=makePanel(0,0,1,8,cDim,cDim,0);lv_obj_set_parent(zero,panel);
                lv_obj_set_pos(zero,(designPortrait?106:10)+(designPortrait?kScreenWidth-146:kScreenWidth-60)/2,designPortrait?41:28);
            }
        }
        if(tuneRole_==2 && tuneBank_==1)row(designPortrait?276:206,"ABS braking",brakeAbs_.enabled?"On":"Off",153,designPortrait?54:28);
        drawPageControls(tuneBank_,2);
        button(kScreenWidth-165,designFooterY-(designPortrait?72:34),155,28,"Channel setup",29);
    } else if (page == UiPage::ValueEditor) {
        char heading[48];
        if(editorReturn_==UiPage::ResponseProfile)snprintf(heading,sizeof(heading),"CH%u / %s",selectedChannel_,designFieldNames[editorField_]);
        else snprintf(heading,sizeof(heading),"%s / %s",designRoleNames[tuneRole_],designFieldNames[editorField_]);
        label(heading,12,47,UiText::Menu);
        const bool steeringEndpoints = steeringEndpointEditor();
        if (steeringEndpoints) {
            for (uint8_t side=0; side<2; ++side) {
                char text[32];snprintf(text,sizeof(text),"%s%s  %d%%",side?"Right":"Left",
                    side==steeringEndpointSide_?" *":"",steeringEndpointDrafts_[side]);
                steeringEndpointButtons_[side] = button(12+side*(kScreenWidth/2),83,kScreenWidth/2-24,
                       designPortrait?62:38,text,150+side,side==steeringEndpointSide_);
            }
        } else {
            editorValueLabel_=label("",0,0,UiText::Reading,cCyan);
            lv_obj_align(editorValueLabel_,LV_ALIGN_TOP_MID,0,83);
        }
        const int graphY=steeringEndpoints && designPortrait?160:128;
        const int graphH=designPortrait ? (steeringEndpoints?118:150) : 60;
        auto graph=makePanel(12,graphY,kScreenWidth-24,graphH,cPanel,cLine);
        designCurve_=lv_line_create(graph);
        lv_obj_set_style_line_color(designCurve_,colour(cCyan),0); lv_obj_set_style_line_width(designCurve_,2,0);
        if(editorField_==3 || (editorField_>=5&&editorField_<=6)) { lv_obj_add_flag(designCurve_,LV_OBJ_FLAG_HIDDEN); makeLabel(graph,editorField_==3 ? "Time to reach requested output" : "Offset from channel neutral",10,graphH/2-7,UiText::Caption,cDim); }
        if (steeringEndpoints) label("Steer or tap a side, then use + / -",12,designPortrait?282:190,UiText::Caption,cDim);
        const int y=designPortrait ? 302 : 204, w=(kScreenWidth-44)/4;
        button(10,y,w,38,"-",30); editorStepButton_=button(18+w,y,w,38,editorFine_ ? "Fine" : "Coarse",32);
        button(26+w*2,y,w,38,"+",31); button(34+w*3,y,w,38,"Reset",33);
        boundedLabel(label(editorField_==7?"Response offset is separate from channel trim / subtrim.":steeringEndpoints||liveExpoEditor()?"Live. Apply keeps; saved in CUT. Cancel restores.":"Changes take effect only after Apply",12,designPortrait ? 352 : 249,UiText::Caption,cDim),kScreenWidth-24);
        button(10,designFooterY,kScreenWidth/2-15,36,"Cancel",34);
        button(kScreenWidth/2+5,designFooterY,kScreenWidth/2-15,36,"Apply",35,true);
        refreshValueEditor(true);
    } else if (data) {
        const char *tabs[]={"Sensors","GPS","Channels","Inputs"};const uint8_t actions[]={60,61,62,223};
        if(page!=UiPage::SensorDetail || designPortrait)drawPageTabs(tabs,actions,4,page==UiPage::Gps?1:page==UiPage::ChannelMonitor?2:0,&TransmitterUi::controlDesign);
        if(page==UiPage::Telemetry) {
            label(sensorBank_==2 ? "Current / energy" : sensorBank_ ? "Drive battery" : "Radio / motion",12,88,UiText::Caption,cCyan);
            drawPageControls(sensorBank_,(DisplaySensorCount+5)/6);
            makePageActionButton(kScreenWidth-108,84,96,22,"Run review",UiText::Caption,cPanel,cCyan,cLine,&TransmitterUi::controlV2,2);
            const int cols=designPortrait?2:3, w=(kScreenWidth-20+kPanelGap)/cols, h=(designFooterY-34-108+kPanelGap)/(6/cols);
            for(uint8_t i=0;i<6;++i) {
                const uint8_t sensor=displaySensorSource(sensorBank_*6+i);
                if(sensor>=DashboardSensorCount)break;
                int x=10+i%cols*w,y=108+i/cols*h;
                auto panel=makePanel(x,y,w-kPanelGap,h-kPanelGap,cPanel,cLine);
                makeLabel(panel,designSensorNames[sensor],8,6,UiText::Caption,cDim);
                designValues_[i]=makeLabel(panel,"--",8,22,UiText::Heading);
                makeLabel(panel,designSensorUnits[sensor],8,48,UiText::Caption,cDim);
                designDetails_[i]=makeLabel(panel,"No data",w-78,48,UiText::Caption,cDim);
                addPageActionTarget(x,y,w-kPanelGap,h-kPanelGap,&TransmitterUi::controlDesign,70+i);
            }
            if(sensorBank_==2){
                const int x=10+(4%cols)*w,y=108+(4/cols)*h,wide=(cols-4%cols)*w-kPanelGap;
                auto panel=makePanel(x,y,wide,h-kPanelGap,cPanel,cLine);
                makeLabel(panel,"Standard CRSF battery",8,6,UiText::Caption,cDim);
                makePageActionButton(x+8,y+25,wide-16,32,standardBatteryVehicle_?"Vehicle pack / CU01":"Receiver supply",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,245);
            }
        } else if(page==UiPage::SensorDetail) {
            boundedLabel(label(designSensorNames[sensorSelected_],12,designPortrait?90:49,UiText::Menu,cDim),kScreenWidth-156);
            designValues_[0]=label("--",12,110,UiText::Heading);
            designDetails_[0]=label("No data",kScreenWidth-140,118,UiText::Caption,cDim);
            makePageActionButton(kScreenWidth-140,designPortrait?85:47,70,30,sensorSessionStats_[sensorSelected_]?"Session":"30s",UiText::Caption,cPanel,cCyan,cLine,&TransmitterUi::controlDesign,248);
            makePageActionButton(kScreenWidth-66,designPortrait?85:47,54,30,"Reset",UiText::Caption,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,247);
            designDetails_[2]=label("Max -- / Avg --",12,137,UiText::Caption,cDim);
            boundedLabel(designDetails_[2],kScreenWidth-24);
            lv_label_set_long_mode(designDetails_[2],LV_LABEL_LONG_CLIP);lv_obj_set_height(designDetails_[2],UiText::font(UiText::Caption)->line_height);
            if(!designPortrait){
                // Make room for a taller graph and direct sensor configuration.
                lv_obj_set_pos(designValues_[0],12,77);lv_obj_set_pos(designDetails_[0],140,82);
                lv_obj_set_pos(designDetails_[2],200,82);boundedLabel(designDetails_[2],268);

            }
            const int graphTop=designPortrait?157:111,graphHeight=designFooterY-graphTop-(designPortrait?8:48);
            auto panel=makePanel(12,graphTop,kScreenWidth-24,graphHeight,cPanel,cLine);
            for(unsigned n=0;n<3;++n){
                const int y=n*(graphHeight-16)/2;
                designDetails_[3+n]=makeLabel(panel,"--",2,y,UiText::Caption,cDim);
                lv_obj_set_width(designDetails_[3+n],52);lv_obj_set_height(designDetails_[3+n],15);
                lv_label_set_long_mode(designDetails_[3+n],LV_LABEL_LONG_CLIP);
                lv_obj_set_style_text_align(designDetails_[3+n],LV_TEXT_ALIGN_RIGHT,0);
                auto grid=makePanel(0,0,kScreenWidth-94,1,cLine,cLine,0);lv_obj_set_parent(grid,panel);lv_obj_set_pos(grid,60,y+7);
            }
            designCurve_=lv_line_create(panel);
            lv_obj_set_style_line_color(designCurve_,colour(cCyan),0);lv_obj_set_style_line_width(designCurve_,2,0);
            designDetails_[1]=makeLabel(panel,"Waiting for history",64,graphHeight/2-7,UiText::Caption,cDim);
            lv_obj_set_width(designDetails_[1],kScreenWidth-108);lv_label_set_long_mode(designDetails_[1],LV_LABEL_LONG_CLIP);
            if(!designPortrait && sensorSelected_!=17){
                makePageActionButton(12,238,68,30,v2GraphPaused_?"Live":"Pause",UiText::Caption,cPanel,cCyan,cLine,&TransmitterUi::controlV2,20);
                makePageActionButton(84,238,30,30,"<",UiText::Caption,cPanel,cWhite,cLine,&TransmitterUi::controlV2,21);
                makePageActionButton(118,238,30,30,">",UiText::Caption,cPanel,cWhite,cLine,&TransmitterUi::controlV2,22);
                designDetails_[6]=label("30 s history",154,245,UiText::Caption,cDim);boundedLabel(designDetails_[6],143);
                const char *setup=sensorSelected_==0?"Dashboard setup":sensorSelected_==2?"ERS / motor poles":sensorSelected_==4?"RF warnings":sensorSelected_>=6&&sensorSelected_<=11?"Battery alarms":sensorSelected_>=14?"ERS sensor setup":"Sensor settings";
                makePageActionButton(300,238,168,30,setup,UiText::Caption,cPanel2,cCyan,cLine,&TransmitterUi::controlV2,30);
                v2GraphCursorLine_=lv_line_create(panel);lv_obj_set_style_line_color(v2GraphCursorLine_,colour(cAmber),0);lv_obj_set_style_line_width(v2GraphCursorLine_,1,0);
            }
            if(sensorSelected_==17){
                lv_obj_add_flag(panel,LV_OBJ_FLAG_HIDDEN);
                boundedLabel(label("Reported by CU01, not voltage-derived.",12,184,UiText::Caption,cDim),kScreenWidth-24);
                button(12,211,kScreenWidth-24,32,"Set CU01 capacity",249);
                lv_obj_set_parent(designDetails_[1],lv_scr_act());lv_obj_set_pos(designDetails_[1],12,designFooterY-31);lv_obj_set_width(designDetails_[1],kScreenWidth-24);
            }
        } else if(page==UiPage::ChannelMonitor) {
            char range[48];snprintf(range,sizeof(range),"CH%u - CH%u   Requested / sent (%%)",designBank_*4+1,designBank_*4+4);
            label(range,12,85,UiText::Caption,cDim);
            for(uint8_t i=0;i<4;++i) {
                int y=106+i*(designPortrait?66:32);char name[12];snprintf(name,sizeof(name),"CH%u",designBank_*4+i+1);
                label(name,12,y,UiText::Secondary);designValues_[i]=label("--",78,y,UiText::Secondary,cCyan);
                designBars_[i]=makeBar(lv_scr_act(),designPortrait?12:245,designPortrait?y+25:y+5,designPortrait?290:222,8,-1000,1000);
                lv_bar_set_mode(designBars_[i],LV_BAR_MODE_SYMMETRICAL);
            }
            drawPageControls(designBank_,4);
        } else {
            drawGpsMapView(false);
        }
    } else if(page==UiPage::MainMenu || page==UiPage::BatteryMenu) {
        if(page==UiPage::BatteryMenu)label("Vehicle batteries",12,48,UiText::Heading);
        auto menuRow=[&](int y,const char *name,const char *value,uint8_t id){
            const int h=designPortrait?(setupBank_?52:59):(setupBank_?32:38);
            auto panel=makePanel(12,y,kScreenWidth-24,h,cBlack,cLine);
            lv_obj_set_style_border_side(panel,LV_BORDER_SIDE_BOTTOM,0);
            auto title=makeLabel(panel,name,0,(h-UiText::font(UiText::Menu)->line_height)/2,UiText::Menu,cWhite);
            boundedLabel(title,kScreenWidth-118);
            auto v=makeLabel(panel,value,0,0,UiText::Menu,cCyan);lv_obj_align(v,LV_ALIGN_RIGHT_MID,-18,0);
            makeLabel(panel,LV_SYMBOL_RIGHT,kScreenWidth-36,(h-12)/2,UiText::Caption,cCyan);
            addPageActionTarget(12,y,kScreenWidth-24,h,&TransmitterUi::controlDesign,id);
        };
        if(page==UiPage::BatteryMenu){
            menuRow(94,"Receiver battery","RX",146);
            menuRow(94+(designPortrait?59:38),"Drive pack","Warnings",246);
        }else{
            const char *tabs[]={"Model","Handset"};const uint8_t actions[]={140,141};
            drawPageTabs(tabs,actions,2,setupBank_?1:0,&TransmitterUi::controlDesign);
            const char *names[2][8]={{"Channels","Control map","Dashboard","Drive modes","Vehicle batteries","RF warnings","ESC settings","ERS sensors"},{"Display","Sounds / alerts","RF module","TX battery","Network / web","System","",""}};
            const uint8_t ids[2][8]={{41,42,49,43,143,254,46,244},{142,47,44,145,205,206,0,0}};
            const char *values[2][8]={{"CH1-16","","",driveModeSettings_.activeMode==DriveMode::Walk?"Walk":"Normal","","Model","ER6G",""},{"","","Shared","","","","",""}};
            const uint8_t section=setupBank_?1:0,total=section?6:8;
            for(int i=0;i<4;++i){int n=setupPage_*4+i;if(n>=total)break;menuRow(85+i*(designPortrait?67:38),names[section][n],values[section][n],ids[section][n]);}
            drawPageControls(setupPage_,2);
        }
    } else if(page==UiPage::Channels) {
        label("Channels",12,48,UiText::Heading);
        for(int i=0;i<4;++i){int ch=designBank_*4+i+1;char name[36];snprintf(name,sizeof(name),"CH%u  %s",ch,ch==channelRoleSettings_.steeringChannel?"Steering":ch==channelRoleSettings_.throttleChannel?"Throttle":"Auxiliary");row(86+i*(designPortrait?67:40),name,"Configure",90+i,designPortrait?56:37);}
        button(10,designFooterY-34,60,28,"<",80);button(kScreenWidth-70,designFooterY-34,60,28,">",81);
    } else if(page==UiPage::ModelSelect) {
        label("Models",12,48,UiText::Heading);
        if(modelAppearanceEditing_){
            label(modelImageName(modelImages_[activeModelSlot_]),12,85,UiText::Menu,cCyan);
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
        boundedLabel(label(modelSelectStatus_[0]?modelSelectStatus_:"Change model in CUT with throttle centred",12,designFooterY-49,UiText::Caption,cDim),kScreenWidth-24);
        button(10,designFooterY-32,60,27,"<",80);button(78,designFooterY-32,kScreenWidth-156,27,"Appearance",160);button(kScreenWidth-70,designFooterY-32,60,27,">",81);
    } else if(page==UiPage::WidgetSelect) {
        label("Drive layout",12,48,UiText::Heading);
        const char *positions[]={"Top left","Top right","Bottom left","Bottom right"};
        for(int i=0;i<4;++i){
            const auto &w=dashboardLayouts_[activeModelSlot_].cells[i];
            char description[40];
            bool covered=w.kind!=DashboardWidgetKind::Empty && !(visibleDashboardWidgets(dashboardLayouts_[activeModelSlot_])&(1<<i));
            snprintf(description,sizeof(description),"%s%s",dashboardKindName(w.kind),covered?" (hidden)":"");
            row(86+i*(designPortrait?65:39),positions[i],description,110+i,designPortrait?56:36);
        }
        if(designPortrait)button(10,designFooterY-48,300,40,"Use suggested layout",147);
        else label("Select a position to add or edit a widget",12,designFooterY-25,UiText::Caption,cDim);
    } else if(page==UiPage::WidgetEditor) {
        if(widgetInputsEditing_){
            label("Visible inputs",12,47,UiText::Heading);
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
            label(pageText,12,designFooterY-22,UiText::Caption,cDim);
            button(kScreenWidth-110,designFooterY-25,100,25,"Done",177,true);
        }else{
        label("Widget settings",12,47,UiText::Heading);
        const bool speed=widgetDraft_.kind==DashboardWidgetKind::Sensor;
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
            if(i==4 && widgetDraft_.kind!=DashboardWidgetKind::Channels && widgetDraft_.kind!=DashboardWidgetKind::Sensor)break;
            row(77+i*(designPortrait?51:30),names[i],values[i],120+i,designPortrait?47:28);
        }
        if(widgetDraft_.kind==DashboardWidgetKind::AllChannels)row(77+4*(designPortrait?51:30),"Visible inputs","Choose",169,designPortrait?47:28);
        if(widgetDraft_.kind==DashboardWidgetKind::DrivePack){
            auto help=label("CU01 voltage, current and used mAh.\nRemaining is a sensor estimate.",12,77+3*(designPortrait?51:30)+4,UiText::Caption,cDim);
            lv_obj_set_width(help,kScreenWidth-24);
        }
        if(widgetDraft_.kind==DashboardWidgetKind::PowerEnergy||widgetDraft_.kind==DashboardWidgetKind::ControlHistory){
            auto help=label(widgetDraft_.kind==DashboardWidgetKind::PowerEnergy?
                "CU01 electrical input / local session.\nGaps are marked; Wh/km needs GPS travel.":
                "Calibrated driver inputs / 10 seconds.\nBefore expo, trim and mixing; not outputs.",12,77+3*(designPortrait?51:30)+4,UiText::Caption,cDim);
            lv_obj_set_width(help,kScreenWidth-24);
        }
        if(widgetDraft_.kind==DashboardWidgetKind::Map){
            auto help=label("Position data shows the map.\nSpeed-only data shows speed and statistics.",12,77+3*(designPortrait?51:30)+4,UiText::Caption,cDim);
            lv_obj_set_width(help,kScreenWidth-24);
        }
        if(dashboardSessionWidget(widgetDraft_.kind)){
            const char *text=widgetDraft_.kind==DashboardWidgetKind::TripComputer?
                "GPS distance, moving time and speeds.\nReset is shared with Run summary.":
                widgetDraft_.kind==DashboardWidgetKind::RunSummary?
                "GPS travel and CU01 use since reset.\nReset is shared with Trip computer.":
                "CU01 load and CV01 cell balance.\nMinimum and peak are observed since reset.";
            auto help=label(text,12,77+3*(designPortrait?51:30)+4,UiText::Caption,cDim);
            lv_obj_set_width(help,kScreenWidth-24);
        }
        if(widgetDraft_.kind==DashboardWidgetKind::SpeedRpm){
            auto help=label("GPS ground speed and reported motor RPM.\nIndependent readings and labelled dial ranges.",12,77+3*(designPortrait?51:30)+4,UiText::Caption,cDim);
            lv_obj_set_width(help,kScreenWidth-24);
        }
        label("Covered widgets are kept for later",12,designFooterY-(designPortrait?65:45),UiText::Caption,cDim);
        button(10,designFooterY-(designPortrait?44:23),90,designPortrait?40:23,"Cancel",125);button(kScreenWidth-110,designFooterY-(designPortrait?44:23),100,designPortrait?40:23,"Apply",126,true);
        }
    }
    finishPageFocus();
    // Resolve the width of initially empty status labels before LONG_DOT sees
    // their first live text; otherwise it can replace the entire reason with dots.
    if(page==UiPage::OutputStatus || page==UiPage::Tune || page==UiPage::Gps || page==UiPage::SensorDetail)lv_obj_update_layout(lv_scr_act());
    updateDesign(designTelemetry_,designSafety_,designTxVoltage_,designTxValid_);
    uiMetrics.buildUs = uint32_t(micros()) - uiMetrics.pageStartedUs;
}

void TransmitterUi::controlDesign(uint8_t item, PageControlEvent event)
{
    playKeyClick();
    if(item>=154&&item<=157){
        const uint8_t cell=item-154;
        if(activePage_!=UiPage::Dashboard||!dashboardMetricsResettable(dashboardWidgetForView(cell).kind))return;
        const uint8_t visible=maximizedWidget_>=0?uint8_t(1<<maximizedWidget_):visibleDashboardWidgets(dashboardLayouts_[activeModelSlot_]);
        if(!(visible&(1<<cell)))return;
        widgetMetricsResetKind_=dashboardWidgetForView(cell).kind;
        widgetMetricsReturnMaximized_=maximizedWidget_;
        drawDesignPage(UiPage::WidgetMetricsReset);return;
    }
    if(item==158||item==159){
        if(activePage_!=UiPage::WidgetMetricsReset)return;
        if(item==159){
            if(widgetMetricsResetKind_==DashboardWidgetKind::BatteryLoadBalance)resetBatteryLoadMetrics();
            else if(widgetMetricsResetKind_==DashboardWidgetKind::TripComputer||widgetMetricsResetKind_==DashboardWidgetKind::RunSummary)resetTripRunMetrics();
            else resetWidgetPowerMetrics();
        }
        maximizedWidget_=widgetMetricsReturnMaximized_;drawDashboard();return;
    }
    if(item==244){
        v2SensorSetupReturn_=false;
        ersSelected_=-1;ersListBank_=0;ersPageMessage_[0]=0;
        drawDesignPage(UiPage::ErsSensors);return;
    }
    if(item==243){
        if(activePage_==UiPage::Dashboard){maximizedWidget_=-1;drawDashboard();}
        return;
    }
    if(item>=239&&item<=242){
        if(remoteTouchActive_){remoteTouchRejected_=true;return;}
        if(item==239){moduleLinkReturnPage_=activePage_;drawDesignPage(UiPage::ModuleLink);return;}
        if(activePage_!=UiPage::ModuleLink)return;
        if(item==240){if(moduleLink_.enabled()||!moduleLink_.enable())return;requestConfigurationCut();}
        else if(item==242){
            if(armRequested_||designSafety_==OutputSafetyStatus::Armed)return;
            requestConfigurationCut();moduleLink_.pause();
        }
        else moduleLink_.dismiss();
        activePage_=moduleLinkReturnPage_;redrawActivePage();return;
    }
    if(item==238){
        if(remoteTouchActive_){remoteTouchRejected_=true;return;}
        requestConfigurationCut();SetupPortal::stop();drawDesignPage(UiPage::OutputStatus);return;
    }
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
                else if(moduleLinkManaged_&&!moduleLink_.enabled()){
                    moduleLinkReturnPage_=UiPage::OutputStatus;drawDesignPage(UiPage::ModuleLink);return;
                }
                else if(!SetupPortal::active()&&(!remoteTouchActive_||remoteArmHoldDispatch_)&&!calibrationActive()&&!escProgrammer_.blocksArm()){armRequested_=true;safetyResetRequested_=true;activePage_=statusReturnPage_;redrawActivePage();}
            }
            return;
        }
        if(item==182||item==183){
            const auto mode=item==182?DriveMode::Walk:DriveMode::Normal;
            // Reselecting the active tab is navigation only: never CUT or arm.
            if(mode==driveModeSettings_.activeMode)return;
            const auto returnPage=activePage_;
            if(!requestDriveMode(mode))return; // Walk exit stays behind the PIN.
            requestConfigurationCut();saveDriveModeSettings();
            if(returnPage==UiPage::Dashboard||returnPage==UiPage::ModeChooser)drawDashboard();
            else drawDesignPage(UiPage::DriveModes);
            return;
        }
        if(item==184){driveModesReturnPage_=UiPage::Dashboard;drawDriveModesPage();return;}
        if(item==185||item==219||item==220){driveModeSettings_.walkLimitPercent=constrain(driveModeSettings_.walkLimitPercent+(item==219||event==PageControlEvent::Decrease?-5:5),10,80);requestConfigurationCut();saveDriveModeSettings();drawDriveModesPage();return;}
        if(item==186){channelInputPage_=0;drawDesignPage(UiPage::ChannelInput);return;}
        if(item==187){drawDesignPage(UiPage::ChannelResponseSetup);return;}
        if(item==188){mixingDraft_=channelModel_.mixing[selectedChannel_-1];mixingSaveError_=false;mixingLiveInputs_=true;drawDesignPage(UiPage::MixingOverview);return;}
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
        if(item==205){setupBank_=1;setupPage_=0;drawDesignPage(UiPage::NetworkOptions);return;}
        if(item==206){drawDesignPage(UiPage::SystemMenu);return;}
        if(item==207||item==208){setupPage_=constrain(setupPage_+(item==207?-1:1),0,1);drawMainMenu();return;}
        if(item==209){
            if(activePage_==UiPage::OutputStatus){activePage_=statusReturnPage_;redrawActivePage();return;}
            if(activePage_==UiPage::ParameterControlPicker||activePage_==UiPage::ResponseProfile){drawDesignPage(UiPage::ChannelResponseSetup);return;}
            if(activePage_==UiPage::ChannelInput&&channelInputPage_){channelInputPage_=0;drawDesignPage(UiPage::ChannelInput);return;}
            if(activePage_==UiPage::ChannelInput||activePage_==UiPage::ChannelResponseSetup||activePage_==UiPage::MixingOverview){drawChannelPlaceholder();return;}
            if(activePage_==UiPage::ChannelPlaceholder){drawChannelsPage();return;}
            if(activePage_==UiPage::ModeChooser||activePage_==UiPage::DriveModes){drawDesignPage(driveModesReturnPage_);return;}
            drawMainMenu();return;
        }
        if(item==210||item==211||item==212){openWifiPage(item==211,item==212?5:0);return;}
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
        openWifiPage();return;
    }
    if(item==0&&designTouchDispatch_&&armGestureValid_&&lastTouchDurationMs_>=kDashboardArmHoldMs+(remoteTouchActive_?0:kDriveHoldFeedbackDelayMs)){
        // Manual output changes require the full hold in either direction.
        if(armRequested_){requestConfigurationCut();}
        else if(moduleLinkManaged_&&!moduleLink_.enabled()){
            moduleLinkReturnPage_=UiPage::Dashboard;drawDesignPage(UiPage::ModuleLink);return;
        }
        else if(SetupPortal::active()){
            statusReturnPage_=UiPage::Dashboard;drawDesignPage(UiPage::OutputStatus);return;
        }
        else if((!remoteTouchActive_||remoteArmHoldDispatch_)&&!calibrationActive()&&!escProgrammer_.blocksArm()){armRequested_=true;safetyResetRequested_=true;}
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
    if(item==8){timerAction(2);return;}

    if(item>=10 && item<=12){tuneRole_=item-10;drawDesignPage(UiPage::Tune);return;}
    if(item>=20 && item<=26){
        if(activePage_==UiPage::Dashboard){tuneRole_=item==20||item==22?0:2;selectedChannel_=tuneRole_==0?channelRoleSettings_.steeringChannel:channelRoleSettings_.throttleChannel;openDesignEditor(item>=22?5:0,UiPage::Dashboard);}
        else openDesignEditor(item-20,UiPage::Tune);
        return;
    }
    if(item==27 || item==28){
        if(activePage_!=UiPage::Tune)return;
        const uint8_t page=item==27?0:1;
        if(page!=tuneBank_){tuneBank_=page;drawDesignPage(UiPage::Tune);}
        return;
    }
    if(item==153 && activePage_==UiPage::Tune && tuneRole_==2){drawDesignPage(UiPage::BrakeAbs);return;}
    if(item==29){drawChannelPlaceholder();return;}
    if((item==150 || item==151) && steeringEndpointEditor()) {
        if(event==PageControlEvent::Activate) selectSteeringEndpoint(item-150);
        return;
    }
    if(item>=30 && item<=35){
        if(item==34){drawDesignPage(editorReturn_);return;}
        if(item==35){if(!liveExpoEditor()||expoPreviewActive_)applyDesignValue(editorDraft_);drawDesignPage(editorReturn_);return;}
        if(item==32)editorFine_=!editorFine_;
        else if(item==33)editorDraft_=editorField_==0||editorField_==2?100:0;
        else {int step=editorField_==3?(editorFine_?10:50):editorField_==4?1:(editorFine_?1:5);int direction=item==30?-1:1;if(event==PageControlEvent::Decrease)direction=-1;editorDraft_+=direction*step;}
        int low=editorField_==1?-100:editorField_>=5?-20:editorField_==2?50:0;
        int high=editorField_==3?2000:editorField_==4?1:editorField_>=5?20:100;
        editorDraft_=constrain(editorDraft_,low,high);
        if(liveExpoEditor()&&item!=32)expoPreviewActive_=true;
        if(steeringEndpointEditor())steeringEndpointDrafts_[steeringEndpointSide_]=editorDraft_;
        refreshValueEditor(item!=32);
        return;
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
    if(item==246){vehicleBatteryTab_=0;v2SensorSetupReturn_=false;drawDesignPage(UiPage::DriveBatterySetup);return;}
    if(item==254){if(activePage_!=UiPage::RfWarningSetup)rfWarningReturnPage_=activePage_;drawDesignPage(UiPage::RfWarningSetup);return;}
    if(item>=250&&item<=253){
        const uint8_t cell=item-250;const auto &w=dashboardLayouts_[activeModelSlot_].cells[cell];
        if(w.kind==DashboardWidgetKind::Sensor)resetSensorStatistics(w.source);
        else if(w.kind==DashboardWidgetKind::Map&&activePage_==UiPage::Dashboard&&
                widgetViews_[cell].gpsDisplayed==GpsWidgetView::View::Speed)resetSpeedStatistics();
        return;
    }
    if(item==247){resetSensorStatistics(sensorSelected_);v2GraphPaused_=false;drawDesignPage(UiPage::SensorDetail);return;}
    if(item==248){sensorSessionStats_[sensorSelected_]=!sensorSessionStats_[sensorSelected_];saveDriveBatterySettings();drawDesignPage(UiPage::SensorDetail);return;}
    if(item==249){v2SensorSetupReturn_=true;ersSelected_=-1;ersDraftDirty_=false;ersUsePackCapacity_=true;ersListBank_=0;drawDesignPage(UiPage::ErsSensors);return;}
    if(item==245 && activePage_==UiPage::Telemetry){
        if(portalSetStandardBatteryVehicle(!standardBatteryVehicle_))playKeyClick();
        drawDesignPage(UiPage::Telemetry);return;
    }
    if(item>=70 && item<=75){const uint8_t sensor=displaySensorSource(sensorBank_*6+item-70);if(sensor>=DashboardSensorCount)return;sensorSelected_=sensor;drawDesignPage(UiPage::SensorDetail);return;}
    if(item==80 || item==81){
        int direction=item==80?-1:1;
        if(activePage_==UiPage::ModelSelect)modelSelectBank_=constrain(modelSelectBank_+direction,0,1);
        else if(activePage_==UiPage::MainMenu)setupBank_=constrain(setupBank_+direction,0,designPortrait?1:2);
        else if(activePage_==UiPage::Telemetry)sensorBank_=constrain(sensorBank_+direction,0,(DisplaySensorCount-1)/6);
        else designBank_=constrain(designBank_+direction,0,3);
        drawDesignPage(activePage_);return;
    }
    if(item>=82 && item<=85){if(item==82)gpsZoom_=max<int>(-3,gpsZoom_-1);if(item==83)gpsZoom_=min<int>(3,gpsZoom_+1);if(item==84)gpsTrailVisible_=!gpsTrailVisible_;if(item==85){gpsCount_=0;++gpsTrailRevision_;}drawDesignPage(UiPage::Gps);return;}
    if(item==86){drawChannelRolesPage();return;}
    if(item>=90 && item<=93){selectedChannel_=designBank_*4+item-90+1;drawChannelPlaceholder();return;}
    if(item>=100 && item<=104){selectModelMemory(modelSelectBank_*5+item-100);drawModelSelectPage();return;}
    if(item>=110 && item<=113){widgetCell_=item-110;widgetDraft_=dashboardLayouts_[activeModelSlot_].cells[widgetCell_];widgetHiddenDraft_=dashboardHidden_[activeModelSlot_][widgetCell_];widgetInputsEditing_=false;drawDesignPage(UiPage::WidgetEditor);return;}
    if(item>=120 && item<=124){
        int direction=event==PageControlEvent::Decrease?-1:1;
        if(item==120){widgetDraft_.kind=nextDashboardWidgetKind(widgetDraft_.kind,direction);if(widgetDraft_.kind==DashboardWidgetKind::BatteryCells)widgetDraft_.source=0;}
        if(item==121)cycleDashboardWidgetSize(widgetCell_,widgetDraft_,direction);
        if(item==122){int style=static_cast<int>(widgetDraft_.style);const int styles=static_cast<int>(DashboardWidgetStyle::Count);for(int n=0;n<styles;++n){style=(style+direction+styles)%styles;if(dashboardStyleAllowed(widgetDraft_.kind,static_cast<DashboardWidgetStyle>(style)))break;}widgetDraft_.style=static_cast<DashboardWidgetStyle>(style);}
        if(item==123 && widgetDraft_.kind==DashboardWidgetKind::AllChannels)widgetDraft_.source=(widgetDraft_.source+direction+3)%3;
        else if(item==123 && widgetDraft_.kind==DashboardWidgetKind::BatteryCells)widgetDraft_.source=(widgetDraft_.source+direction+13)%13;
        else if(item==123)widgetDraft_.source=widgetDraft_.kind==DashboardWidgetKind::Sensor ? nextDashboardSensorSource(widgetDraft_.source,direction) : (widgetDraft_.source+direction+16)%16;
        if(item==124){
            if(widgetDraft_.kind==DashboardWidgetKind::Sensor)
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
        if(w.kind==DashboardWidgetKind::Timer){timerBank_=0;timerConfirmation_=0;drawDesignPage(UiPage::TimerControl);}
        else if(w.kind==DashboardWidgetKind::AllChannels || w.kind==DashboardWidgetKind::BatteryCells){++channelWidgetPages_[item-130];drawDashboard();}
        else if(w.kind==DashboardWidgetKind::Map){
            if(widgetViews_[item-130].gpsDisplayed==GpsWidgetView::View::Speed){sensorSelected_=0;drawDesignPage(UiPage::SensorDetail);}
            else drawDesignPage(UiPage::Gps);
        }
        else if(w.kind==DashboardWidgetKind::Channels){designBank_=w.source/4;drawDesignPage(UiPage::ChannelMonitor);}
        else if(w.kind==DashboardWidgetKind::Sensor){sensorSelected_=w.source;drawDesignPage(UiPage::SensorDetail);}
        else if(w.kind==DashboardWidgetKind::Power||w.kind==DashboardWidgetKind::RfMargin)drawRadioDiagnostics();
        else if(w.kind==DashboardWidgetKind::Model)drawModelSelectPage();
        return;
    }
}

void TransmitterUi::updateDesign(const CrsfTelemetryState &t, OutputSafetyStatus safety, uint16_t tx, bool txValid)
{
    const uint32_t now=millis();
    // Histories only contain real decoded readings; storage is fixed and bounded.
    if(telemetryHistory_&&static_cast<int32_t>(now-nextSensorSampleMs_)>=0){
        nextSensorSampleMs_=now+1000;
        for(int sensor=0;sensor<DashboardSensorCount;++sensor){
            for(int j=0;j<29;++j){sensorHistory_[sensor][j]=sensorHistory_[sensor][j+1];sensorHistoryValid_[sensor][j]=sensorHistoryValid_[sensor][j+1];}
            uint32_t stamp=0;sensorHistoryValid_[sensor][29]=sensorReading(t,sensor,sensorHistory_[sensor][29],stamp);
        }
        for(int ch=0;ch<16;++ch){for(int i=0;i<29;++i)channelHistory_[ch][i]=channelHistory_[ch][i+1];channelHistory_[ch][29]=designRequested_[ch];}
        sensorHistoryCount_=min<int>(30,sensorHistoryCount_+1);
    }
    if(gpsPositionAvailable(t,now) && t.gpsUpdatedMs!=gpsLastSampleMs_){
        gpsLastSampleMs_=t.gpsUpdatedMs;
        if(!gpsCount_ || gpsLat_[gpsCount_-1]!=t.latitudeE7 || gpsLon_[gpsCount_-1]!=t.longitudeE7){
            if(gpsCount_==48){for(int i=0;i<47;++i){gpsLat_[i]=gpsLat_[i+1];gpsLon_[i]=gpsLon_[i+1];}--gpsCount_;}
            gpsLat_[gpsCount_]=t.latitudeE7;gpsLon_[gpsCount_]=t.longitudeE7;++gpsCount_;++gpsTrailRevision_;
        }
    }
    const bool lowTx=txValid && tx<=txBatterySettings_.lowAlarmDecivolts;
    const bool lowRx=t.receiverVoltageValid && rxBatterySettings_.alarmEnabled && t.receiverVoltageDecivolts<=rxBatterySettings_.lowAlarmDecivolts;
    updateHeaderTxVoltage(tx,txValid,txBatterySettings_.lowAlarmDecivolts);
    updateHeaderGpsTime(t,now);
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
            uiSetText(designDetails_[1],hardwareRecovery_?"Hardware fault; arming inhibited.":SetupPortal::active()?"Finish Wi-Fi setup before enabling output.":moduleLinkManaged_&&!moduleLink_.enabled()?"Module link paused; enable it before arming.":reason);
            if(armProgress_)uiSetText(lv_obj_get_child(lv_obj_get_parent(armProgress_),0),armRequested_?"Hold to cut (0.8 s)":"Enable output");
        }
        if(hardwareRecovery_&&!rfBenchMode_)col=cRed;
        if(designStatus_)uiSetText(designStatus_,"Drive");
        updateDriveIcon(driveIcon_,safety==OutputSafetyStatus::Armed,col==cRed);
    }

    if(designLink_){
        if(moduleLinkManaged_&&!moduleLink_.enabled())uiSetText(designLink_,"Link off");
        else if(SetupPortal::active())uiSetText(designLink_,"Setup");
        else if(safety==OutputSafetyStatus::LinkRequired)uiSetText(designLink_,"Link wait");
        else if(lowTx || lowRx)uiSetText(designLink_,lowTx?"TX LOW":"RX LOW");
        else if(t.linkValid)uiSetTextFmt(designLink_,"%u LQ",t.uplinkLinkQuality);
        else uiSetText(designLink_,t.linkUpdatedMs?"LQ stale":"LQ --");
        uiSetTextColor(designLink_,(SetupPortal::active() || (moduleLinkManaged_&&!moduleLink_.enabled()) || safety==OutputSafetyStatus::LinkRequired)?colour(cAmber):lv_color_hex(lowTx||lowRx?0xff746a:t.linkValid?0xa4d6ae:0xa9bbc5),0);
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
    if(activePage_==UiPage::TimerControl){
        auto &timer=timers_[activeModelSlot_];char text[32];formatTimer(text,sizeof(text),timer.display(),true);
        if(designValues_[0])uiSetText(designValues_[0],text);
        if(timerSummary_){char last[24],best[24],average[24];formatTimer(last,sizeof(last),timer.lastLap(),true);formatTimer(best,sizeof(best),timer.bestLap(),true);formatTimer(average,sizeof(average),timer.averageLap(),true);
            uiSetTextFmt(timerSummary_,"Lap %u%s | Last %s\nBest %s | Avg %s",timer.full()?timer.lapCount():timer.lapCount()+1,timer.full()?" (full)":"",last,best,average);}

    }else if(activePage_==UiPage::Tune){
        refreshTuneParameters();
    }else if(activePage_==UiPage::InputMonitor){refreshInputMonitorRows();
    }else if(activePage_==UiPage::RunReview){updateV2RunReview();
    }else if(activePage_==UiPage::Dashboard){
        updateDashboardWidgets(t,tx,txValid);
    } else if(activePage_==UiPage::Telemetry){for(int i=0;i<6;++i){const uint8_t sensor=displaySensorSource(sensorBank_*6+i);if(sensor<DashboardSensorCount)reading(sensor,designValues_[i],designDetails_[i]);}}
    else if(activePage_==UiPage::SensorDetail){
        reading(sensorSelected_,designValues_[0],designDetails_[0]);
        if(!telemetryHistory_){
            uiSetText(designDetails_[2],"Statistics unavailable: memory");
            uiSetHidden(designCurve_,true);uiSetHidden(designDetails_[1],false);
            uiSetText(designDetails_[1],"History unavailable: memory");
            if(v2GraphCursorLine_)uiSetHidden(v2GraphCursorLine_,true);
            return;
        }
        auto stats=sensorStatistics_[sensorSelected_].result(sensorSessionStats_[sensorSelected_],now);
        if(sensorSelected_==0&&sensorSessionStats_[0])stats={speedStatistics_.available(),speedStatistics_.maximum(),0,speedStatistics_.average()};
        if(stats.valid){char maximum[24],average[24];sensorText(sensorSelected_,stats.maximum,maximum,sizeof(maximum));sensorText(sensorSelected_,stats.average,average,sizeof(average));
            uiSetTextFmt(designDetails_[2],"Max %s / Avg %s %s",maximum,average,designSensorUnits[sensorSelected_]);}
        else uiSetTextFmt(designDetails_[2],"Max -- / Avg -- %s",designSensorUnits[sensorSelected_]);
        const auto *history=v2GraphPaused_?v2FrozenHistory_.data():sensorHistory_[sensorSelected_];
        const auto *valid=v2GraphPaused_?v2FrozenValid_.data():sensorHistoryValid_[sensorSelected_];
        int32_t lo=INT32_MAX,hi=INT32_MIN;int count=0;
        // Draw only the most recent contiguous valid segment, never across gaps.
        int start=30;while(start>0 && valid[start-1])--start;
        for(int i=start;i<30;++i){lo=min(lo,history[i]);hi=max(hi,history[i]);++count;}
        if(count){
            if(lo==hi){const int32_t padding=max<int64_t>(1,llabs(int64_t(lo))/100);lo=max<int64_t>(INT32_MIN,int64_t(lo)-padding);hi=min<int64_t>(INT32_MAX,int64_t(hi)+padding);}
            for(unsigned n=0;n<3;++n){char text[24];sensorText(sensorSelected_,int64_t(hi)-(int64_t(hi)-lo)*n/2,text,sizeof(text));uiSetText(designDetails_[3+n],text);}
        }else for(unsigned n=0;n<3;++n)uiSetText(designDetails_[3+n],"--");
        if(count>=2){
            const auto panel=lv_obj_get_parent(designCurve_);const int height=lv_obj_get_height(panel),width=lv_obj_get_width(panel);
            lv_point_t points[30];
            for(int i=start;i<30;++i)points[i-start]={static_cast<lv_coord_t>(60+(i-start)*(width-70)/max(1,count-1)),static_cast<lv_coord_t>(height-9-(static_cast<int64_t>(history[i])-lo)*(height-16)/max<int64_t>(1,static_cast<int64_t>(hi)-lo))};
            uiSetLinePoints(designCurve_,designPoints_,points,count);uiSetHidden(designCurve_,false);
            uiSetHidden(designDetails_[1],true);
        } else {uiSetHidden(designCurve_,true);uiSetHidden(designDetails_[1],false);uiSetText(designDetails_[1],"Waiting for history");}
        if(sensorSelected_==17)uiSetText(designDetails_[1],"Discover CU01, read, set mAh, then Apply.");
        updateV2GraphCursor();
    } else if(activePage_==UiPage::ChannelMonitor){
        for(int i=0;i<4;++i){int ch=designBank_*4+i;int output=permilleFromCrsfValue(dashboardChannelValues_[ch]);
            uiSetTextFmt(designValues_[i],"%+d / %+d%% %s",permilleFromCrsfValue(designRequested_[ch])/10,output/10,(dashboardEnabledChannelMask_&(1U<<ch))?"":"off");lv_bar_set_value(designBars_[i],output,LV_ANIM_OFF);
        }
    } else if(activePage_==UiPage::Gps){
        // Once a position has been received, keep the map free of changing
        // fix-status text. Stale data is indicated by the retained marker.
        if(!gpsFullScreen_){
            uiSetHidden(designDetails_[0],gpsCount_!=0);
        }
        auto mapObject=lv_obj_get_parent(designCurve_);
        const double zoom=pow(2.0,gpsZoom_);
        const double pixelsPerMetre=gpsBasemapPreviewScale(mapObject,true)*zoom;
        if(!gpsCount_)previewGpsBasemap(mapObject,designMapViewport_,true,zoom);
        if(gpsCount_){
            const int width=lv_obj_get_width(mapObject),height=lv_obj_get_height(mapObject);
            followGpsBasemap(mapObject,designMapViewport_,gpsLat_[gpsCount_-1],gpsLon_[gpsCount_-1],pixelsPerMetre);
            updateGpsTrail(designCurve_,gpsPoints_,gpsRenderedCount_,gpsLat_,gpsLon_,gpsCount_,designMapViewport_,width,height,gpsTrailVisible_,gpsProjection_,gpsTrailRevision_);
            const auto marker=gpsViewportPoint(designMapViewport_,gpsLat_[gpsCount_-1],gpsLon_[gpsCount_-1],width,height);
            lv_obj_set_pos(designMarker_,marker.x-8,marker.y-8);uiSetHidden(designMarker_,false);
            uiSetBgColor(designMarker_,colour(gpsFixConfirmed(t,now)?0x00707d:gpsPositionAvailable(t,now)?0xb87800:0x53636c),0);
            uiSetOpacity(designMarker_,gpsPositionAvailable(t,now)?LV_OPA_COVER:LV_OPA_50);
            uiSetTextFmt(designDetails_[1],"%um",static_cast<unsigned>(40/pixelsPerMetre));
        }
    }
    // Retain the existing voltage warning cadence on every primary page.
    if(lowTx){if(static_cast<int32_t>(now-nextTxLowBatteryAlarmMs_)>=0){playBuzzer(300);nextTxLowBatteryAlarmMs_=now+kLowBatteryRepeatMs;}}else nextTxLowBatteryAlarmMs_=now;
    if(lowRx){if(static_cast<int32_t>(now-nextRxLowBatteryAlarmMs_)>=0){playBuzzer(300);nextRxLowBatteryAlarmMs_=now+kLowBatteryRepeatMs;}}else nextRxLowBatteryAlarmMs_=now;
}

DashboardWidget TransmitterUi::dashboardWidgetForView(uint8_t cell) const
{
    auto widget=dashboardLayouts_[activeModelSlot_].cells[cell];
    if(maximizedWidget_==cell){widget.width=2;widget.height=2;}
    return widget;
}

bool TransmitterUi::handleDashboardWidgetHold(int startX,int startY,int endX,int endY,uint32_t duration)
{
    if(activePage_!=UiPage::Dashboard||duration<650||abs(endX-startX)>=24||abs(endY-startY)>=24)return false;
    for(uint8_t i=0;i<pageFocusCount_;++i){
        const auto &target=pageFocusTargets_[i];
        if(!target.enabled)continue;
        if(target.handler!=&TransmitterUi::controlDesign||target.argument<130||target.argument>133)continue;
        const auto &r=target.activate;
        auto inside=[&](int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;};
        if(inside(startX,startY)&&inside(endX,endY)){
            const int cell=target.argument-130;
            maximizedWidget_=maximizedWidget_==cell?-1:cell;
            drawDashboard();return true;
        }
    }
    return false;
}

void TransmitterUi::drawDashboardWidgets()
{
    const auto &layout=dashboardLayouts_[activeModelSlot_];
    constexpr int gap=kPanelGap;
    const int top=48;
    const int cellW=(kScreenWidth-20-gap)/2, cellH=(designFooterY-top-gap-6)/2;
    const uint8_t visible=maximizedWidget_>=0?uint8_t(1<<maximizedWidget_):visibleDashboardWidgets(layout);
    uint8_t occupied=0;
    for(uint8_t i=0;i<4;++i)if(visible&(1<<i))occupied |= dashboardWidgetMask(i,layout.cells[i]);
    for(uint8_t cell=0;cell<4;++cell){
        if(maximizedWidget_>=0&&cell!=maximizedWidget_)continue;
        const auto w=dashboardWidgetForView(cell);
        int x=maximizedWidget_>=0?10:10+cell%2*(cellW+gap),y=maximizedWidget_>=0?top:top+cell/2*(cellH+gap);
        if(w.kind==DashboardWidgetKind::Empty){
            if(!(occupied&(1<<cell)))makePageActionButton(x,y,cellW,cellH,"+ Add widget",UiText::Secondary,cPanel,cDim,cLine,&TransmitterUi::controlDesign,110+cell);
            continue;
        }
        if(!(visible&(1<<cell)))continue;
        auto &view=widgetViews_[cell];
        const int width=cellW*w.width+gap*(w.width-1),height=maximizedWidget_>=0?designFooterY-top-6:cellH*w.height+gap*(w.height-1);
        view.panel=makePanel(x,y,width,height,cPanel,cLine);
        auto title=makeLabel(view.panel,dashboardKindName(w.kind),8,5,UiText::Caption,cDim);view.title=title;
        if(maximizedWidget_>=0&&w.kind!=DashboardWidgetKind::DrivePack&&w.kind!=DashboardWidgetKind::PowerEnergy&&w.kind!=DashboardWidgetKind::ControlHistory&&w.kind!=DashboardWidgetKind::SpeedRpm&&!dashboardSessionWidget(w.kind))lv_obj_add_flag(title,LV_OBJ_FLAG_HIDDEN);
        const bool channelPreview=w.kind==DashboardWidgetKind::Channels ||
            (w.kind==DashboardWidgetKind::AllChannels&&w.source!=1);
        boundedLabel(title,width-(channelPreview?40:16));
        if(channelPreview){
            view.heldOutputIcon=makeHeldOutputIcon(view.panel);
            if(dashboardOutputsLive_)lv_obj_add_flag(view.heldOutputIcon,LV_OBJ_FLAG_HIDDEN);
        }
        const bool speedWidget=w.kind==DashboardWidgetKind::Sensor && w.source==0;
        const int actionHeight=w.kind==DashboardWidgetKind::Timer?height-(designPortrait?45:31):speedWidget?height-32:dashboardMetricsResettable(w.kind)?height-22:height;
        const bool interactive=w.kind==DashboardWidgetKind::Controls||w.kind==DashboardWidgetKind::Trims;
        if(!interactive){
            if(w.kind==DashboardWidgetKind::Map)view.gpsCardTarget=pageFocusCount_;
            addPageActionTarget(x,y,width,actionHeight,&TransmitterUi::controlDesign,130+cell);
        }
        if(speedWidget)addPageActionTarget(x,y+height-32,width-80,32,&TransmitterUi::controlDesign,130+cell);
        if(dashboardMetricsResettable(w.kind)){
            addPageActionTarget(x,y+height-22,width-64,22,&TransmitterUi::controlDesign,130+cell);
            makePageActionButton(x+width-60,y+height-21,54,19,"Reset",UiText::Caption,cPanel2,cWhite,cLine,&TransmitterUi::controlDesign,154+cell);
        }
        if(w.kind==DashboardWidgetKind::Timer){
            view.values[0]=makeLabel(view.panel,"00:00",8,24,height>130?UiText::Reading:UiText::Menu);
            view.values[1]=makeLabel(view.panel,"",8,height>130?76:44,UiText::Caption,cDim);
            if(height>130){view.values[2]=makeLabel(view.panel,"",8,100,UiText::Caption,cDim);}

            makePageActionButton(x+8,y+height-(designPortrait?45:31),width/2-12,designPortrait?40:26,timerPrimaryLabel(),UiText::Secondary,cPanel2,cCyan,cLine,&TransmitterUi::controlDesign,7);
            makePageActionButton(x+width/2+4,y+height-(designPortrait?45:31),width/2-12,designPortrait?40:26,"Reset",UiText::Secondary,cPanel2,cWhite,cLine,&TransmitterUi::controlDesign,8);
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
                view.values[j]=makeLabel(view.panel,"",xx,yy,UiText::Caption,cWhite);
                int off=0,on=0;const int entry=channels[first+j];
                const bool trim=entry==(0x80|4)||entry==(0x80|5);
                const bool binary=(entry&0x80)?!trim:dashboardBinaryChannel(entry,off,on);
                boundedLabel(view.values[j],vertical?cw-4:(binary?cw-43:cw-6));
                if(vertical){lv_label_set_long_mode(view.values[j],LV_LABEL_LONG_CLIP);lv_obj_set_height(view.values[j],2*UiText::font(UiText::Caption)->line_height);lv_obj_set_style_text_align(view.values[j],LV_TEXT_ALIGN_CENTER,0);}
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
            view.detail=makeLabel(view.panel,caption,8,height-16,UiText::Caption,cDim);boundedLabel(view.detail,width-16);
        } else if(w.kind==DashboardWidgetKind::Power || w.kind==DashboardWidgetKind::Channels){
            bool power=w.kind==DashboardWidgetKind::Power;
            const int count=power || w.secondSource<16?2:1;
            if(w.style==DashboardWidgetStyle::Graph){
                view.plot=lv_line_create(view.panel);lv_obj_set_style_line_color(view.plot,colour(cCyan),0);lv_obj_set_style_line_width(view.plot,2,0);
                if(count==2){view.secondPlot=lv_line_create(view.panel);lv_obj_set_style_line_color(view.secondPlot,colour(cAmber),0);lv_obj_set_style_line_width(view.secondPlot,2,0);}
                view.detail=makeLabel(view.panel,"Output history / 30 s",8,height-16,UiText::Caption,cDim);
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
                    auto name=makeLabel(view.panel,text,0,0,UiText::Caption,cDim);lv_obj_set_pos(name,ax+diameter/2-12,ay+diameter-14);
                    view.values[j]=makeLabel(view.panel,"--",ax+5,ay+diameter/2-9,UiText::Secondary,cWhite);
                    lv_obj_set_width(view.values[j],diameter-10);lv_obj_set_style_text_align(view.values[j],LV_TEXT_ALIGN_CENTER,0);
                } else {
                    int yy=25+j*(height-27)/count;
                    if(w.style==DashboardWidgetStyle::Graph){
                        const bool tall=w.width==1&&w.height==2;
                        makeLabel(view.panel,text,tall?8:8+j*width/count,tall?22+j*15:22,UiText::Caption,j?cAmber:cCyan);
                        view.values[j]=makeLabel(view.panel,"--",tall?50:50+j*width/count,tall?22+j*15:22,UiText::Caption);
                        continue;
                    }
                    makeLabel(view.panel,text,8,yy,UiText::Caption,cDim);
                    view.values[j]=makeLabel(view.panel,"--",power&&height>130?8:width/2,power&&height>130?yy+18:yy,power&&height>130?UiText::Heading:UiText::Menu);
                    if(w.style==DashboardWidgetStyle::Bars){view.bars[j]=makeBar(view.panel,8,yy+(power&&height>130?48:19),width-16,5,power?0:-1000,power?160:1000);if(!power)lv_bar_set_mode(view.bars[j],LV_BAR_MODE_SYMMETRICAL);}
                }
            }
        } else if(w.kind==DashboardWidgetKind::SpeedRpm){
            drawSpeedRpmWidget(cell,width,height);
        } else if(dashboardSessionWidget(w.kind)){
            drawTripRunBatteryWidget(cell,width,height);
        } else if(w.kind==DashboardWidgetKind::PowerEnergy){
            uiSetText(title,"Power / energy / CU01");
            const int columns=width>=220?2:1,rows=4/columns;
            const int cw=(width-16)/columns,rh=(height-46)/rows;
            const char *captions[]={"Input power","Peak seen","Energy used","Est. Wh/km"};
            const int ch=UiText::font(UiText::Caption)->line_height;
            for(uint8_t n=0;n<4;++n){
                const int xx=8+(n%columns)*cw,yy=23+(n/columns)*rh;
                boundedLabel(makeLabel(view.panel,captions[n],xx,yy,UiText::Caption,cDim),cw-6);
                view.values[n]=makeLabel(view.panel,"--",xx,yy+ch,UiText::Secondary,cWhite);
                lv_obj_set_size(view.values[n],cw-6,max(ch,rh-ch));
                lv_label_set_long_mode(view.values[n],LV_LABEL_LONG_CLIP);
            }
            view.detail=makeLabel(view.panel,"CU01 / since reset",8,height-17,UiText::Caption,cDim);
            boundedLabel(view.detail,width-76);
        } else if(w.kind==DashboardWidgetKind::ControlHistory){
            uiSetText(title,"Control INPUTS");
            const int legendWidth=(width-16)/2;
            view.values[0]=makeLabel(view.panel,"ST --",8,21,UiText::Caption,cCyan);
            view.values[1]=makeLabel(view.panel,"TH --",8+legendWidth,21,UiText::Caption,cAmber);
            boundedLabel(view.values[0],legendWidth-4);boundedLabel(view.values[1],legendWidth-4);
            const int plotTop=39,plotHeight=max(6,height-plotTop-29);
            makeLabel(view.panel,"+100",5,plotTop-3,UiText::Caption,cDim);
            makeLabel(view.panel,"0",5,plotTop+plotHeight/2-6,UiText::Caption,cDim);
            makeLabel(view.panel,"-100",5,plotTop+plotHeight-9,UiText::Caption,cDim);
            auto oldest=makeLabel(view.panel,"-10s",38,plotTop+plotHeight+1,UiText::Caption,cDim);
            boundedLabel(oldest,38);
            makeLabel(view.panel,"Now",width-30,plotTop+plotHeight+1,UiText::Caption,cDim);
            if(widgetMetrics_){
                view.plot=lv_chart_create(view.panel);position(view.plot,38,plotTop,width-46,plotHeight);
                lv_obj_set_style_pad_all(view.plot,0,0);lv_obj_set_style_border_width(view.plot,0,0);
                lv_obj_set_style_bg_opa(view.plot,LV_OPA_TRANSP,0);
                lv_obj_set_style_line_color(view.plot,colour(cLine),LV_PART_MAIN);
                lv_obj_set_style_line_width(view.plot,1,LV_PART_MAIN);
                lv_obj_set_style_line_width(view.plot,2,LV_PART_ITEMS);
                lv_obj_set_style_size(view.plot,0,LV_PART_INDICATOR);
                lv_obj_clear_flag(view.plot,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
                lv_chart_set_type(view.plot,LV_CHART_TYPE_LINE);
                lv_chart_set_point_count(view.plot,ControlHistory::Capacity);
                lv_chart_set_range(view.plot,LV_CHART_AXIS_PRIMARY_Y,-1000,1000);
                lv_chart_set_div_line_count(view.plot,3,3);
                for(uint8_t n=0;n<2;++n){
                    auto &series=widgetMetrics_.get()->controlSeries[cell][n];
                    auto values=widgetMetrics_.get()->controlValues[cell][n];
                    for(unsigned p=0;p<ControlHistory::Capacity;++p)values[p]=LV_CHART_POINT_NONE;
                    series=lv_chart_add_series(view.plot,colour(n?cAmber:cCyan),LV_CHART_AXIS_PRIMARY_Y);
                    lv_chart_set_ext_y_array(view.plot,series,values);
                }
            }
            view.detail=makeLabel(view.panel,widgetMetrics_?"INPUTS / CUT":"Metrics unavailable",8,height-15,UiText::Caption,cDim);
            boundedLabel(view.detail,width-16);
        } else if(w.kind==DashboardWidgetKind::DrivePack){
            uiSetText(title,"Drive pack / CU01");
            const DrivePackWidgetGrid grid(width,height);
            const char *captions[]={"Pack voltage","Current","Used capacity","Est. remaining"};
            const int captionHeight=UiText::font(UiText::Caption)->line_height;
            for(uint8_t n=0;n<4;++n){
                const int xx=8+(n%grid.columns)*grid.columnWidth,yy=24+(n/grid.columns)*grid.rowHeight;
                auto caption=makeLabel(view.panel,captions[n],xx,yy,UiText::Caption,cDim);
                boundedLabel(caption,grid.columnWidth-6);
                view.values[n]=makeLabel(view.panel,"--",xx,yy+captionHeight+2,UiText::Menu,n==3?cDim:cWhite);
                lv_obj_set_size(view.values[n],grid.columnWidth-6,grid.rowHeight-captionHeight-2);
                lv_label_set_long_mode(view.values[n],LV_LABEL_LONG_CLIP);
            }
        } else if(w.kind==DashboardWidgetKind::BatteryCells){
            for(uint8_t i=0;i<CrsfTelemetryState::MaxVehicleCells;++i){
                view.values[i]=makeLabel(view.panel,"--",8,24,UiText::Caption,cWhite);
                lv_obj_add_flag(view.values[i],LV_OBJ_FLAG_HIDDEN);
            }
            view.detail=makeLabel(view.panel,"No cell telemetry",8,height-(height<120?17:32),UiText::Caption,cDim);
            lv_obj_set_size(view.detail,width-16,height<120?15:30);
            lv_label_set_long_mode(view.detail,LV_LABEL_LONG_CLIP);
        } else if(w.kind==DashboardWidgetKind::Sensor){
            drawDashboardSensorWidget(cell,w,x,y,width,height);
        } else if(w.kind==DashboardWidgetKind::Map){
            lv_obj_add_flag(title,LV_OBJ_FLAG_HIDDEN);
            view.mapViewport.fillSavedRaster=true;
            view.mapViewport.showAttribution=false;
            view.basemap=createGpsBasemap(view.panel,0,0,width-2,height-2,view.mapViewport);
            view.plot=lv_line_create(view.basemap);lv_obj_set_style_line_color(view.plot,colour(0x00707d),0);lv_obj_set_style_line_width(view.plot,2,0);
            view.marker=gpsPositionDot(view.basemap);
            lv_obj_add_flag(view.marker,LV_OBJ_FLAG_HIDDEN);
            auto north=makeLabel(view.basemap,LV_SYMBOL_UP "\nN",0,0,UiText::Caption,0x17343c);
            lv_obj_set_style_text_align(north,LV_TEXT_ALIGN_CENTER,0);
            lv_obj_set_style_bg_color(north,colour(0xf5f7f3),0);
            lv_obj_set_style_bg_opa(north,LV_OPA_90,0);
            lv_obj_set_style_pad_all(north,3,0);
            lv_obj_align(north,LV_ALIGN_TOP_RIGHT,-4,4);
            view.detail=makeLabel(view.basemap,"",4,4,UiText::Caption,0x17343c);
            lv_obj_set_style_bg_color(view.detail,colour(0xf5f7f3),0);
            lv_obj_set_style_bg_opa(view.detail,LV_OPA_90,0);
            lv_obj_set_width(view.detail,width-44);
            lv_label_set_long_mode(view.detail,LV_LABEL_LONG_WRAP);
            view.speedPanel=makePanel(0,0,width-2,height-2,cPanel,cPanel,0);
            lv_obj_set_parent(view.speedPanel,view.panel);
            lv_obj_set_style_border_width(view.speedPanel,0,0);
            view.gpsFooterTarget=pageFocusCount_;
            addPageActionTarget(x,y+height-34,width-82,32,&TransmitterUi::controlDesign,130+cell);
            DashboardWidget speed=w;speed.kind=DashboardWidgetKind::Sensor;speed.source=0;
            speed.style=DashboardWidgetStyle::Numbers;speed.speedValue=SpeedWidgetValue::Current;
            drawDashboardSensorWidget(cell,speed,x,y,width-2,height-2,true);
            view.values[4]=makeLabel(view.speedPanel,"Speed data only",8,height>132?98:53,UiText::Caption,cDim);
            boundedLabel(view.values[4],width-18);
            lv_obj_add_flag(view.speedPanel,LV_OBJ_FLAG_HIDDEN);
            pageFocusTargets_[view.gpsFooterTarget].enabled=false;
            pageFocusTargets_[view.gpsResetTarget].enabled=false;
        } else if(w.kind==DashboardWidgetKind::Controls){
            const int buttonW=(width-24)/2,buttonH=min(44,height-36),buttonY=y+28+(height-36-buttonH)/2;
            for(uint8_t mode=0;mode<2;++mode){
                view.switches[mode]=makePageActionButton(x+8+mode*(buttonW+8),buttonY,buttonW,buttonH,mode?"Walk":"Normal",UiText::Caption,cPanel2,cWhite,cLine,&TransmitterUi::controlDesign,mode?182:183);
            }
        } else if(w.kind==DashboardWidgetKind::Trims){
            const int cols=width>=220?2:1,rows=4/cols,cw=(width-16)/cols,rh=(height-30)/rows;
            for(uint8_t n=0;n<4;++n){
                auto b=makePageActionButton(x+8+(n%cols)*cw,y+26+(n/cols)*rh,cw-4,rh-4,"",UiText::Caption,cPanel2,cWhite,cLine,&TransmitterUi::controlV2,40+n);
                view.values[n]=lv_obj_get_child(b,0);
            }
        } else if(w.kind==DashboardWidgetKind::RfMargin){
            view.values[0]=makeLabel(view.panel,"--",8,25,UiText::Heading,cWhite);
            makeLabel(view.panel,"dB",width-34,32,UiText::Caption,cDim);
            view.detail=makeLabel(view.panel,"No fresh link telemetry",8,height-24,UiText::Caption,cDim);
            boundedLabel(view.detail,width-16);
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
            else {auto icon=makeLabel(view.panel,LV_SYMBOL_IMAGE,0,0,UiText::Reading,cDim);lv_obj_center(icon);}
            auto name=makeLabel(view.panel,modelNames_[activeModelSlot_],0,0,UiText::Caption,cWhite);lv_obj_align(name,LV_ALIGN_BOTTOM_MID,0,-4);
        }
        // Child controls take priority over the widget hold/navigation target.
        if(interactive)addPageActionTarget(x,y,width,height,&TransmitterUi::controlDesign,130+cell);
    }
}

void TransmitterUi::updateDashboardWidgets(const CrsfTelemetryState &t,uint16_t tx,bool txValid)
{
    for(uint8_t cell=0;cell<4;++cell){
        const auto w=dashboardWidgetForView(cell);auto &view=widgetViews_[cell];if(!view.panel)continue;
        if(view.heldOutputIcon){
            const bool hidden=lv_obj_has_flag(view.heldOutputIcon,LV_OBJ_FLAG_HIDDEN);
            if(dashboardOutputsLive_&&!hidden)lv_obj_add_flag(view.heldOutputIcon,LV_OBJ_FLAG_HIDDEN);
            else if(!dashboardOutputsLive_&&hidden)lv_obj_clear_flag(view.heldOutputIcon,LV_OBJ_FLAG_HIDDEN);
        }
        if(w.kind==DashboardWidgetKind::Timer){
            auto &timer=timers_[activeModelSlot_];char text[32],last[24],best[24];formatTimer(text,sizeof(text),timer.display());
            uiSetText(view.values[0],text);formatTimer(last,sizeof(last),timer.lastLap(),true);formatTimer(best,sizeof(best),timer.bestLap(),true);
            if(view.values[1]){
                if(timer.settings.mode==TimerMode::Lap)uiSetTextFmt(view.values[1],"Lap %u%s",timer.full()?timer.lapCount():timer.lapCount()+1,timer.ready()?" ready":"");
                else uiSetTextFmt(view.values[1],"%s / %s",timerModeName(timer.settings.mode),timer.ready()?"Ready":timer.running()?"Running":"Paused");
            }
            if(view.values[2])uiSetTextFmt(view.values[2],"Last %s\nBest %s",last,best);
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
                    if(view.bars[j]){lv_bar_set_value(view.bars[j],valid?position:0,LV_ANIM_OFF);uiSetOpacity(view.bars[j],valid?LV_OPA_COVER:LV_OPA_30);}
                }else{
                    const int value=permilleFromCrsfValue(designRequested_[entry]);
                    if(view.switches[j] && dashboardBinaryChannel(entry,off,on)){
                        // An intermediate requested value is not a false OFF.
                        valid=abs(value-off)<=25||abs(value-on)<=25;high=abs(value-on)<abs(value-off);
                        uiSetTextFmt(view.values[j],vertical?"CH%u\n%s":"CH%u %s",entry+1,valid?(high?"ON":"OFF"):"MID");
                    }else uiSetTextFmt(view.values[j],vertical?"CH%u\n%+d%%":"CH%u %+d%%",entry+1,value/10);
                    if(view.bars[j])lv_bar_set_value(view.bars[j],value,LV_ANIM_OFF);
                }
                if(view.switches[j]){
                    if(valid&&high)lv_obj_add_state(view.switches[j],LV_STATE_CHECKED);else lv_obj_clear_state(view.switches[j],LV_STATE_CHECKED);
                    uiSetOpacity(view.switches[j],valid?LV_OPA_COVER:LV_OPA_30);
                }
            }
        } else if(w.kind==DashboardWidgetKind::Power || w.kind==DashboardWidgetKind::Channels){
            bool power=w.kind==DashboardWidgetKind::Power;
            const int count=power || w.secondSource<16?2:1;
            for(int j=0;j<count;++j){
                bool valid=power?(j?t.receiverVoltageValid:txValid):true;
                int value=power?(j?t.receiverVoltageDecivolts:tx):permilleFromCrsfValue(designRequested_[j?w.secondSource:w.source]);
                bool low=power && valid && (j?(rxBatterySettings_.alarmEnabled && value<=rxBatterySettings_.lowAlarmDecivolts):value<=txBatterySettings_.lowAlarmDecivolts);
                if(!valid)uiSetText(view.values[j],"--");
                else if(power)uiSetTextFmt(view.values[j],"%d.%d%s",value/10,value%10,w.style==DashboardWidgetStyle::Dials?"":" V");
                else if(w.style==DashboardWidgetStyle::Switches)uiSetText(view.values[j],value>100?"HIGH":value< -100?"LOW":"MID");
                else uiSetTextFmt(view.values[j],"%+d%%",value/10);
                uiSetTextColor(view.values[j],colour(low?cRed:valid?cWhite:cDim),0);
                int maximum=1000;
                if(power){auto chem=j?rxBatterySettings_.chemistry:txBatterySettings_.chemistry;int cells=j?rxBatterySettings_.cellCount:txBatterySettings_.cellCount;maximum=cells*(chem==BatteryChemistry::NiMH?15:chem==BatteryChemistry::LiFe?37:42);}
                if(w.style==DashboardWidgetStyle::Graph && telemetryHistory_ && sensorHistoryCount_>=2){
                    lv_obj_update_layout(view.panel);
                    lv_point_t points[30];int ch=j?w.secondSource:w.source;
                    int width=lv_obj_get_width(view.panel),height=lv_obj_get_height(view.panel),count=min<int>(30,sensorHistoryCount_);
                    const int plotTop=w.width==1&&w.height==2&&w.secondSource<16?55:37;
                    const int plotHeight=max(1,height-plotTop-21);
                    for(int i=0;i<count;++i)points[i]={static_cast<lv_coord_t>(8+i*(width-16)/(count-1)),static_cast<lv_coord_t>(plotTop+plotHeight/2-permilleFromCrsfValue(channelHistory_[ch][30-count+i])*plotHeight/2000)};
                    uiSetLinePoints(j?view.secondPlot:view.plot,j?view.secondPoints:view.points,points,count);
                }
                if(view.arcs[j]){lv_arc_set_range(view.arcs[j],power?0:-1000,maximum);lv_arc_set_value(view.arcs[j],valid?value:0);uiSetArcColor(view.arcs[j],colour(low?cRed:valid?cCyan:cDim),LV_PART_INDICATOR);}
                if(view.bars[j]){lv_bar_set_range(view.bars[j],power?0:-1000,maximum);lv_bar_set_value(view.bars[j],valid?value:0,LV_ANIM_OFF);uiSetBgColor(view.bars[j],colour(low?cRed:cCyan),LV_PART_INDICATOR);}
            }
        } else if(w.kind==DashboardWidgetKind::SpeedRpm){
            updateSpeedRpmWidget(cell,t,millis());
        } else if(dashboardSessionWidget(w.kind)){
            updateTripRunBatteryWidget(cell,millis());
        } else if(w.kind==DashboardWidgetKind::PowerEnergy){
            lv_obj_update_layout(view.panel);
            const auto reading=widgetMetrics_?widgetMetrics_.get()->power.result(millis()):PowerEnergyStatistics::Result{};
            char text[4][32];for(auto &line:text)snprintf(line,sizeof(line),"--");
            if(reading.powerValid)snprintf(text[0],sizeof(text[0]),"%.1f W",reading.watts);
            if(reading.peakValid)snprintf(text[1],sizeof(text[1]),"%.1f W",reading.peakWatts);
            if(reading.energyValid)snprintf(text[2],sizeof(text[2]),"%s%.2f Wh",reading.energyComplete?"":"~",reading.wattHours);
            if(reading.efficiencyValid)snprintf(text[3],sizeof(text[3]),"%s%.1f Wh/km",reading.efficiencyComplete?"":"~",reading.wattHoursPerKm);
            for(uint8_t n=0;n<4;++n){
                auto value=view.values[n];uiSetText(value,text[n]);
                uiSetTextFont(value,drivePackReadingFont(text[n],lv_obj_get_width(value),lv_obj_get_height(value)),0);
            }
            const bool narrow=lv_obj_get_width(view.panel)<200;
            const char *caption=!widgetMetrics_?(narrow?"Unavailable":"Metrics unavailable"):
                !reading.energyValid?"CU01 / waiting":
                !reading.energyComplete?(narrow?"Wh gaps":"CU01 / gaps"):
                !reading.efficiencyValid?(narrow?"GPS/100m":"Wh/km needs GPS/100m"):
                !reading.efficiencyComplete?(narrow?"Wh/km gaps":"Wh/km / gaps"):"Since reset";
            uiSetText(view.detail,caption);
        } else if(w.kind==DashboardWidgetKind::ControlHistory){
            updateControlHistoryWidget(cell,millis());
        } else if(w.kind==DashboardWidgetKind::DrivePack){
            lv_obj_update_layout(view.panel);
            const auto reading=drivePackWidgetReading(t,millis());
            char text[4][32];
            for(auto &line:text)snprintf(line,sizeof(line),"--");
            if(reading.fresh){
                snprintf(text[0],sizeof(text[0]),"%u.%u V",reading.voltageDeciV/10,reading.voltageDeciV%10);
                snprintf(text[1],sizeof(text[1]),"%u.%u A",reading.currentDeciA/10,reading.currentDeciA%10);
                snprintf(text[2],sizeof(text[2]),"%lu mAh",static_cast<unsigned long>(reading.usedMah));
                if(reading.remainingValid)snprintf(text[3],sizeof(text[3]),"%u%%",reading.remainingPercent);
            }
            for(uint8_t n=0;n<4;++n){
                auto value=view.values[n];uiSetText(value,text[n]);
                uiSetTextFont(value,drivePackReadingFont(text[n],lv_obj_get_width(value),lv_obj_get_height(value)),0);
            }
        } else if(w.kind==DashboardWidgetKind::BatteryCells){
            lv_obj_update_layout(view.panel);
            const uint8_t reported=min<uint8_t>(t.vehicleCellCount,CrsfTelemetryState::MaxVehicleCells);
            const uint8_t count=max<uint8_t>(w.source,reported);
            const int width=lv_obj_get_width(view.panel),height=lv_obj_get_height(view.panel);
            // Small widgets page the cell list instead of shrinking below
            // Caption. Keep every detected/expected cell available by tapping.
            const int columns=width>=360?6:width>=200?3:2;
            const int available=lv_obj_get_y(view.detail)-24;
            const int maxRows=max(1,available/UiText::font(UiText::Caption)->line_height);
            const int rows=min(maxRows,max(1,(count+columns-1)/columns));
            const int capacity=rows*columns,pages=max(1,(count+capacity-1)/capacity);
            channelWidgetPages_[cell]%=pages;
            const int first=channelWidgetPages_[cell]*capacity;
            const int rowHeight=available/rows,columnWidth=(width-16)/columns;
            if(pages>1)uiSetTextFmt(view.title,"Cells %u-%u / %u >",first+1,min<int>(count,first+capacity),count);
            else uiSetText(view.title,"Battery cells");
            for(uint8_t i=0;i<CrsfTelemetryState::MaxVehicleCells;++i){
                if(t.vehicleCellsValid && i<reported)uiSetTextFmt(view.values[i],"%u: %u.%03u",i+1,t.vehicleCellsMv[i]/1000,t.vehicleCellsMv[i]%1000);
                else uiSetTextFmt(view.values[i],"%u: --",i+1);
                if(i<first || i>=min<int>(count,first+capacity)){lv_obj_add_flag(view.values[i],LV_OBJ_FLAG_HIDDEN);continue;}
                lv_obj_clear_flag(view.values[i],LV_OBJ_FLAG_HIDDEN);
                const int slot=i-first;
                lv_obj_set_pos(view.values[i],8+(slot%columns)*columnWidth,24+(slot/columns)*rowHeight);
                lv_obj_set_size(view.values[i],columnWidth,rowHeight);
                if(lv_label_get_long_mode(view.values[i])!=LV_LABEL_LONG_CLIP)lv_label_set_long_mode(view.values[i],LV_LABEL_LONG_CLIP);
                uiSetTextFont(view.values[i],UiText::font(UiText::Caption),0);
            }
            if(!t.vehicleCellsValid)uiSetText(view.detail,t.vehicleCellsUpdatedMs?"Cell telemetry stale":"No cell telemetry");
            else if(w.source && w.source!=reported)uiSetTextFmt(view.detail,"Expected %uS; received %uS\nCheck pack / telemetry",w.source,reported);
            else if(height<120)uiSetTextFmt(view.detail,"%uS  %u.%02uV  Low %u.%02uV",reported,t.vehiclePackMv/1000,(t.vehiclePackMv%1000)/10,t.vehicleMinimumCellMv/1000,(t.vehicleMinimumCellMv%1000)/10);
            else uiSetTextFmt(view.detail,"%uS  %u.%03u V\nMin %u.%03u V",reported,t.vehiclePackMv/1000,t.vehiclePackMv%1000,t.vehicleMinimumCellMv/1000,t.vehicleMinimumCellMv%1000);
        } else if(w.kind==DashboardWidgetKind::Sensor){
            updateDashboardSensorWidget(cell,w,t,millis());
        } else if(w.kind==DashboardWidgetKind::Controls){
            for(uint8_t mode=0;mode<2;++mode){
                const bool selected=(driveModeSettings_.activeMode==DriveMode::Walk)==bool(mode);
                uiSetBgColor(view.switches[mode],colour(selected?cCyan:cPanel2),0);
                uiSetTextColor(lv_obj_get_child(view.switches[mode],0),colour(selected?cPanel:cWhite),0);
            }
        } else if(w.kind==DashboardWidgetKind::Trims){
            const int st=channelParameterValue(steeringChannel(),ChannelParameterKind::Trim);
            const int th=channelParameterValue(throttleChannel(),ChannelParameterKind::Trim);
            uiSetTextFmt(view.values[0],"ST trim\n%+.1f%%",st/10.0);
            uiSetTextFmt(view.values[1],"ST rate\n%u%%",profileForChannel(steeringChannel()).positiveRatePercent);
            uiSetTextFmt(view.values[2],"TH trim\n%+.1f%%",th/10.0);
            uiSetTextFmt(view.values[3],"Brake\n%u%%",profileForChannel(throttleChannel()).negativeRatePercent);
        } else if(w.kind==DashboardWidgetKind::RfMargin){
            const bool fresh=t.linkValid&&uint32_t(millis()-t.linkUpdatedMs)<=3000;
            const int sensitivity=elrs24Sensitivity(t.rfMode,rfWarningSettings_.elrsMajor);
            if(fresh&&sensitivity)uiSetTextFmt(view.values[0],"%d",t.uplinkRssiDbm-sensitivity);
            else uiSetText(view.values[0],"--");
            if(fresh){
                const unsigned power=crsfPowerMilliwatts(t.txPowerCode);
                if(power)uiSetTextFmt(view.detail,"LQ %u%% | %u mW",t.uplinkLinkQuality,power);
                else uiSetTextFmt(view.detail,"LQ %u%% | power --",t.uplinkLinkQuality);
            }else uiSetText(view.detail,"No fresh link telemetry");
        } else if(w.kind==DashboardWidgetKind::Map){
            updateGpsAutoWidget(cell,t,millis());
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
