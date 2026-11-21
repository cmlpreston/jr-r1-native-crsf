// Exercise the physical touch sampling path, including its release guard.
bootPinUnlocked_=true;
startupPreflightComplete_=true;
pinPurpose_=PinPurpose::None;
moduleLinkManaged_=false;
armRequested_=false;
designSafety_=OutputSafetyStatus::Cut;
SetupPortal::begin(this);
SetupPortal::stop();
const auto previousCutHandler=safetyCutHandler_;
safetyCutHandler_=auditConfigurationCut;
const bool previousHeaderHidden=dashboardHeaderHidden_;
dashboardHeaderHidden_=true;

auto sampleTouch=[&](int x,int y){
    simulatorSetTouchForTesting(x,y,true);
    updateTouch();
};
auto releaseTouch=[&]{
    simulatorSetTouchForTesting(0,0,false);
    updateTouch();
    if (touchWasDown_) {
        delay(kTouchReleaseDebounceMs+1);
        updateTouch();
    }
    if (discardTouchUntilRelease_) updateTouch();
    touchIgnoreUntilMs_=millis();
};
auto lockFromSettings=[&]{
    drawDisplayPage();
    for (uint8_t n=0;n<pageFocusCount_;++n) {
        const auto &target=pageFocusTargets_[n];
        if (target.handler==&TransmitterUi::controlTouchLock) {
            sampleTouch(target.activate.x+target.activate.width/2,
                        target.activate.y+target.activate.height/2);
            releaseTouch();
            assert(touchLocked_ && activePage_==UiPage::Dashboard);
            return;
        }
    }
    assert(false && "Display should have a Touch lock action");
};

