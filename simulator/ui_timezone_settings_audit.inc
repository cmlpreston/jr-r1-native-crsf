// Handset clock configuration uses disposable preferences and synthetic UTC.
const auto zoneOriginalSystem=systemSettings_;
const auto zoneOriginalModel=activeModelSlot_;
const auto zoneOriginalLayout=dashboardLayouts_[activeModelSlot_];
const bool zoneOriginalManaged=moduleLinkManaged_;
const auto zoneOriginalBank=setupBank_,zoneOriginalPage=setupPage_;
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;hardwareRecovery_=false;
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
const auto zoneCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto zoneCuts=auditCutRequests;
const bool zoneLinkEnabled=moduleLink_.enabled();
using Zone=GpsLocalTime::Zone;
auto zoneGuard=[&](bool armed){
    assert(armRequested_==armed&&!safetyResetRequested_&&auditCutRequests==zoneCuts);
    assert(moduleLink_.enabled()==zoneLinkEnabled);
    assert(activeModelSlot_==zoneOriginalModel);
    assert(!memcmp(&zoneOriginalLayout,&dashboardLayouts_[activeModelSlot_],sizeof(zoneOriginalLayout)));
};
auto zoneTouch=[&](PageControlHandler handler,uint8_t argument){
    for(uint8_t i=0;i<pageFocusCount_;++i){const auto target=pageFocusTargets_[i];
        if(target.enabled&&target.handler==handler&&target.argument==argument){
            const auto r=target.activate;
            assert(r.width>0&&r.height>0);
            return dispatchPageControlTouch(r.x+r.width/2,r.y+r.height/2);
        }
    }
    return false;
};
auto zoneRow=[&](uint8_t row){
    for(uint8_t i=0;i<pageFocusCount_;++i)
        if(pageFocusTargets_[i].enabled&&pageFocusTargets_[i].handler==&TransmitterUi::controlClock&&pageFocusTargets_[i].argument==row)
            return i;
    assert(false&&"Clock row must be registered");return uint8_t(0);
};
auto zoneAdjust=[&](uint8_t row,bool increase){
    const auto target=pageFocusTargets_[zoneRow(row)];
    const auto r=increase?target.increase:target.decrease;
    assert(r.width>0&&r.height>0);
    assert(dispatchPageControlTouch(r.x+r.width/2,r.y+r.height/2));
};
auto zoneKey=[&](uint8_t row,bool increase){
    pageFocusIndex_=zoneRow(row);pageFocusVisible_=true;
    keypadAction(increase?PhysicalControlId::KeyPlus:PhysicalControlId::KeyMinus,false);
};
auto zoneFit=[&]{
    lv_obj_update_layout(lv_scr_act());
    auto walk=[&](auto &&self,lv_obj_t *parent)->void{
        for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);++i){
            auto object=lv_obj_get_child(parent,i);
            if(lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))continue;
            if(lv_obj_check_type(object,&lv_label_class)){
                lv_area_t a;lv_obj_get_coords(object,&a);
                assert(a.x1>=0&&a.y1>=0&&a.x2<kScreenWidth&&a.y2<kScreenHeight);
            }
            self(self,object);
        }
    };
    walk(walk,lv_scr_act());auditTypography(lv_scr_act());
};
auto zoneShot=[&](const char *name){
    if(const char *prefix=getenv("JR_R1_TIMEZONE_SETTINGS_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
        lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
};
// Missing or wrong-sized keys use the same per-key defaults. They do not
// reset any unrelated handset setting or model memory.
{
    Preferences p;assert(p.begin("jr-r1-system",false));
    p.putBytes("tz_zone","",0);p.putBytes("tz_q15","",0);p.end();
    systemSettings_.timeZone.zone=Zone::Utc;systemSettings_.timeZone.customOffsetMinutes=-345;
    loadSystemSettings();
    assert(systemSettings_.timeZone.zone==Zone::SydneyMelbourne&&systemSettings_.timeZone.customOffsetMinutes==600);
    assert(systemSettings_.buzzerEnabled==zoneOriginalSystem.buzzerEnabled);
    assert(p.begin("jr-r1-system",false));p.putUChar("tz_zone",255);p.putChar("tz_q15",127);p.end();
    loadSystemSettings();assert(systemSettings_.timeZone.zone==Zone::SydneyMelbourne);
    assert(systemSettings_.timeZone.customOffsetMinutes>=-720&&systemSettings_.timeZone.customOffsetMinutes<=840);
    assert(systemSettings_.timeZone.customOffsetMinutes%15==0);
}
CrsfTelemetryState zoneTelemetry{};
zoneTelemetry.gpsTimeValid=true;zoneTelemetry.gpsUtcYear=2026;
zoneTelemetry.gpsUtcMonth=11;zoneTelemetry.gpsUtcDay=15;
zoneTelemetry.gpsUtcHour=12;zoneTelemetry.gpsUtcMinute=34;zoneTelemetry.gpsUtcSecond=56;
zoneTelemetry.gpsTimeFrames=13;
auto zoneFresh=[&]{
    zoneTelemetry.gpsTimeUpdatedMs=millis();designTelemetry_=zoneTelemetry;
    updateDesign(zoneTelemetry,OutputSafetyStatus::Armed,123,true);
};
auto zoneUnchangedUtc=[&]{
    assert(zoneTelemetry.gpsUtcYear==2026&&zoneTelemetry.gpsUtcMonth==11&&zoneTelemetry.gpsUtcDay==15);
    assert(zoneTelemetry.gpsUtcHour==12&&zoneTelemetry.gpsUtcMinute==34&&zoneTelemetry.gpsUtcSecond==56);
    assert(zoneTelemetry.gpsTimeFrames==13&&!zoneTelemetry.gpsValid&&!zoneTelemetry.linkValid);
};
for(unsigned orientation=0;orientation<2;++orientation){
    // Reach the setting through the real Handset tab and second page.
    setupBank_=0;setupPage_=0;drawMainMenu();
    assert(zoneTouch(&TransmitterUi::controlDesign,141)&&setupBank_==1);
    assert(zoneTouch(&TransmitterUi::controlPageDot,1)&&setupPage_==1);
    assert(zoneTouch(&TransmitterUi::controlDesign,255)&&activePage_==UiPage::ClockSetup);
    armRequested_=true;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Armed;
    systemSettings_.timeZone.zone=Zone::SydneyMelbourne;systemSettings_.timeZone.customOffsetMinutes=600;
    drawDesignPage(UiPage::ClockSetup);zoneFresh();zoneFit();
    assert(!strcmp(lv_label_get_text(headerGpsTimeLabel),"23:34"));
    assert(strstr(lv_label_get_text(designValues_[0]),"Local 23:34")&&strstr(lv_label_get_text(designValues_[0]),"UTC 12:34"));
    zoneShot("sydney");zoneGuard(true);
    // A physical +/- key changes the selected row and is persisted, rather
    // than merely changing an editor draft or this model's preferences.
    zoneKey(0,true);assert(systemSettings_.timeZone.zone==Zone::Brisbane);
    assert(!strcmp(lv_label_get_text(headerGpsTimeLabel),"22:34"));
    const auto presetOffset=systemSettings_.timeZone.customOffsetMinutes;
    controlClock(1,PageControlEvent::Increase);
    assert(systemSettings_.timeZone.customOffsetMinutes==presetOffset);
    for(uint8_t i=0;i<pageFocusCount_;++i)
        assert(pageFocusTargets_[i].handler!=&TransmitterUi::controlClock||pageFocusTargets_[i].argument!=1);
    zoneKey(0,false);assert(systemSettings_.timeZone.zone==Zone::SydneyMelbourne);
    const char *expected[]={"23:34","22:34","23:04","22:04","20:34","12:34","22:34"};
    for(unsigned choice=0;choice<unsigned(Zone::Count);++choice){
        assert(unsigned(systemSettings_.timeZone.zone)==choice);
        zoneFresh();zoneFit();
        assert(!strcmp(lv_label_get_text(headerGpsTimeLabel),expected[choice]));
        const auto selected=systemSettings_.timeZone.zone;
        saveSystemSettings();systemSettings_.timeZone.zone=Zone::Utc;loadSystemSettings();
        assert(systemSettings_.timeZone.zone==selected);
        zoneAdjust(0,true);zoneGuard(true);zoneUnchangedUtc();
    }
    assert(systemSettings_.timeZone.zone==Zone::SydneyMelbourne);
    // Save/reload and another model both retain one shared timezone selection.
    systemSettings_.timeZone.zone=Zone::Custom;systemSettings_.timeZone.customOffsetMinutes=-345;
    saveSystemSettings();
    activeModelSlot_=(zoneOriginalModel+1)%kModelMemoryCount;loadSystemSettings();
    assert(systemSettings_.timeZone.zone==Zone::Custom&&systemSettings_.timeZone.customOffsetMinutes==-345);
    activeModelSlot_=zoneOriginalModel;drawDesignPage(UiPage::ClockSetup);zoneFresh();zoneFit();
    assert(!strcmp(lv_label_get_text(headerGpsTimeLabel),"06:49"));
    assert(strstr(lv_label_get_text(designValues_[0]),"Local 06:49"));zoneShot("custom");
    zoneAdjust(1,true);assert(systemSettings_.timeZone.customOffsetMinutes==-330);
    zoneKey(1,false);assert(systemSettings_.timeZone.customOffsetMinutes==-345);
    systemSettings_.timeZone.customOffsetMinutes=840;drawDesignPage(UiPage::ClockSetup);
    zoneAdjust(1,true);assert(systemSettings_.timeZone.customOffsetMinutes==840);
    zoneAdjust(1,false);assert(systemSettings_.timeZone.customOffsetMinutes==825);
    systemSettings_.timeZone.customOffsetMinutes=-720;drawDesignPage(UiPage::ClockSetup);
    zoneKey(1,false);assert(systemSettings_.timeZone.customOffsetMinutes==-720);
    zoneKey(1,true);assert(systemSettings_.timeZone.customOffsetMinutes==-705);
    loadSystemSettings();assert(systemSettings_.timeZone.customOffsetMinutes==-705);
    zoneGuard(true);zoneUnchangedUtc();
    // The preview and header never display an old value as current time.
    auto stale=zoneTelemetry;stale.gpsTimeUpdatedMs=millis()-3001;
    designTelemetry_=stale;drawDesignPage(UiPage::ClockSetup);updateDesign(stale,OutputSafetyStatus::Armed,123,true);zoneFit();
    assert(lv_obj_has_flag(headerGpsTimeLabel,LV_OBJ_FLAG_HIDDEN));
    assert(!strcmp(lv_label_get_text(designValues_[0]),"GPS time is stale"));zoneShot("stale");
    designTelemetry_=CrsfTelemetryState{};drawDesignPage(UiPage::ClockSetup);zoneFit();
    assert(lv_obj_has_flag(headerGpsTimeLabel,LV_OBJ_FLAG_HIDDEN));
    assert(!strcmp(lv_label_get_text(designValues_[0]),"Waiting for GPS time"));
    assert(zoneTouch(&TransmitterUi::controlNavigation,0)&&activePage_==UiPage::MainMenu);
    assert(setupBank_==1&&setupPage_==1);zoneGuard(true);
    armRequested_=false;designSafety_=OutputSafetyStatus::Cut;
    assert(requestDisplayRotation());applyDisplayRotation();
}
systemSettings_=zoneOriginalSystem;saveSystemSettings();loadSystemSettings();
activeModelSlot_=zoneOriginalModel;moduleLinkManaged_=zoneOriginalManaged;
setupBank_=zoneOriginalBank;setupPage_=zoneOriginalPage;safetyCutHandler_=zoneCutHandler;
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
designTelemetry_=CrsfTelemetryState{};drawDashboard();
Serial.println("Handset timezone: menu/keypad/touch/Back, defaults/corrupt keys, shared persistence/model independence, presets/custom bounds, local/UTC/stale previews and output guards; both orientations PASS");
return true;
