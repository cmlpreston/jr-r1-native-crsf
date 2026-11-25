// Actual shared-panel rendering and hit testing. Existing Drive-hold audits
// independently cover output-release semantics, blockers and remote expiry.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
hardwareRecovery_=false;displaySleeping_=false;SetupPortal::stop();
for(unsigned orientation=0;orientation<2;++orientation){
    auto shot=[&](const char *name){if(const char *prefix=getenv("JR_R1_FEEDBACK_OVERLAY_IMAGES")){
        lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);char path[256];
        snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
        simulatorSaveScreenshot(path);
    }};
    auto underlay=[&](){return makeButton(30,(kScreenHeight-LiveFeedback::Height)/2+40,80,40,"Underlay");};
    auto check=[&](lv_obj_t *panel,lv_obj_t *caption,lv_obj_t *target){
        lv_obj_update_layout(lv_scr_act());
        assert(lv_obj_get_x(panel)==24&&lv_obj_get_y(panel)==(kScreenHeight-112)/2);
        assert(lv_obj_get_width(panel)==kScreenWidth-48&&lv_obj_get_height(panel)==112);
        assert(lv_obj_get_style_radius(panel,0)==kUiCornerRadius);
        assert(lv_obj_get_style_border_color(panel,0).full==colour(cCyan).full);
        assert(lv_obj_get_style_bg_color(panel,0).full==colour(cPanel2).full);
        assert(lv_obj_get_style_text_font(lv_obj_get_child(panel,0),0)==UiText::font(UiText::Secondary));
        assert(lv_obj_get_style_text_font(caption,0)==UiText::font(UiText::Caption));
        std::vector<lv_obj_t*> pending{panel};
        while(!pending.empty()){
            auto object=pending.back();pending.pop_back();
            for(auto flag:{LV_OBJ_FLAG_CLICKABLE,LV_OBJ_FLAG_SCROLLABLE,LV_OBJ_FLAG_CLICK_FOCUSABLE})
                assert(!lv_obj_has_flag(object,flag));
            assert(lv_obj_get_group(object)==nullptr);
            for(uint32_t n=0;n<lv_obj_get_child_cnt(object);++n)pending.push_back(lv_obj_get_child(object,n));
        }
        lv_point_t point{50,static_cast<lv_coord_t>((kScreenHeight-112)/2+55)};
        assert(lv_indev_search_obj(lv_scr_act(),&point)==target); // Real LVGL pass-through.
    };
    for(auto kind:{ChannelParameterKind::Trim,ChannelParameterKind::Subtrim}){
        armRequested_=true;safetyResetRequested_=false;
        channelModel_.parameters[steeringChannel()-1]=ChannelParameterValues{};
        drawDashboard();auto target=underlay();const auto focusCount=pageFocusCount_;
        assert(applyChannelParameterAction(encodeChannelParameterTarget(steeringChannel(),kind),10));
        assert(armRequested_&&!safetyResetRequested_&&pageFocusCount_==focusCount);
        check(adjustmentOverlay_,lv_obj_get_child(adjustmentOverlay_,2),target);
        shot(kind==ChannelParameterKind::Trim?"trim":"subtrim");
        auto panel=adjustmentOverlay_;updateChannelAdjustment(adjustmentChangedMs_+1199);assert(adjustmentOverlay_==panel);
        updateChannelAdjustment(adjustmentChangedMs_+1200);assert(!adjustmentOverlay_&&!adjustmentChannel_);
    }
    armRequested_=false;drawDashboard();auto target=underlay();const auto focusCount=pageFocusCount_;
    touchWasDown_=armGestureValid_=true;touchStartPage_=activePage_;touchDownStartedMs_=1000;
    touchStartX_=30;touchStartY_=kScreenHeight-(portraitUi?40:20);
    updateDriveHoldFeedback(1999);assert(!driveHoldOverlay_);
    updateDriveHoldFeedback(2000);assert(driveHoldOverlay_&&lv_bar_get_value(driveHoldBar_)==0);
    check(driveHoldOverlay_,driveHoldLabel_,target);assert(pageFocusCount_==focusCount);
    updateDriveHoldFeedback(2400);assert(lv_bar_get_value(driveHoldBar_)==400&&!armRequested_);shot("hold-enable");
    // A simultaneous physical adjustment must not cover an active safety hold.
    assert(applyChannelParameterAction(encodeChannelParameterTarget(steeringChannel(),ChannelParameterKind::Trim),20));
    assert(lv_obj_get_child_id(adjustmentOverlay_)>lv_obj_get_child_id(driveHoldOverlay_));
    updateDriveHoldFeedback(2400);
    assert(lv_obj_get_child_id(driveHoldOverlay_)+1==lv_obj_get_child_cnt(lv_scr_act()));shot("hold-priority");
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    auto *feedbackDisplay=lv_disp_get_default();const auto oldFeedbackMonitor=feedbackDisplay->driver->monitor_cb;
    feedbackDisplay->driver->monitor_cb=auditRefreshMonitor;auditRedrawnPixels=0;
    for(unsigned n=0;n<10;++n){updateDriveHoldFeedback(2400);lv_refr_now(nullptr);}
    assert(auditRedrawnPixels==0);feedbackDisplay->driver->monitor_cb=oldFeedbackMonitor;
    updateChannelAdjustment(adjustmentChangedMs_+1200);
    updateDriveHoldFeedback(2799);assert(lv_bar_get_value(driveHoldBar_)<800&&!armRequested_);
    updateDriveHoldFeedback(2800);assert(lv_bar_get_value(driveHoldBar_)==800&&!armRequested_);
    assert(strstr(lv_label_get_text(driveHoldLabel_),"Release to request"));shot("hold-ready");
    armRequested_=true;updateDriveHoldFeedback(2400);
    assert(armRequested_&&strstr(lv_label_get_text(driveHoldLabel_),"Keep holding to CUT"));shot("hold-cut");
    armGestureValid_=false;updateDriveHoldFeedback(2801);assert(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN));
    armGestureValid_=true;bootPinUnlocked_=false;updateDriveHoldFeedback(2802);assert(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN));
    bootPinUnlocked_=true;displaySleeping_=true;updateDriveHoldFeedback(2803);assert(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN));
    displaySleeping_=false;hardwareRecovery_=true;updateDriveHoldFeedback(2804);assert(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN));
    hardwareRecovery_=false;touchWasDown_=false;armGestureValid_=false;armRequested_=false;
    // A remote hold has no Drive-tab feedback delay, and cancellation hides it.
    remoteArmPage_=static_cast<uint32_t>(activePage_);remoteArmHold_.begin(remoteArmPage_,3000);
    updateDriveHoldFeedback(3400);assert(!lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN)&&lv_bar_get_value(driveHoldBar_)==400);
    remoteArmHold_.cancel();updateDriveHoldFeedback(3401);assert(lv_obj_has_flag(driveHoldOverlay_,LV_OBJ_FLAG_HIDDEN));
    drawDashboard();assert(!driveHoldOverlay_&&!adjustmentOverlay_);
    assert(requestDisplayRotation());applyDisplayRotation();
}
Serial.println("Shared feedback panels: centered shell/typography, trim/subtrim expiry, real LVGL touch pass-through/no focus, Drive delay/progress/cancellation and remote feedback; both orientations PASS");
return true;