for (uint8_t orientation=0;orientation<2;++orientation) {
    releaseTouch();
    designSafety_=OutputSafetyStatus::NeutralRequired;
    armRequested_=true;
    const auto cuts=auditCutRequests;
    remoteArmHold_.begin(42,millis());
    lockFromSettings();
    assert(!armRequested_ && !remoteArmHold_.id() && auditCutRequests==cuts+1);

    const int y=touchLockStripY()+kTouchLockStripHeight/2;
    const int cutX=touchLockSideWidth()/2;
    const int unlockX=kScreenWidth-cutX;
    assert(touchLockStrip_ && touchLockOutput_ && !designHeaderOutput_);
    assert(!strcmp(lv_label_get_text(touchLockOutput_),"CUT"));
    lv_obj_update_layout(lv_layer_top());
    lv_area_t bounds;
    lv_obj_get_coords(touchLockStrip_,&bounds);
    assert(bounds.x1==0 && bounds.x2==kScreenWidth-1 && bounds.y1==touchLockStripY());
    assert(lv_area_get_height(&bounds)==kTouchLockStripHeight);
    auditTypography(touchLockStrip_);

    SetupPortal::start();
    assert(SetupPortal::canControl());
    int left,top,width,height;
    assert(!portalDriveHoldBounds(left,top,width,height));
    assert(!portalRemoteInput(unlockX,y,0,0,0) && !portalRemoteInput(0,0,2,0,0));
    assert(!portalPageSwipe(20,110,kScreenWidth-20,110,500));
    assert(touchLocked_ && activePage_==UiPage::Dashboard);
    SetupPortal::stop();

    // Physical navigation still works through the modal Quick menu.
    const uint32_t keyAt=millis();
    assert(handleKeypadEvent(PhysicalControlId::KeyMode,InputEvent::Pressed,keyAt));
    assert(handleKeypadEvent(PhysicalControlId::KeyMode,InputEvent::Released,keyAt+40));
    assert(touchLocked_ && activePage_==UiPage::QuickNavigation && touchLockStrip_);
    assert(handleKeypadEvent(PhysicalControlId::KeyMode,InputEvent::Pressed,keyAt+100));
    assert(handleKeypadEvent(PhysicalControlId::KeyMode,InputEvent::Released,keyAt+800));
    assert(touchLocked_ && activePage_==UiPage::Dashboard && touchLockStrip_);
    drawDesignPage(UiPage::OutputStatus);
    sampleTouch(kScreenWidth/2,portraitUi?262:208);
    touchDownStartedMs_-=kDashboardArmHoldMs+1;
    releaseTouch();
    assert(!armRequested_ && activePage_==UiPage::OutputStatus && touchLocked_);
    drawDashboard();

    sampleTouch(unlockX,y); // A tap alone cannot unlock.
    releaseTouch();
    assert(touchLocked_);
    sampleTouch(kScreenWidth/2,y);
    sampleTouch(unlockX,y); // A drag starting outside the button cannot unlock.
    touchDownStartedMs_-=kTouchUnlockHoldMs+1;
    releaseTouch();
    assert(touchLocked_);
    sampleTouch(unlockX,y);
    sampleTouch(kScreenWidth/2,y);
    sampleTouch(unlockX,y); // Leaving the target cancels even after re-entry.
    touchDownStartedMs_-=kTouchUnlockHoldMs+1;
    releaseTouch();
    assert(touchLocked_);
    sampleTouch(unlockX,y);
    touchDownStartedMs_-=kTouchUnlockHoldMs+1;
    releaseTouch();
    assert(!touchLocked_ && !touchLockStrip_ && !armRequested_);

    // Locking a running model does not CUT until the dedicated control is used.
    designSafety_=OutputSafetyStatus::Armed;
    armRequested_=true;
    lockFromSettings();
    assert(armRequested_ && auditCutRequests==cuts+1);
    assert(!strcmp(lv_label_get_text(touchLockOutput_),"LIVE"));
    sleepDisplay();
    drawLogoSaver();
    assert(!displaySleeping_ && activePage_==UiPage::Dashboard);
    sampleTouch(cutX,y);
    assert(touchWasDown_ && !armRequested_ && auditCutRequests==cuts+2);
    designSafety_=OutputSafetyStatus::Cut;
    touchDownStartedMs_-=kDashboardArmHoldMs+kDriveHoldFeedbackDelayMs+1;
    releaseTouch();
    assert(!armRequested_ && touchLocked_ && auditCutRequests==cuts+2);

    // Both wake paths consume the first touch and restore the lock.
    drawLogoSaver();
    assert(activePage_==UiPage::LogoSaver && !touchLockStrip_);
    sampleTouch(unlockX,y);
    touchDownStartedMs_-=kTouchUnlockHoldMs+1;
    releaseTouch();
    assert(touchLocked_ && activePage_==UiPage::Dashboard && touchLockStrip_);
    sleepDisplay();
    assert(displaySleeping_);
    displayWakeArmed_=true;
    sampleTouch(unlockX,y);
    assert(!displaySleeping_ && touchLocked_);
    releaseTouch();
    assert(touchLocked_);

    // A keypad unlock during a held Drive touch cannot convert its release
    // into an ordinary arm gesture. A new, complete hold can still arm.
    sampleTouch(cutX,y);
    assert(touchWasDown_ && !armGestureValid_);
    controlTouchLock(0,activate);
    assert(!touchLocked_ && discardTouchUntilRelease_);
    sampleTouch(cutX,y);
    releaseTouch();
    assert(!armRequested_ && auditCutRequests==cuts+2);
    sampleTouch(cutX,y);
    touchDownStartedMs_-=kDashboardArmHoldMs+kDriveHoldFeedbackDelayMs+1;
    releaseTouch();
    assert(armRequested_ && auditCutRequests==cuts+2);
    requestConfigurationCut();
    assert(requestDisplayRotation());
    applyDisplayRotation();
}

dashboardHeaderHidden_=previousHeaderHidden;
safetyResetRequested_=false;
safetyCutHandler_=previousCutHandler;
Serial.println("Touch lock: physical CUT, unlock gestures, keypad navigation, wake, browser mirror and stale holds; both orientations PASS");
return true;
