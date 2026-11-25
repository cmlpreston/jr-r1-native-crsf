// Actual LVGL dual-dial rendering with synthetic, location-free telemetry.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;bindingCount_=0;
channelRoleSettings_.steeringChannel=1;channelRoleSettings_.throttleChannel=2;
channelRoleSettings_.steeringEnabled=channelRoleSettings_.throttleEnabled=true;
steeringInput_.setCalibration({1000,2000,3000});triggerInput_.setCalibration({1000,2000,3000});
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
const auto dualCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto dualCuts=auditCutRequests;const auto dualOriginal=dashboardLayouts_[activeModelSlot_];
static_assert(uint8_t(DashboardWidgetKind::SpeedRpm)==19&&sizeof(DashboardWidget)==7,
    "Append a stable dual-dial ID without altering saved bytes");
auto *dualDisplay=lv_disp_get_default();const auto dualPriorMonitor=dualDisplay->driver->monitor_cb;
dualDisplay->driver->monitor_cb=auditRefreshMonitor;
struct DualObjectChurn {unsigned created=0;} dualChurn;
auto dualObjectEvent=[](lv_event_t *event){
    if(lv_event_get_code(event)==LV_EVENT_CHILD_CREATED)
        ++static_cast<DualObjectChurn *>(lv_event_get_user_data(event))->created;
};
lv_obj_add_event_cb(lv_scr_act(),dualObjectEvent,LV_EVENT_ALL,&dualChurn);
auto dualShot=[&](const char *name){
    if(const char *prefix=getenv("JR_R1_SPEED_RPM_WIDGET_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
};
auto dualFit=[&](uint8_t cell){
    auto panel=widgetViews_[cell].panel;assert(panel);
    lv_obj_update_layout(panel);lv_refr_now(nullptr);
    lv_area_t bounds;lv_obj_get_coords(panel,&bounds);std::vector<lv_area_t> labels;
    auto visit=[&](auto &&self,lv_obj_t *parent)->void{
        for(uint32_t n=0;n<lv_obj_get_child_cnt(parent);++n){
            auto child=lv_obj_get_child(parent,n);if(lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))continue;
            if(lv_obj_check_type(child,&lv_label_class)){
                const char *text=lv_label_get_text(child);if(!*text)continue;
                lv_area_t area;lv_obj_get_coords(child,&area);const auto font=lv_obj_get_style_text_font(child,0);
                int measured=0;unsigned lines=1;
                for(const char *first=text;*first;){
                    const char *end=strchr(first,'\n');if(!end)end=first+strlen(first);
                    measured=max(measured,int(lv_txt_get_width(first,end-first,font,0,LV_TEXT_FLAG_NONE)));
                    if(!*end)break;
                    ++lines;first=end+1;
                }
                const bool fits=area.x1>=bounds.x1&&area.y1>=bounds.y1&&area.x2<=bounds.x2&&area.y2<=bounds.y2&&
                    int(lines)*font->line_height<=lv_obj_get_height(child)&&measured<=lv_obj_get_width(child);
                if(!fits){
                    Serial.printf("Speed/RPM clipped '%s' measured=%d label=%dx%d font=%u panel=%dx%d screen=%dx%d\n",
                        text,measured,int(lv_obj_get_width(child)),int(lv_obj_get_height(child)),font->line_height,
                        int(lv_obj_get_width(panel)),int(lv_obj_get_height(panel)),kScreenWidth,kScreenHeight);
                    dualShot("fit-failure");
                }
                assert(fits);
                for(const auto &other:labels)
                    assert(area.x2<other.x1||area.x1>other.x2||area.y2<other.y1||area.y1>other.y2);
                labels.push_back(area);
            }
            self(self,child);
        }
    };visit(visit,panel);
    for(unsigned n=0;n<2;++n){
        auto arc=widgetViews_[cell].arcs[n];assert(arc);
        lv_area_t area;lv_obj_get_coords(arc,&area);
        assert(area.x1>=bounds.x1&&area.y1>=bounds.y1&&area.x2<=bounds.x2&&area.y2<=bounds.y2);
        assert(lv_arc_get_min_value(arc)==0&&lv_arc_get_max_value(arc)==1000);
    }
    assert(!lv_obj_has_flag(widgetViews_[cell].title,LV_OBJ_FLAG_HIDDEN));auditTypography(panel);
    assert(!safetyResetRequested_&&auditCutRequests==dualCuts);
};
auto dualPlace=[&](uint8_t width,uint8_t height,uint8_t cell=0){
    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};DashboardWidget widget;
    widget.kind=DashboardWidgetKind::SpeedRpm;widget.style=DashboardWidgetStyle::Dials;
    widget.width=width;widget.height=height;
    // Unused persisted sensor fields must not change either displayed source.
    widget.source=14;widget.secondSource=15;widget.speedValue=SpeedWidgetValue::Maximum;
    placeDashboardWidget(dashboardLayouts_[activeModelSlot_],cell,widget);drawDashboard();
};
auto dualRender=[&](uint8_t cell,const CrsfTelemetryState &telemetry,uint32_t now){
    designTelemetry_=telemetry;updateSpeedRpmWidget(cell,telemetry,now);
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
};
auto dualUnchanged=[&](uint8_t cell,const CrsfTelemetryState &telemetry,uint32_t now){
    dualRender(cell,telemetry,now);dualChurn.created=0;auditRedrawnPixels=0;
    lv_mem_monitor_t before;lv_mem_monitor(&before);
    for(unsigned n=0;n<20;++n)dualRender(cell,telemetry,now);
    lv_mem_monitor_t after;lv_mem_monitor(&after);
    assert(auditRedrawnPixels==0&&dualChurn.created==0);
    assert(before.used_cnt==after.used_cnt&&before.free_size==after.free_size);
};
auto dualAvailable=[&](uint8_t cell,bool speed,bool rpm){
    auto &view=widgetViews_[cell];const bool expected[]={speed,rpm};
    for(unsigned n=0;n<2;++n){
        assert(view.values[n]&&view.bars[n]&&view.values[n+2]&&view.values[n+4]);
        assert(bool(strstr(lv_label_get_text(view.values[n]),"--"))!=expected[n]);
        assert(lv_obj_get_style_arc_opa(view.arcs[n],LV_PART_INDICATOR)==(expected[n]?LV_OPA_COVER:LV_OPA_TRANSP));
        assert(strstr(lv_label_get_text(view.values[n+2]),"0"));
        assert(strlen(lv_label_get_text(view.bars[n]))>0);
    }
    assert(strstr(lv_label_get_text(view.values[4]),"SPEED"));
    assert(strstr(lv_label_get_text(view.values[5]),"RPM"));
};
CrsfTelemetryState dualTelemetry{};
dualTelemetry.gpsSpeedValid=true;dualTelemetry.groundSpeedDeciKmh=640;
dualTelemetry.rpmValid=true;dualTelemetry.rpm=50000;
dualTelemetry.gpsValid=false;dualTelemetry.gpsSatellitesValid=false;
dualTelemetry.ersBatteryValid=true;dualTelemetry.ersVoltageDeciV=160;dualTelemetry.ersCurrentDeciA=100;
for(unsigned orientation=0;orientation<2;++orientation){
    for(uint8_t cell=0;cell<4;++cell)for(uint8_t width=1;width<=2;++width)for(uint8_t height=1;height<=2;++height){
        if(cell%2+width>2||cell/2+height>2)continue;
        const uint32_t now=millis();dualTelemetry.gpsSpeedUpdatedMs=dualTelemetry.rpmUpdatedMs=now;
        dualTelemetry.gpsSpeedValid=dualTelemetry.rpmValid=true;
        dualTelemetry.groundSpeedDeciKmh=640;dualTelemetry.rpm=50000;
        dualPlace(width,height,cell);dualRender(cell,dualTelemetry,now);dualFit(cell);dualAvailable(cell,true,true);
        assert(strstr(lv_label_get_text(widgetViews_[cell].values[0]),"64"));
        assert(strstr(lv_label_get_text(widgetViews_[cell].values[1]),"50"));
        dualUnchanged(cell,dualTelemetry,now);
        if(cell==0){char name[32];snprintf(name,sizeof(name),"%ux%u",width,height);dualShot(name);}
        const auto saved=dashboardLayouts_[activeModelSlot_];
        lv_area_t area;lv_obj_get_coords(widgetViews_[cell].panel,&area);
        const int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;
        assert(handleDashboardWidgetHold(x,y,x,y,700)&&maximizedWidget_==cell);
        dualRender(cell,dualTelemetry,now);dualFit(cell);dualAvailable(cell,true,true);dualUnchanged(cell,dualTelemetry,now);
        if(cell==0)dualShot("maximized");
        controlDesign(243,activate);assert(maximizedWidget_==-1);
        assert(!memcmp(&saved,&dashboardLayouts_[activeModelSlot_],sizeof(saved)));
    }
    // Fields have independent clocks, including exact stale boundaries and wrap.
    const uint32_t now=millis();dualPlace(1,1);
    dualTelemetry.gpsSpeedValid=dualTelemetry.rpmValid=true;
    dualTelemetry.gpsSpeedUpdatedMs=dualTelemetry.rpmUpdatedMs=now;
    dualRender(0,dualTelemetry,now+3000);dualAvailable(0,true,true);dualFit(0);
    dualRender(0,dualTelemetry,now+3001);dualAvailable(0,false,false);dualFit(0);dualShot("stale");
    dualTelemetry.gpsSpeedUpdatedMs=now+3001;
    dualRender(0,dualTelemetry,now+3001);dualAvailable(0,true,false);dualFit(0);dualUnchanged(0,dualTelemetry,now+3001);
    dualTelemetry.gpsSpeedUpdatedMs=now;dualTelemetry.rpmUpdatedMs=now+3001;
    dualRender(0,dualTelemetry,now+3001);dualAvailable(0,false,true);dualFit(0);
    dualTelemetry.gpsSpeedUpdatedMs=dualTelemetry.rpmUpdatedMs=0xfffffff0u;
    dualRender(0,dualTelemetry,20);dualAvailable(0,true,true);
    dualRender(0,dualTelemetry,0xfffffff0u+3001);dualAvailable(0,false,false);
    // Genuine zero remains available; negative RPM is unavailable, never a
    // reversed arc or a diagnosis of the ESC's forward/reverse state.
    dualTelemetry.groundSpeedDeciKmh=0;dualTelemetry.rpm=0;
    dualTelemetry.gpsSpeedUpdatedMs=dualTelemetry.rpmUpdatedMs=now;
    dualRender(0,dualTelemetry,now);dualAvailable(0,true,true);dualFit(0);dualShot("zero");
    assert(lv_arc_get_value(widgetViews_[0].arcs[0])==0&&lv_arc_get_value(widgetViews_[0].arcs[1])==0);
    dualTelemetry.rpm=-1;dualRender(0,dualTelemetry,now);dualAvailable(0,true,false);
    dualTelemetry.rpm=50000;dualTelemetry.gpsSpeedValid=false;
    dualRender(0,dualTelemetry,now);dualAvailable(0,false,true);
    dualTelemetry.gpsSpeedValid=true;dualTelemetry.rpmValid=false;
    dualRender(0,dualTelemetry,now);dualAvailable(0,true,false);
    dualTelemetry.gpsSpeedValid=dualTelemetry.rpmValid=false;
    dualRender(0,dualTelemetry,now);dualAvailable(0,false,false);dualFit(0);dualUnchanged(0,dualTelemetry,now);
    // Large admitted numeric values must fit without integer range overflow.
    dualTelemetry.gpsSpeedValid=dualTelemetry.rpmValid=true;
    dualTelemetry.groundSpeedDeciKmh=65535;dualTelemetry.rpm=INT32_MAX;
    for(uint8_t width=1;width<=2;++width)for(uint8_t height=1;height<=2;++height){
        dualPlace(width,height);dualRender(0,dualTelemetry,now);dualFit(0);dualAvailable(0,true,true);
        for(unsigned n=0;n<2;++n)assert(lv_arc_get_value(widgetViews_[0].arcs[n])>0&&lv_arc_get_value(widgetViews_[0].arcs[n])<=1000);
    }
    // Expanded ranges latch independently for this rendered card, preventing
    // needle rescaling while decelerating or when either source becomes stale.
    dualTelemetry.rpm=50000;designTelemetry_=dualTelemetry;dualPlace(2,2);dualRender(0,dualTelemetry,now);
    const uint32_t speedCeiling=widgetViews_[0].dialMaximum[0];
    assert(speedCeiling>=65535&&widgetViews_[0].dialMaximum[1]==60000);
    dualTelemetry.groundSpeedDeciKmh=640;dualTelemetry.rpm=INT32_MAX;dualRender(0,dualTelemetry,now);
    const uint32_t rpmCeiling=widgetViews_[0].dialMaximum[1];assert(rpmCeiling>=uint32_t(INT32_MAX));
    dualTelemetry.groundSpeedDeciKmh=0;dualTelemetry.rpm=0;dualRender(0,dualTelemetry,now);
    assert(widgetViews_[0].dialMaximum[0]==speedCeiling&&widgetViews_[0].dialMaximum[1]==rpmCeiling);
    dualRender(0,dualTelemetry,now+3001);dualAvailable(0,false,false);
    assert(widgetViews_[0].dialMaximum[0]==speedCeiling&&widgetViews_[0].dialMaximum[1]==rpmCeiling);
    // Merely rendering or tapping cannot reset statistics, configure a sensor,
    // request CUT, change the module link or otherwise alter live control.
    armRequested_=true;designSafety_=OutputSafetyStatus::Armed;dualPlace(2,2);
    const auto configuration=controlConfigurationSnapshot();const auto ers=ersConfig_;const auto sequence=ersSequence_;
    const bool settingsPending=settingsSavePending_,channelsPending=channelParameterSavePending_,moduleEnabled=moduleLink_.enabled();
    const auto speed=speedStatistics_;SensorStatistics savedStatistics[DashboardSensorCount];
    if(telemetryHistory_)memcpy(savedStatistics,sensorStatistics_.data(),sizeof(savedStatistics));
    dualRender(0,dualTelemetry,now);dualUnchanged(0,dualTelemetry,now);
    controlDesign(130,activate);controlDesign(154,activate);assert(activePage_==UiPage::Dashboard);
    assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==dualCuts&&moduleLink_.enabled()==moduleEnabled);
    assert(equivalentControlConfiguration(configuration,controlConfigurationSnapshot()));
    assert(!memcmp(&ers,&ersConfig_,sizeof(ers))&&sequence==ersSequence_);
    assert(!memcmp(&speed,&speedStatistics_,sizeof(speed)));
    if(telemetryHistory_)assert(!memcmp(savedStatistics,sensorStatistics_.data(),sizeof(savedStatistics)));
    assert(settingsPending==settingsSavePending_&&channelsPending==channelParameterSavePending_);
    armRequested_=false;designSafety_=OutputSafetyStatus::Cut;
    // Editor cycle and Cancel retain the existing model layout and fields.
    const auto beforePicker=dashboardLayouts_[activeModelSlot_];
    drawDesignPage(UiPage::WidgetSelect);controlDesign(110,activate);
    widgetDraft_.kind=DashboardWidgetKind::BatteryLoadBalance;controlDesign(120,activate);
    assert(widgetDraft_.kind==DashboardWidgetKind::SpeedRpm);
    assert(dashboardWidgetKindAvailable(widgetDraft_.kind));controlDesign(125,activate);
    assert(!memcmp(&beforePicker,&dashboardLayouts_[activeModelSlot_],sizeof(beforePicker)));
    assert(requestDisplayRotation());applyDisplayRotation();
}
// The appended kind survives the existing per-model seven-byte layout storage.
dualPlace(1,1,3);const auto dualSaved=dashboardLayouts_[activeModelSlot_];
const uint8_t dualOtherSlot=(activeModelSlot_+1)%kModelMemoryCount;const auto dualOther=dashboardLayouts_[dualOtherSlot];
saveModelMemorySettings();dashboardLayouts_[activeModelSlot_]=DashboardLayout{};loadModelMemorySettings();
assert(!memcmp(&dualSaved,&dashboardLayouts_[activeModelSlot_],sizeof(dualSaved)));
assert(!memcmp(&dualOther,&dashboardLayouts_[dualOtherSlot],sizeof(dualOther)));
assert(!armRequested_&&!safetyResetRequested_&&auditCutRequests==dualCuts);
dashboardLayouts_[activeModelSlot_]=dualOriginal;
lv_obj_remove_event_cb(lv_scr_act(),dualObjectEvent);dualDisplay->driver->monitor_cb=dualPriorMonitor;
safetyCutHandler_=dualCutHandler;
Serial.println("Speed/RPM dual dials: actual LVGL spans/origins/maximize, labels/scales, independent freshness, zero/missing/extreme fields, unchanged zero-pixel/no-allocation ticks, safe taps and model persistence; both orientations PASS");
return true;
