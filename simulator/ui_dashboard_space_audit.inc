// Shared header and dashboard geometry use synthetic data and disposable preferences.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;
startupPreflightComplete_=true;hardwareRecovery_=false;
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
const auto spaceOriginal=dashboardLayouts_[activeModelSlot_];
const auto spaceModel=activeModelSlot_;
const bool spaceManaged=moduleLinkManaged_,spaceAbs=brakeAbs_.enabled;
const bool spaceLinkEnabled=moduleLink_.enabled();
const auto spaceCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto spaceCuts=auditCutRequests;
moduleLinkManaged_=true;brakeAbs_.enabled=true;
unsigned spaceCases=0,spaceMaximized=0;
auto spaceShot=[&](const char *name){
    if(const char *prefix=getenv("JR_R1_DASHBOARD_SPACE_IMAGES")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
        lv_refr_now(nullptr);simulatorSaveScreenshot(path);
    }
};
auto spaceTarget=[&](uint8_t action){
    for(uint8_t i=0;i<pageFocusCount_;++i){const auto &target=pageFocusTargets_[i];
        if(target.enabled&&target.handler==&TransmitterUi::controlDesign&&target.argument==action)
            return target.activate;
    }
    assert(false&&"Expected dashboard action target");return pageFocusTargets_[0].activate;
};
auto spaceGuards=[&]{
    assert(activeModelSlot_==spaceModel&&!armRequested_&&!safetyResetRequested_);
    assert(moduleLink_.enabled()==spaceLinkEnabled&&auditCutRequests==spaceCuts);
};
auto spaceBounds=[&](uint8_t cell){
    auto panel=widgetViews_[cell].panel;assert(panel);
    lv_obj_update_layout(lv_scr_act());
    const int x=lv_obj_get_x(panel),y=lv_obj_get_y(panel);
    const int width=lv_obj_get_width(panel),height=lv_obj_get_height(panel);
    assert(x>=10&&x+width<=kScreenWidth-10);
    assert(y>=kDashboardWidgetTop&&y+height<=designFooterY-6);
    // Both native widget navigation and header shortcuts follow rendered bounds.
    const auto target=spaceTarget(130+cell);
    assert(target.x==x&&target.y==y&&target.width<=width&&target.height<=height);
    const auto link=spaceTarget(63),wifi=spaceTarget(205);
    assert(link.y>=0&&link.y+link.height==UiLayout::HeaderHeight);
    assert(wifi.y==0&&wifi.y+wifi.height==UiLayout::HeaderHeight);
    assert(link.y+link.height<y&&wifi.y+wifi.height<y);
    if(dashboardWidgetForView(cell).kind==DashboardWidgetKind::Map){
        const auto &view=widgetViews_[cell];assert(view.basemap);
        assert(lv_obj_get_parent(view.basemap)==panel);
        assert(lv_obj_get_width(view.basemap)==width-2&&lv_obj_get_height(view.basemap)==height-2);
        assert(view.mapViewport.fillSavedRaster);
    }
    spaceGuards();
};
for(unsigned orientation=0;orientation<2;++orientation){
    dashboardLayouts_[activeModelSlot_]=defaultDashboardLayout();drawDashboard();
    spaceShot("overview");
    for(int kind=1;kind<int(DashboardWidgetKind::Count);++kind){
        const auto widgetKind=static_cast<DashboardWidgetKind>(kind);
        if(!dashboardWidgetKindAvailable(widgetKind))continue;
        for(uint8_t cell=0;cell<4;++cell)for(uint8_t width=1;width<=2;++width)for(uint8_t height=1;height<=2;++height){
            if(cell%2+width>2||cell/2+height>2)continue;
            dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
            DashboardWidget widget;widget.kind=widgetKind;widget.width=width;widget.height=height;
            if(widgetKind==DashboardWidgetKind::ControlHistory)widget.style=DashboardWidgetStyle::Graph;
            else if(widgetKind==DashboardWidgetKind::SpeedRpm)widget.style=DashboardWidgetStyle::Dials;
            else if(widgetKind==DashboardWidgetKind::AllChannels||widgetKind==DashboardWidgetKind::Channels)
                widget.style=DashboardWidgetStyle::Bars;
            placeDashboardWidget(dashboardLayouts_[activeModelSlot_],cell,widget);
            const auto saved=dashboardLayouts_[activeModelSlot_];
            drawDashboard();spaceBounds(cell);++spaceCases;
            assert(!memcmp(&saved,&dashboardLayouts_[activeModelSlot_],sizeof(saved)));
            if(cell/2==0)assert(lv_obj_get_y(widgetViews_[cell].panel)==kDashboardWidgetTop);
            if(!portraitUi)assert(lv_obj_get_height(widgetViews_[cell].panel)==(height==1?113:234));
            if(widgetKind==DashboardWidgetKind::Map&&width==2&&height==2)spaceShot("map-full-grid");
            // Cycle the maximized origin across kinds while keeping all original
            // size/anchor combinations above under the same geometry checks.
            if(width==1&&height==1&&cell==(kind-1)%4){
                const auto target=spaceTarget(130+cell);
                const int x=target.x+8,y=target.y+8;
                assert(handleDashboardWidgetHold(x,y,x,y,700));
                assert(maximizedWidget_==cell);spaceBounds(cell);++spaceMaximized;
                assert(lv_obj_get_y(widgetViews_[cell].panel)==kDashboardWidgetTop);
                assert(lv_obj_get_height(widgetViews_[cell].panel)==designFooterY-kDashboardWidgetTop-6);
                const auto minimize=spaceTarget(243);
                assert(minimize.y>=kDashboardWidgetTop&&minimize.y+minimize.height<designFooterY);
                lastTouchDurationMs_=100;
                handleTouch(minimize.x+minimize.width/2,minimize.y+minimize.height/2);
                assert(activePage_==UiPage::Dashboard&&maximizedWidget_==-1);
                assert(!memcmp(&saved,&dashboardLayouts_[activeModelSlot_],sizeof(saved)));
                spaceGuards();
            }
        }
    }
    // The top-right widget begins at y40, inside the former 44 px header region.
    // Test the columns shared with the Wi-Fi and link touch targets above it.
    dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
    DashboardWidget channels;channels.kind=DashboardWidgetKind::Channels;channels.style=DashboardWidgetStyle::Bars;
    placeDashboardWidget(dashboardLayouts_[activeModelSlot_],1,channels);
    const auto routingLayout=dashboardLayouts_[activeModelSlot_];
    drawDashboard();
    const auto widgetTarget=spaceTarget(131);
    const int reclaimedY=widgetTarget.y+1;
    assert(reclaimedY>=UiLayout::HeaderHeight&&reclaimedY<44);
    for(const auto &shortcut:{spaceTarget(205),spaceTarget(63)}){
        const int left=max(shortcut.x,widgetTarget.x);
        const int right=min(shortcut.x+shortcut.width,widgetTarget.x+widgetTarget.width);
        assert(left<right);
        const int x=(left+right)/2;
        for(uint32_t duration:{100U,900U}){
            drawDashboard();lastTouchDurationMs_=duration;touchStartX_=x;touchStartY_=reclaimedY;
            handleTouch(x,reclaimedY);assert(activePage_==UiPage::ChannelMonitor);
            assert(!memcmp(&routingLayout,&dashboardLayouts_[activeModelSlot_],sizeof(routingLayout)));
            spaceGuards();
        }
        drawDashboard();assert(handleDashboardWidgetHold(x,reclaimedY,x,reclaimedY,700)&&maximizedWidget_==1);
        navigateBack();assert(activePage_==UiPage::Dashboard&&maximizedWidget_==-1);
        spaceGuards();
    }
    // The same shortcuts remain usable in their new, shorter header region.
    drawDashboard();const auto link=spaceTarget(63);
    const int linkX=link.x+link.width/2,linkY=link.y+link.height/2;
    lastTouchDurationMs_=100;handleTouch(linkX,linkY);
    assert(activePage_==UiPage::RadioDiagnostics);spaceGuards();
    drawDashboard();lastTouchDurationMs_=900;touchStartX_=linkX;touchStartY_=linkY;
    handleTouch(touchStartX_,touchStartY_);assert(activePage_==UiPage::ModuleLink);spaceGuards();
    drawDashboard();lastTouchDurationMs_=100;handleTouch(kScreenWidth-20,18);
    assert(activePage_==UiPage::NetworkOptions);spaceGuards();
    drawDashboard();assert(requestDisplayRotation());applyDisplayRotation();
}
dashboardLayouts_[activeModelSlot_]=spaceOriginal;moduleLinkManaged_=spaceManaged;brakeAbs_.enabled=spaceAbs;
safetyCutHandler_=spaceCutHandler;drawDashboard();
Serial.printf("Dashboard space: %u widget size/anchor cases, %u maximizations, both orientations, map growth, reclaimed-band/header touch routing and output/layout guards PASS\n",spaceCases,spaceMaximized);
return true;
