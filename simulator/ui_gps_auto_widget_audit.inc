// Actual CRC-valid CRSF packets, production widget updates and LVGL events.
// Only synthetic Highfield Park coordinates are used, never a saved user map.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;armRequested_=true;safetyResetRequested_=false;
designSafety_=OutputSafetyStatus::Armed;OfflineMap::install(OfflineMap::Package{});
const auto autoGpsCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto autoGpsCuts=auditCutRequests;
const auto autoGpsConfiguration=controlConfigurationSnapshot();
const auto autoGpsLayout=dashboardLayouts_[activeModelSlot_];
const auto autoGpsOther=dashboardLayouts_[(activeModelSlot_+1)%kModelMemoryCount];
const auto autoGpsErs=ersConfig_;const auto autoGpsSequence=ersSequence_;
const bool autoGpsPending=elrsRequestPending_;
auto *autoGpsDisplay=lv_disp_get_default();const auto autoGpsMonitor=autoGpsDisplay->driver->monitor_cb;
autoGpsDisplay->driver->monitor_cb=auditRefreshMonitor;
struct AutoGpsChurn {unsigned created=0;} autoGpsChurn;
auto autoGpsObjects=[](lv_event_t *event){
    if(lv_event_get_code(event)==LV_EVENT_CHILD_CREATED)
        ++static_cast<AutoGpsChurn *>(lv_event_get_user_data(event))->created;
};
lv_obj_add_event_cb(lv_scr_act(),autoGpsObjects,LV_EVENT_ALL,&autoGpsChurn);
auto autoGpsVisible=[](lv_obj_t *object){
    if(!object)return false;
    for(auto current=object;current;current=lv_obj_get_parent(current))
        if(lv_obj_has_flag(current,LV_OBJ_FLAG_HIDDEN))return false;
    return true;
};
auto autoGpsShot=[&](const char *phase,uint8_t span=0){
    if(const char *prefix=getenv("JR_R1_GPS_AUTO_WIDGET_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-%u-%s.bmp",prefix,portraitUi?"portrait":"landscape",span,phase);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
};
auto autoGpsInvariant=[&](){
    assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==autoGpsCuts);
    assert(equivalentControlConfiguration(autoGpsConfiguration,controlConfigurationSnapshot()));
    assert(!memcmp(&autoGpsErs,&ersConfig_,sizeof(autoGpsErs))&&ersSequence_==autoGpsSequence);
    assert(elrsRequestPending_==autoGpsPending);
    assert(!memcmp(&autoGpsOther,&dashboardLayouts_[(activeModelSlot_+1)%kModelMemoryCount],sizeof(autoGpsOther)));
};
auto autoGpsPacket=[](int speed,bool fields=false,int32_t latitude=GpsMap::DemoLat,
                      int32_t longitude=GpsMap::DemoLon,int satellites=-1){
    std::vector<uint8_t> payload(15,0xff);
    if(fields){
        for(unsigned n=0;n<4;++n){payload[n]=uint32_t(latitude)>>(24-n*8);payload[n+4]=uint32_t(longitude)>>(24-n*8);}
        payload[10]=0x23;payload[11]=0x28;payload[12]=0x04;payload[13]=0x10;
    }
    if(speed>=0){payload[8]=unsigned(speed)>>8;payload[9]=unsigned(speed);}
    if(satellites>=0)payload[14]=satellites;
    return payload;
};
auto autoGpsFeed=[](CrsfTelemetryParser &parser,const std::vector<uint8_t> &payload,bool corrupt=false){
    std::vector<uint8_t> frame={0xc8,uint8_t(payload.size()+2),0x02};
    frame.insert(frame.end(),payload.begin(),payload.end());uint8_t crc=0;
    for(size_t n=2;n<frame.size();++n){crc^=frame[n];for(unsigned bit=0;bit<8;++bit)crc=crc&0x80?(crc<<1)^0xd5:crc<<1;}
    frame.push_back(crc^uint8_t(corrupt));for(auto value:frame)parser.feed(value);
    return parser.state();
};
auto autoGpsStamped=[](CrsfTelemetryState state,uint32_t stamp){
    // Replay decoded values at a deterministic clock; never manufacture field
    // validity or overwrite the parser's retained coordinates on FF packets.
    state.gpsDecodedUpdatedMs=state.gpsWireUpdatedMs=stamp;
    if(state.gpsValid)state.gpsUpdatedMs=stamp;
    if(state.gpsSpeedValid)state.gpsSpeedUpdatedMs=stamp;
    if(state.gpsSatellitesValid)state.gpsSatellitesUpdatedMs=stamp;
    return state;
};
auto autoGpsFit=[&](lv_obj_t *panel){
    lv_obj_update_layout(panel);lv_refr_now(nullptr);lv_area_t bounds;lv_obj_get_coords(panel,&bounds);
    auto check=[&](auto &&self,lv_obj_t *object)->void{
        if(!autoGpsVisible(object))return;
        if(lv_obj_check_type(object,&lv_label_class)){
            const char *text=lv_label_get_text(object);if(*text){
                lv_area_t area;lv_obj_get_coords(object,&area);const auto font=lv_obj_get_style_text_font(object,0);
                assert(area.x1>=bounds.x1&&area.y1>=bounds.y1&&area.x2<=bounds.x2&&area.y2<=bounds.y2);
                lv_point_t size;lv_txt_get_size(&size,text,font,0,0,lv_obj_get_width(object),LV_TEXT_FLAG_NONE);
                assert(size.y<=lv_obj_get_height(object));
                assert(!strstr(text,"Green mode")&&!strstr(text,"Blue mode"));
            }
        }
        for(uint32_t n=0;n<lv_obj_get_child_cnt(object);++n)self(self,lv_obj_get_child(object,n));
    };
    check(check,panel);auditTypography(panel);autoGpsInvariant();
};
auto autoGpsTarget=[&](uint8_t argument){
    for(uint8_t n=0;n<pageFocusCount_;++n)
        if(pageFocusTargets_[n].handler==&TransmitterUi::controlDesign&&pageFocusTargets_[n].argument==argument)return int(n);
    return -1;
};
uint32_t autoGpsClock=millis();
auto autoGpsRender=[&](const CrsfTelemetryState &state,uint32_t now,uint8_t cell=0){
    autoGpsClock=now;
    designTelemetry_=state;
    speedStatistics_.update(state.groundSpeedDeciKmh,state.gpsSpeedUpdatedMs,now,gpsSpeedAvailable(state,now));
    if(telemetryHistory_)sensorStatistics_[0].update(state.groundSpeedDeciKmh,state.gpsSpeedUpdatedMs,now,gpsSpeedAvailable(state,now));
    const auto beforeView=widgetViews_[cell].gpsDisplayed;
    const uint32_t beforeEpoch=SetupPortal::screenEpochForTesting();
    updateGpsAutoWidget(cell,state,now);lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    const bool changed=beforeView!=widgetViews_[cell].gpsDisplayed;
    const uint32_t expectedEpoch=changed?(beforeEpoch==UINT32_MAX?1:beforeEpoch+1):beforeEpoch;
    assert(SetupPortal::screenEpochForTesting()==expectedEpoch);
    if(changed){
        // The real remote-hold primitive binds release to a screen token. An
        // otherwise fresh hold from the old map/speed meaning cannot consume a
        // release under the new token; the HTTP route rejects stale taps too.
        RemoteArmHold staleHold;const auto id=staleHold.begin(beforeEpoch,now);
        assert(staleHold.heartbeat(id,beforeEpoch,now+500));
        assert(!staleHold.finish(id,expectedEpoch,now+850));autoGpsInvariant();
    }
};
auto autoGpsMode=[&](uint8_t cell,bool speed){
    const auto &view=widgetViews_[cell];
    if(autoGpsVisible(view.basemap)==speed||autoGpsVisible(view.speedPanel)!=speed)
        Serial.printf("Auto GPS mode mismatch: expectedSpeed=%u displayed=%u policy=%u mapVisible=%u speedVisible=%u touch=%u hold=%lu active=%u now=%lu screen=%dx%d\n",
            unsigned(speed),unsigned(view.gpsDisplayed),unsigned(gpsWidgetPolicy_.result().view),unsigned(autoGpsVisible(view.basemap)),unsigned(autoGpsVisible(view.speedPanel)),
            unsigned(touchWasDown_),static_cast<unsigned long>(remoteArmHold_.id()),unsigned(remoteArmHold_.active(autoGpsClock)),static_cast<unsigned long>(autoGpsClock),kScreenWidth,kScreenHeight);
    assert(autoGpsVisible(view.basemap)!=speed&&autoGpsVisible(view.speedPanel)==speed);
    assert(view.gpsDisplayed==(speed?GpsWidgetView::View::Speed:GpsWidgetView::View::Map));
    const int reset=autoGpsTarget(250+cell);assert(reset>=0&&pageFocusTargets_[reset].enabled==speed);
    assert(autoGpsVisible(view.speedReset)==speed);autoGpsFit(view.panel);
    assert(view.gpsCardTarget<pageFocusCount_);const auto &card=pageFocusTargets_[view.gpsCardTarget];
    lv_area_t bounds;lv_obj_get_coords(view.panel,&bounds);
    assert(card.enabled&&card.activate.x==bounds.x1&&card.activate.y==bounds.y1);
    assert(card.activate.width==lv_obj_get_width(view.panel));
    const int footerHeight=speed?2+UiLayout::WidgetSensorFooterHeight:0;
    assert(card.activate.height==lv_obj_get_height(view.panel)-footerHeight);
    if(speed){
        const auto &resetTarget=pageFocusTargets_[reset].activate;
        assert(resetTarget.height==UiLayout::WidgetSensorResetHeight);
        assert(card.activate.y+card.activate.height<=resetTarget.y);
    }
};
auto autoGpsNoChurn=[&](const CrsfTelemetryState &state,uint32_t now,uint8_t cell=0){
    autoGpsRender(state,now,cell);const auto panel=widgetViews_[cell].panel;
    const auto map=widgetViews_[cell].basemap,speed=widgetViews_[cell].speedPanel;
    const auto epoch=SetupPortal::screenEpochForTesting();const auto focusCount=pageFocusCount_;
    autoGpsChurn.created=0;auditRedrawnPixels=0;lv_mem_monitor_t before;lv_mem_monitor(&before);
    for(unsigned n=0;n<20;++n)autoGpsRender(state,now,cell);
    lv_mem_monitor_t after;lv_mem_monitor(&after);
    assert(auditRedrawnPixels==0&&autoGpsChurn.created==0);
    assert(before.used_cnt==after.used_cnt&&before.free_size==after.free_size);
    assert(widgetViews_[cell].panel==panel&&widgetViews_[cell].basemap==map&&widgetViews_[cell].speedPanel==speed);
    assert(SetupPortal::screenEpochForTesting()==epoch&&pageFocusCount_==focusCount);autoGpsInvariant();
};
auto autoGpsTap=[&](uint8_t argument){
    const int index=autoGpsTarget(argument);assert(index>=0&&pageFocusTargets_[index].enabled);
    const auto target=pageFocusTargets_[index];
    handleTouch(target.activate.x+target.activate.width/2,target.activate.y+target.activate.height/2);
};
CrsfTelemetryParser autoGpsParser;
auto autoGpsFull=autoGpsFeed(autoGpsParser,autoGpsPacket(450,true,GpsMap::DemoLat,GpsMap::DemoLon,8));
assert(autoGpsFull.gpsValid&&autoGpsFull.gpsSpeedValid&&autoGpsFull.gpsSatellitesValid&&autoGpsFull.satellites==8);
const auto autoGpsSpeed=autoGpsFeed(autoGpsParser,autoGpsPacket(450));
assert(!autoGpsSpeed.gpsValid&&autoGpsSpeed.gpsSpeedValid&&!autoGpsSpeed.gpsSatellitesValid);
assert(autoGpsSpeed.latitudeE7==autoGpsFull.latitudeE7&&autoGpsSpeed.longitudeE7==autoGpsFull.longitudeE7);
const auto autoGpsZero=autoGpsFeed(autoGpsParser,autoGpsPacket(0));
assert(autoGpsZero.gpsSpeedValid&&autoGpsZero.groundSpeedDeciKmh==0&&!autoGpsZero.gpsValid);
const auto autoGpsNone=autoGpsFeed(autoGpsParser,autoGpsPacket(-1));
assert(!autoGpsNone.gpsSpeedValid&&!autoGpsNone.gpsValid&&!autoGpsNone.gpsSatellitesValid);
autoGpsFeed(autoGpsParser,autoGpsPacket(650,true,GpsMap::DemoLat,GpsMap::DemoLon,12),true);
assert(autoGpsParser.state().crcErrors==1&&!autoGpsParser.state().gpsSpeedValid);
for(unsigned orientation=0;orientation<2;++orientation){
    for(uint8_t span=0;span<4;++span){
        dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
        DashboardWidget widget;widget.kind=DashboardWidgetKind::Map;widget.width=(span&1)?2:1;widget.height=(span&2)?2:1;
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,widget);
        // drawDashboard primes the view from designTelemetry_. Start each
        // independent acquisition scenario without a previous screen's fix.
        designTelemetry_=CrsfTelemetryState{};gpsWidgetPolicy_.reset();maximizedWidget_=-1;gpsCount_=0;++gpsTrailRevision_;
        gpsLat_[0]=GpsMap::DemoLat-50;gpsLon_[0]=GpsMap::DemoLon-50;
        gpsLat_[1]=GpsMap::DemoLat;gpsLon_[1]=GpsMap::DemoLon;
        drawDashboard();const uint32_t now=millis();
        auto waiting=CrsfTelemetryState{};autoGpsRender(waiting,now);autoGpsMode(0,false);
        assert(lv_obj_has_flag(widgetViews_[0].marker,LV_OBJ_FLAG_HIDDEN));
        autoGpsNoChurn(waiting,now);autoGpsShot("waiting",span);
        // First speed-only observation switches immediately, including a real
        // zero. Remembered coordinates must not turn FF fields into a fix.
        auto zero=autoGpsStamped(autoGpsZero,now);autoGpsRender(zero,now);autoGpsMode(0,true);
        assert(!strcmp(lv_label_get_text(widgetViews_[0].values[0]),"0.0"));
        autoGpsNoChurn(zero,now);autoGpsShot("speed-zero",span);
        auto speed=autoGpsStamped(autoGpsSpeed,now+100);autoGpsRender(speed,now+100);autoGpsMode(0,true);
        assert(!strcmp(lv_label_get_text(widgetViews_[0].values[0]),"45.0"));
        autoGpsNoChurn(speed,now+100);autoGpsShot("speed",span);
        const auto savedLayout=dashboardLayouts_[activeModelSlot_];const auto trailRevision=gpsTrailRevision_;
        const auto trailCount=gpsCount_;const auto peak=speedStatistics_.maximum();
        assert(peak>=450);autoGpsTap(250);assert(activePage_==UiPage::Dashboard&&!speedStatistics_.available());
        assert(gpsTrailRevision_==trailRevision&&gpsCount_==trailCount);
        autoGpsRender(speed,now+100);assert(!speedStatistics_.available()); // reset suppresses cached packet
        assert(!memcmp(&savedLayout,&dashboardLayouts_[activeModelSlot_],sizeof(savedLayout)));autoGpsInvariant();
        // Dropout retains the speed surface, with missing data rather than 0.
        autoGpsRender(speed,now+3101);autoGpsMode(0,true);
        assert(!strcmp(lv_label_get_text(widgetViews_[0].values[0]),"--"));autoGpsNoChurn(speed,now+3101);
        autoGpsShot("speed-unavailable",span);
        // Fresh location takes priority and preserves raster fill, a point
        // marker and trail. Short speed-only gaps keep the map for three sec.
        gpsCount_=2;++gpsTrailRevision_;const auto mapTrailCount=gpsCount_;const auto mapTrailRevision=gpsTrailRevision_;
        auto full=autoGpsStamped(autoGpsFull,now+4000);autoGpsRender(full,now+4000);autoGpsMode(0,false);
        assert(widgetViews_[0].mapViewport.fillSavedRaster&&!widgetViews_[0].mapViewport.showAttribution);
        assert(!lv_obj_has_flag(widgetViews_[0].marker,LV_OBJ_FLAG_HIDDEN));
        autoGpsNoChurn(full,now+4000);autoGpsShot("map-fix",span);
        autoGpsRender(autoGpsNone,now+4000);autoGpsMode(0,false);
        assert(lv_obj_get_style_opa(widgetViews_[0].marker,0)<LV_OPA_COVER);
        autoGpsNoChurn(autoGpsNone,now+4000);autoGpsRender(full,now+4000);
        speed=autoGpsStamped(autoGpsSpeed,now+4500);autoGpsRender(speed,now+4500);autoGpsMode(0,false);
        assert(!lv_obj_has_flag(widgetViews_[0].marker,LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_get_style_opa(widgetViews_[0].marker,0)<LV_OPA_COVER); // last known, never a fresh fix
        autoGpsRender(autoGpsStamped(autoGpsSpeed,now+7000),now+7000);autoGpsMode(0,false);
        autoGpsRender(autoGpsStamped(autoGpsSpeed,now+7001),now+7001);autoGpsMode(0,true);
        autoGpsRender(autoGpsStamped(autoGpsFull,now+7100),now+7100);autoGpsMode(0,false);
        assert(gpsCount_==mapTrailCount&&gpsTrailRevision_==mapTrailRevision);autoGpsInvariant();
        // Known acquisition evidence remains map-shaped, never an invented
        // speed-mode claim or a marker at global no-fix coordinates.
        for(int satellites:{0,1,2,3,4,12}){
            const auto acquisition=autoGpsStamped(autoGpsFeed(autoGpsParser,autoGpsPacket(0,false,0,0,satellites)),now+8000+satellites);
            autoGpsRender(acquisition,now+8000+satellites);autoGpsMode(0,false);
            assert(lv_obj_get_style_opa(widgetViews_[0].marker,0)<LV_OPA_COVER);
        }
        const auto globalNoFix=autoGpsStamped(autoGpsFeed(autoGpsParser,autoGpsPacket(0,true,0,0,0)),now+9000);
        assert(!gpsPositionAvailable(globalNoFix,now+9000));autoGpsRender(globalNoFix,now+9000);autoGpsMode(0,false);
        assert(lv_obj_get_style_opa(widgetViews_[0].marker,0)<LV_OPA_COVER);autoGpsNoChurn(globalNoFix,now+9000);autoGpsShot("acquiring",span);
        const auto knownZero=autoGpsStamped(autoGpsFeed(autoGpsParser,autoGpsPacket(200,true,GpsMap::DemoLat,GpsMap::DemoLon,0)),now+9100);
        // Legacy S.Port encoders can report zero as unknown. Preserve their
        // non-placeholder position, explicitly unconfirmed rather than fixed.
        assert(gpsPositionAvailable(knownZero,now+9100)&&!gpsFixConfirmed(knownZero,now+9100));
        assert(knownZero.gpsSatellitesValid&&knownZero.satellites==0);
        autoGpsRender(knownZero,now+9100);autoGpsMode(0,false);
        assert(lv_obj_get_style_opa(widgetViews_[0].marker,0)==LV_OPA_COVER);
        assert(autoGpsVisible(widgetViews_[0].detail)&&strstr(lv_label_get_text(widgetViews_[0].detail),"unconfirmed"));
        for(int satellites:{1,2,3}){
            const auto lowSatellites=autoGpsStamped(autoGpsFeed(autoGpsParser,autoGpsPacket(200,true,GpsMap::DemoLat,GpsMap::DemoLon,satellites)),now+9150+satellites);
            assert(!gpsPositionAvailable(lowSatellites,now+9150+satellites));
            autoGpsRender(lowSatellites,now+9150+satellites);autoGpsMode(0,false);
            assert(lv_obj_get_style_opa(widgetViews_[0].marker,0)<LV_OPA_COVER);
        }
        const auto unknownSatellites=autoGpsStamped(autoGpsFeed(autoGpsParser,autoGpsPacket(200,true)),now+9200);
        assert(gpsPositionAvailable(unknownSatellites,now+9200)&&!gpsFixConfirmed(unknownSatellites,now+9200));
        autoGpsRender(unknownSatellites,now+9200);autoGpsMode(0,false);
        assert(lv_obj_get_style_opa(widgetViews_[0].marker,0)==LV_OPA_COVER);
        // Objects and page epoch survive ordinary field updates and transitions.
        const auto panel=widgetViews_[0].panel,map=widgetViews_[0].basemap,speedRoot=widgetViews_[0].speedPanel;
        const auto epoch=SetupPortal::screenEpochForTesting();autoGpsChurn.created=0;
        for(unsigned n=0;n<20;++n){
            const uint32_t stamp=now+10000+n*100;
            autoGpsRender(autoGpsStamped(n%2?autoGpsFull:autoGpsSpeed,stamp),stamp);
        }
        assert(autoGpsChurn.created==0&&panel==widgetViews_[0].panel&&map==widgetViews_[0].basemap&&speedRoot==widgetViews_[0].speedPanel);
        assert(SetupPortal::screenEpochForTesting()==epoch);
        // A normal hold maximizes the same auto widget, without persisting a
        // presentation choice; both subviews continue to work there.
        lv_area_t area;lv_obj_get_coords(panel,&area);const int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;
        assert(handleDashboardWidgetHold(x,y,x,y,700)&&maximizedWidget_==0);
        autoGpsRender(autoGpsStamped(autoGpsFull,now+13000),now+13000);autoGpsMode(0,false);autoGpsShot("map-maximized",span);
        autoGpsRender(autoGpsStamped(autoGpsSpeed,now+16001),now+16001);autoGpsMode(0,true);autoGpsShot("speed-maximized",span);
        assert(!memcmp(&savedLayout,&dashboardLayouts_[activeModelSlot_],sizeof(savedLayout)));
        controlDesign(243,activate);assert(maximizedWidget_==-1);autoGpsInvariant();
        if(span==0){
            // Disabled fallback controls must be absent from keypad navigation
            // and direct dispatch. Their old rectangle is an ordinary map tap.
            gpsWidgetPolicy_.reset();autoGpsRender(autoGpsStamped(autoGpsFull,now+20000),now+20000);autoGpsMode(0,false);
            const int resetIndex=autoGpsTarget(250);assert(resetIndex>=0&&!pageFocusTargets_[resetIndex].enabled);
            const auto resetTarget=pageFocusTargets_[resetIndex];const auto maximum=speedStatistics_.maximum();
            assert(!dispatchPageControl(resetIndex,activate)&&speedStatistics_.maximum()==maximum);
            pageFocusVisible_=true;pageFocusIndex_=resetIndex;refreshPageFocus();
            assert(pageFocusTargets_[pageFocusIndex_].enabled);
            for(int direction:{-1,1})for(unsigned step=0;step<2*pageFocusCount_;++step){
                movePageFocus(direction);assert(pageFocusTargets_[pageFocusIndex_].enabled);
            }
            pageFocusVisible_=false;refreshPageFocus();
            const int oldResetX=resetTarget.activate.x+resetTarget.activate.width/2;
            const int oldResetY=resetTarget.activate.y+resetTarget.activate.height/2;
            Serial.printf("Auto GPS old reset tap: xy=%d,%d page=%u view=%u maximum=%u touch=%u duration=%lu\n",
                oldResetX,oldResetY,unsigned(activePage_),unsigned(widgetViews_[0].gpsDisplayed),maximum,
                unsigned(touchWasDown_),static_cast<unsigned long>(lastTouchDurationMs_));
            for(uint8_t n=0;n<pageFocusCount_;++n){const auto &target=pageFocusTargets_[n];const auto &r=target.activate;
                if(target.argument==130||(oldResetX>=r.x&&oldResetX<r.x+r.width&&oldResetY>=r.y&&oldResetY<r.y+r.height))
                    Serial.printf("Auto GPS tap overlap target=%u argument=%u enabled=%u rect=%d,%d,%d,%d\n",n,target.argument,unsigned(target.enabled),r.x,r.y,r.width,r.height);
            }
            Serial.printf("Auto GPS card index=%u panel=%dx%d\n",widgetViews_[0].gpsCardTarget,int(lv_obj_get_width(widgetViews_[0].panel)),int(lv_obj_get_height(widgetViews_[0].panel)));
            handleTouch(oldResetX,oldResetY);
            if(activePage_!=UiPage::Gps||speedStatistics_.maximum()!=maximum)
                Serial.printf("Auto GPS old reset result: page=%u expected=%u view=%u maximum=%u expected=%u\n",unsigned(activePage_),unsigned(UiPage::Gps),unsigned(widgetViews_[0].gpsDisplayed),speedStatistics_.maximum(),maximum);
            assert(activePage_==UiPage::Gps&&speedStatistics_.maximum()==maximum);
            assert(designMapViewport_.showAttribution&&!designMapViewport_.fillSavedRaster);
            drawDashboard();gpsWidgetPolicy_.reset();
            autoGpsRender(autoGpsStamped(autoGpsSpeed,now+21000),now+21000);autoGpsMode(0,true);
            autoGpsTap(130);assert(activePage_==UiPage::SensorDetail&&sensorSelected_==0);
            drawDashboard();gpsWidgetPolicy_.reset();
            autoGpsRender(autoGpsStamped(autoGpsFull,now+22000),now+22000);autoGpsMode(0,false);
            // Do not move a control out from underneath an active finger or
            // remote hold. Commit the data-driven view after the gesture ends.
            touchWasDown_=true;
            autoGpsRender(autoGpsStamped(autoGpsSpeed,now+25001),now+25001);autoGpsMode(0,false);
            touchWasDown_=false;
            autoGpsRender(autoGpsStamped(autoGpsSpeed,now+25001),now+25001);autoGpsMode(0,true);
            remoteArmHold_.begin(7,now+25002);assert(remoteArmHold_.id());
            autoGpsRender(autoGpsStamped(autoGpsFull,now+25100),now+25100);autoGpsMode(0,true);
            remoteArmHold_.cancel();autoGpsRender(autoGpsStamped(autoGpsFull,now+25100),now+25100);autoGpsMode(0,false);
            gpsWidgetPolicy_.reset();autoGpsRender(autoGpsStamped(autoGpsSpeed,now+25200),now+25200);autoGpsMode(0,true);
            const auto expiredHoldId=remoteArmHold_.begin(7,now+25201);
            autoGpsRender(autoGpsStamped(autoGpsFull,now+25300),now+25300);autoGpsMode(0,true);
            autoGpsRender(autoGpsStamped(autoGpsFull,now+26102),now+26102);autoGpsMode(0,false);
            assert(remoteArmHold_.id()==expiredHoldId&&!remoteArmHold_.active(now+26102));remoteArmHold_.cancel();
            // Grace ages the accepted packet, including delayed UI processing.
            gpsWidgetPolicy_.reset();autoGpsRender(autoGpsStamped(autoGpsFull,now+26000),now+28000);
            autoGpsRender(autoGpsStamped(autoGpsSpeed,now+29001),now+29001);autoGpsMode(0,true);
            autoGpsInvariant();
        }
    }
    // A deliberately fixed speed widget still behaves as Sensor0 even when
    // full position telemetry exists; the auto behavior belongs only to Map5.
    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
    DashboardWidget fixed;fixed.kind=DashboardWidgetKind::Sensor;fixed.source=0;fixed.width=fixed.height=2;
    placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,fixed);drawDashboard();
    auto fixedTelemetry=autoGpsStamped(autoGpsFull,millis());designTelemetry_=fixedTelemetry;
    updateDashboardWidgets(fixedTelemetry,99,true);lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    assert(!widgetViews_[0].basemap&&autoGpsVisible(widgetViews_[0].values[0]));
    assert(!strcmp(lv_label_get_text(widgetViews_[0].values[0]),"45.0"));
    autoGpsTap(130);assert(activePage_==UiPage::SensorDetail&&sensorSelected_==0);autoGpsInvariant();
    armRequested_=false;assert(requestDisplayRotation());applyDisplayRotation();armRequested_=true;
}
dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
DashboardWidget autoSaved;autoSaved.kind=DashboardWidgetKind::Map;autoSaved.width=2;
DashboardWidget fixedSaved;fixedSaved.kind=DashboardWidgetKind::Sensor;fixedSaved.source=0;fixedSaved.width=2;
placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,autoSaved);placeDashboardWidget(dashboardLayouts_[activeModelSlot_],2,fixedSaved);
const auto autoSavedLayout=dashboardLayouts_[activeModelSlot_];saveModelMemorySettings();
dashboardLayouts_[activeModelSlot_]=DashboardLayout{};loadModelMemorySettings();
assert(!memcmp(&autoSavedLayout,&dashboardLayouts_[activeModelSlot_],sizeof(autoSavedLayout)));
assert(uint8_t(dashboardLayouts_[activeModelSlot_].cells[0].kind)==5&&dashboardLayouts_[activeModelSlot_].cells[2].source==0);
dashboardLayouts_[activeModelSlot_]=autoGpsLayout;autoGpsInvariant();
lv_obj_remove_event_cb(lv_scr_act(),autoGpsObjects);autoGpsDisplay->driver->monitor_cb=autoGpsMonitor;safetyCutHandler_=autoGpsCutHandler;
Serial.println("GPS auto widget: parsed speed-only/zero/FF/fix/acquisition, grace/dropouts, all spans/maximize, local reset, stable objects/zero-pixel idle and output/config invariants; both orientations PASS");
return true;
