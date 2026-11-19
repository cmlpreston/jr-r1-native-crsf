// Header rendering uses synthetic UTC only; no live location or clock changes.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;hardwareRecovery_=false;
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
const auto timeCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto timeCuts=auditCutRequests;
char timeOriginalName[sizeof(modelNames_[0])];memcpy(timeOriginalName,modelNames_[activeModelSlot_],sizeof(timeOriginalName));
memset(modelNames_[activeModelSlot_],'W',sizeof(modelNames_[0])-1);modelNames_[activeModelSlot_][sizeof(modelNames_[0])-1]=0;
const bool timeOriginalAbs=brakeAbs_.enabled,timeOriginalNight=uiNightMode;
const auto timeOriginalZone=systemSettings_.timeZone;
systemSettings_.timeZone=GpsLocalTime::Settings{};
const uint8_t timeOriginalScheme=uiDarkColourScheme;
auto *timeDisplay=lv_disp_get_default();const auto timePriorMonitor=timeDisplay->driver->monitor_cb;
timeDisplay->driver->monitor_cb=auditRefreshMonitor;
struct TimeObjectChurn {unsigned created=0;} timeChurn;
auto timeObjectEvent=[](lv_event_t *event){
    if(lv_event_get_code(event)==LV_EVENT_CHILD_CREATED)
        ++static_cast<TimeObjectChurn *>(lv_event_get_user_data(event))->created;
};
lv_obj_add_event_cb(lv_scr_act(),timeObjectEvent,LV_EVENT_ALL,&timeChurn);
auto verifyHeader=[&](bool back){
    lv_obj_update_layout(lv_scr_act());
    bool backdrop=false,backVisual=false,backTarget=false;
    auto screen=lv_scr_act();
    for(uint32_t n=0;n<lv_obj_get_child_cnt(screen);++n){
        auto object=lv_obj_get_child(screen,n);
        lv_area_t area;lv_obj_get_coords(object,&area);
        if(area.x1==0&&area.x2==kScreenWidth-1&&area.y1==0&&
           lv_obj_get_style_bg_color(object,0).full==colour(cHeader).full&&
           lv_obj_get_style_bg_opa(object,0)==LV_OPA_COVER){
            assert(area.y2==UiLayout::HeaderHeight-1);
            backdrop=true;
        }
        if(area.x1==8&&area.x2==83&&area.y1<UiLayout::HeaderHeight){
            assert(area.y1==1&&area.y2==UiLayout::HeaderHeight-2);
            backVisual=true;
        }
    }
    for(uint8_t n=0;n<pageFocusCount_;++n){
        const auto &target=pageFocusTargets_[n];
        if(target.handler!=&TransmitterUi::controlNavigation||target.argument!=0)continue;
        const auto &area=target.activate;
        assert(area.x==8&&area.width==76&&area.y==0&&area.height==UiLayout::HeaderHeight);
        backTarget=true;
    }
    assert(backdrop&&backVisual==back&&backTarget==back);
};
auto timeFit=[&]{
    lv_obj_update_layout(lv_scr_act());
    const int headerHeight=UiLayout::HeaderHeight;
    assert(designHeaderOutput_);
    lv_area_t clock,voltage,link,wifi,output;lv_obj_get_coords(headerGpsTimeLabel,&clock);
    lv_obj_get_coords(headerTxVoltageLabel,&voltage);lv_obj_get_coords(designLink_,&link);
    lv_obj_get_coords(wifiStatusIcon,&wifi);
    lv_obj_get_coords(lv_obj_get_parent(designHeaderOutput_),&output);
    assert(clock.x1>=0&&clock.y1>=0&&clock.x2<kScreenWidth&&clock.y2<headerHeight);
    const auto *font=lv_obj_get_style_text_font(headerGpsTimeLabel,0);
    assert(lv_txt_get_width(lv_label_get_text(headerGpsTimeLabel),strlen(lv_label_get_text(headerGpsTimeLabel)),font,0,LV_TEXT_FLAG_NONE)<=lv_obj_get_width(headerGpsTimeLabel));
    assert(voltage.x2<link.x1&&link.x2<clock.x1&&clock.x2<wifi.x1);
    assert(output.y1>=0&&output.y2<headerHeight);
    if(portraitUi)assert(clock.x2<output.x1&&output.x2<wifi.x1);
    else assert(output.x2<voltage.x1);
    assert(clock.y1==voltage.y1&&clock.y1==link.y1);
    const auto textFits=[](lv_obj_t *label){
        const auto *f=lv_obj_get_style_text_font(label,0);const auto *text=lv_label_get_text(label);
        return lv_txt_get_width(text,strlen(text),f,0,LV_TEXT_FLAG_NONE)<=lv_obj_get_width(label);
    };
    assert(textFits(headerTxVoltageLabel)&&textFits(designLink_));
    for(const auto *text:{"100 LQ","Link wait","Link off","LQ stale","TX LOW","RX LOW","Setup"})
        assert(lv_txt_get_width(text,strlen(text),font,0,LV_TEXT_FLAG_NONE)<=lv_obj_get_width(designLink_));
    for(const auto *text:{"16.8V","--V"})
        assert(lv_txt_get_width(text,strlen(text),font,0,LV_TEXT_FLAG_NONE)<=lv_obj_get_width(headerTxVoltageLabel));
    for(uint32_t n=0;n<lv_obj_get_child_cnt(lv_scr_act());++n){
        auto object=lv_obj_get_child(lv_scr_act(),n);
        if(object==headerGpsTimeLabel||!lv_obj_check_type(object,&lv_label_class)||lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))continue;
        lv_area_t area;lv_obj_get_coords(object,&area);
        if(area.y1>=headerHeight)continue;
        assert(area.y2<headerHeight);
        assert(clock.x2<area.x1||clock.x1>area.x2||clock.y2<area.y1||clock.y1>area.y2);
        for(uint32_t m=n+1;m<lv_obj_get_child_cnt(lv_scr_act());++m){
            auto other=lv_obj_get_child(lv_scr_act(),m);
            if(!lv_obj_check_type(other,&lv_label_class)||lv_obj_has_flag(other,LV_OBJ_FLAG_HIDDEN))continue;
            lv_area_t otherArea;lv_obj_get_coords(other,&otherArea);
            if(otherArea.y1>=headerHeight)continue;
            assert(area.x2<otherArea.x1||area.x1>otherArea.x2||area.y2<otherArea.y1||area.y1>otherArea.y2);
        }
    }
    auditTypography(lv_scr_act());
};
auto timeShot=[&](const char *name){
    if(const char *prefix=getenv("JR_R1_GPS_TIME_HEADER_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
        lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
};
for(unsigned orientation=0;orientation<2;++orientation){
    for(unsigned theme=0;theme<=darkColourSchemeCount;++theme){
        uiNightMode=theme!=0;uiDarkColourScheme=theme?theme-1:0;
        for(bool abs:{false,true}){
            brakeAbs_.enabled=abs;designTelemetry_=CrsfTelemetryState{};drawDashboard();
            assert(headerGpsTimeLabel&&lv_obj_has_flag(headerGpsTimeLabel,LV_OBJ_FLAG_HIDDEN));
            auto *clock=headerGpsTimeLabel;const auto focusCount=pageFocusCount_;
            CrsfTelemetryState telemetry;telemetry.gpsTimeValid=true;
            telemetry.gpsUtcYear=2026;telemetry.gpsUtcMonth=11;telemetry.gpsUtcDay=15;
            telemetry.gpsUtcHour=12;telemetry.gpsUtcMinute=59;telemetry.gpsTimeUpdatedMs=millis();
            // UTC appears without position, satellites, or a link-status report.
            designTelemetry_=telemetry;updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(!lv_obj_has_flag(clock,LV_OBJ_FLAG_HIDDEN)&&!strcmp(lv_label_get_text(clock),"23:59"));
            assert(telemetry.gpsUtcHour==12&&telemetry.gpsUtcDay==15);
            assert(pageFocusCount_==focusCount&&clock==headerGpsTimeLabel);timeFit();
            if(theme==0&&abs)timeShot("utc-abs");
            lv_refr_now(nullptr);timeChurn.created=0;auditRedrawnPixels=0;
            lv_mem_monitor_t before;lv_mem_monitor(&before);
            for(unsigned n=0;n<20;++n){updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);lv_refr_now(nullptr);}
            lv_mem_monitor_t after;lv_mem_monitor(&after);
            assert(timeChurn.created==0&&auditRedrawnPixels==0);
            assert(before.used_cnt==after.used_cnt&&before.free_size==after.free_size);
            telemetry.gpsUtcHour=13;telemetry.gpsUtcMinute=0;telemetry.gpsTimeUpdatedMs=millis();
            updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(clock==headerGpsTimeLabel&&!strcmp(lv_label_get_text(clock),"00:00"));
            telemetry.gpsUtcMonth=7;updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(!strcmp(lv_label_get_text(clock),"23:00"));
            telemetry.gpsUtcMonth=11;
            telemetry.gpsTimeUpdatedMs=millis()-3001;updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(clock==headerGpsTimeLabel&&lv_obj_has_flag(clock,LV_OBJ_FLAG_HIDDEN));
            telemetry.gpsTimeUpdatedMs=millis();updateDesign(telemetry,OutputSafetyStatus::Cut,123,true);
            assert(!lv_obj_has_flag(clock,LV_OBJ_FLAG_HIDDEN)&&pageFocusCount_==focusCount);
            designTelemetry_=telemetry;drawDesignPage(UiPage::BrakeAbs);timeFit();
            if(theme==0&&abs)timeShot("detail-utc-abs");
            // Legacy detail headers clear the clock pointer before deleting it.
            drawWifiPage();assert(!headerGpsTimeLabel);updateHeaderGpsTime(telemetry,millis());
            verifyHeader(true);
            drawDashboard();assert(headerGpsTimeLabel&&!lv_obj_has_flag(headerGpsTimeLabel,LV_OBJ_FLAG_HIDDEN));
            if(theme==0&&!abs){
                moduleLinkManaged_=true;drawDashboard();
                const auto oldDuration=lastTouchDurationMs_;
                const auto oldStartX=touchStartX_,oldStartY=touchStartY_;
                lv_obj_update_layout(lv_scr_act());
                touchStartX_=lv_obj_get_x(headerGpsTimeLabel)+lv_obj_get_width(headerGpsTimeLabel)/2;
                touchStartY_=lv_obj_get_y(headerGpsTimeLabel)+4;
                lastTouchDurationMs_=900;
                handleTouch(touchStartX_,touchStartY_);assert(activePage_==UiPage::Dashboard);
                touchStartX_=designHeaderLinkX()+designHeaderLinkWidth()/2;
                handleTouch(touchStartX_,touchStartY_);assert(activePage_==UiPage::ModuleLink);
                drawDashboard();lastTouchDurationMs_=100;
                handleTouch(touchStartX_,touchStartY_);assert(activePage_==UiPage::RadioDiagnostics);
                moduleLinkManaged_=false;lastTouchDurationMs_=oldDuration;
                touchStartX_=oldStartX;touchStartY_=oldStartY;drawDashboard();
            }
            assert(!armRequested_&&!safetyResetRequested_&&auditCutRequests==timeCuts);
        }
    }
    const auto originalBank=setupBank_,originalPage=setupPage_;
    const auto originalDuration=lastTouchDurationMs_;
    const auto originalGesture=armGestureValid_;
    drawDashboard();
    const int tabY=kScreenHeight-(portraitUi?60:40)+20;
    constexpr UiPage sections[]={UiPage::Dashboard,UiPage::Tune,UiPage::Telemetry,UiPage::MainMenu};
    for(uint8_t tab=0;tab<4;++tab){
        designTelemetry_.gpsTimeUpdatedMs=millis();
        lastTouchDurationMs_=100;
        handleTouch((2*tab+1)*kScreenWidth/8,tabY);
        assert(activePage_==sections[tab]);
        verifyHeader(false);
        timeFit();
    }
    setupBank_=setupPage_=1;
    drawDisplayPage();lv_obj_update_layout(lv_scr_act());
    verifyHeader(true);
    timeFit();
    auto *output=designHeaderOutput_;
    assert(output&&!strcmp(lv_label_get_text(output),"CUT"));
    updateDesign(designTelemetry_,OutputSafetyStatus::Armed,123,true);
    assert(output==designHeaderOutput_&&!strcmp(lv_label_get_text(output),"LIVE"));
    designSafety_=OutputSafetyStatus::InputFault;
    updateDesign(designTelemetry_,designSafety_,123,true);
    assert(output==designHeaderOutput_&&!strcmp(lv_label_get_text(output),"FAULT"));
    lv_area_t pill;lv_obj_get_coords(lv_obj_get_parent(output),&pill);
    armGestureValid_=true;
    for(uint32_t duration:{100U,1800U}){
        lastTouchDurationMs_=duration;
        handleTouch((pill.x1+pill.x2)/2,(pill.y1+pill.y2)/2);
        assert(activePage_==UiPage::OutputStatus&&!armRequested_&&!safetyResetRequested_);
        assert(!strcmp(lv_label_get_text(designDetails_[0]),"INPUT FAULT"));
        navigateBack();
        assert(activePage_==UiPage::DisplaySetup&&auditCutRequests==timeCuts);
    }
    designSafety_=OutputSafetyStatus::Cut;
    updateDesign(designTelemetry_,designSafety_,123,true);
    navigateBack();
    assert(activePage_==UiPage::MainMenu&&setupBank_==1&&setupPage_==1);
    setupBank_=originalBank;setupPage_=originalPage;
    lastTouchDurationMs_=originalDuration;
    armGestureValid_=originalGesture;
    assert(requestDisplayRotation());applyDisplayRotation();
}
lv_obj_remove_event_cb_with_user_data(lv_scr_act(),timeObjectEvent,&timeChurn);
timeDisplay->driver->monitor_cb=timePriorMonitor;safetyCutHandler_=timeCutHandler;
memcpy(modelNames_[activeModelSlot_],timeOriginalName,sizeof(timeOriginalName));
brakeAbs_.enabled=timeOriginalAbs;uiNightMode=timeOriginalNight;uiDarkColourScheme=timeOriginalScheme;
systemSettings_.timeZone=timeOriginalZone;
Serial.println("Handset header: shared section/detail bounds, GPS time fit, both orientations/palettes, stable objects, status entry and Back PASS");
return true;
