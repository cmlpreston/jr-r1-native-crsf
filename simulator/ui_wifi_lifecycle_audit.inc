// Actual UI lifecycle checks: a status visit never owns the background network.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;hardwareRecovery_=false;
armRequested_=false;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Cut;
const auto wifiPriorCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
SetupPortal::begin(this);SetupPortal::start();
assert(SetupPortal::connect("Home Wi-Fi","lifecycle-test-pass"));
SetupPortal::stop();SetupPortal::tick(true);
assert(!SetupPortal::active()&&SetupPortal::state().connected&&SetupPortal::state().running);
const auto wifiOriginalQuick=std::vector<uint8_t>(v2QuickSlots_,v2QuickSlots_+8);
for(unsigned orientation=0;orientation<2;++orientation){
    // Read-only entry/refresh/Back preserve armed state, existing network and
    // the background web transport rather than opening a setup interlock.
    armRequested_=true;safetyResetRequested_=false;const auto readCuts=auditCutRequests;
    drawDesignPage(UiPage::NetworkOptions);controlDesign(210,activate);
    assert(activePage_==UiPage::WifiSetup&&!SetupPortal::active()&&!wifiSetupSessionOwned_);
    assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==readCuts);
    updateWifiPage();assert(activePage_==UiPage::WifiSetup&&!SetupPortal::active());
    navigateBack();assert(activePage_==UiPage::NetworkOptions&&armRequested_);
    assert(!SetupPortal::active()&&SetupPortal::state().connected&&SetupPortal::state().running);
    assert(auditCutRequests==readCuts&&!safetyResetRequested_);
    armRequested_=false;
    // Every status entry route, including Web access and the browser shortcut,
    // stays read-only until an explicit configuration or hotspot action.
    controlDesign(211,activate);assert(wifiWebAccess_&&!SetupPortal::active());navigateBack();
    controlDesign(152,activate);assert(!wifiWebAccess_&&!SetupPortal::active());navigateBack();
    assert(portalEnableScreen());assert(portalRemoteInput(0,0,2));
    assert(activePage_==UiPage::WifiSetup&&!SetupPortal::active());
    assert(portalRemoteInput(0,0,1));assert(activePage_==UiPage::Dashboard&&!SetupPortal::active());
    // Physical Start setup has an immediate CUT request and a bounded owner.
    openWifiPage();armRequested_=true;safetyResetRequested_=false;
    const auto startCuts=auditCutRequests;controlWifi(1,activate);
    assert(SetupPortal::active()&&wifiSetupSessionOwned_&&!armRequested_);
    assert(safetyResetRequested_&&auditCutRequests==startCuts+1);
    navigateBack();assert(activePage_==UiPage::NetworkOptions&&!SetupPortal::active());
    assert(SetupPortal::state().connected&&SetupPortal::state().running&&!armRequested_);
    // Cancelling an editor discards its draft; leaving the landing page closes
    // only the manual session started by that editing visit.
    openWifiPage();controlWifi(15,activate);assert(wifiEdit_==4&&wifiSetupSessionOwned_);
    char wifiSavedPassword[17];snprintf(wifiSavedPassword,sizeof(wifiSavedPassword),"%s",SetupPortal::state().apPassword);
    strcpy(wifiSetupPassword_,"discard-me-123");navigateBack();
    assert(wifiEdit_==0&&wifiSetupPassword_[0]==0&&SetupPortal::active());
    assert(!strcmp(wifiSavedPassword,SetupPortal::state().apPassword));
    navigateBack();assert(!SetupPortal::active()&&!wifiSetupSessionOwned_);
    // Saving a new credential and immediately leaving in the same UI loop
    // keeps background home access available with the changed credential.
    openWifiPage();controlWifi(15,activate);strcpy(wifiSetupPassword_,"life-new-key-123");
    controlWifi(13,activate);assert(wifiEdit_==0&&SetupPortal::active());
    navigateBack();SetupPortal::tick(true);
    assert(!SetupPortal::active()&&SetupPortal::state().connected&&SetupPortal::state().running);
    assert(!strcmp(SetupPortal::state().apPassword,"life-new-key-123")&&!armRequested_);
    openWifiPage();controlWifi(19,activate);assert(wifiEdit_==6);
    char wifiSavedHostname[33];snprintf(wifiSavedHostname,sizeof(wifiSavedHostname),"%s",SetupPortal::state().hostname);
    strcpy(wifiHostname_,"discard-this-name");navigateBack();
    assert(wifiEdit_==0&&!strcmp(wifiSavedHostname,SetupPortal::state().hostname));
    navigateBack();assert(!SetupPortal::active());
    // A scan queued before the web server is running is serviced once ready.
    openWifiPage();controlWifi(1,activate);
    auto &wifiState=const_cast<SetupPortal::State &>(SetupPortal::state());
    wifiState.running=false;controlWifi(3,activate);
    assert(wifiEdit_==3&&wifiScanPending_);
    wifiState.running=true;updateWifiPage();assert(!wifiScanPending_&&wifiState.networkCount>0);
    navigateBack();assert(wifiEdit_==0&&SetupPortal::active());
    navigateBack();assert(!SetupPortal::active());
    // Alternate physical exits cover section tabs, the Wi-Fi icon and Mode.
    openWifiPage();controlWifi(1,activate);controlSectionNavigation(3,activate);
    assert(activePage_==UiPage::MainMenu&&!SetupPortal::active()&&!wifiSetupSessionOwned_);
    openWifiPage();controlWifi(1,activate);handleTouch(kScreenWidth-20,20);
    assert(activePage_==UiPage::NetworkOptions&&!SetupPortal::active());
    openWifiPage();controlWifi(1,activate);keypadAction(PhysicalControlId::KeyMode,true,false);
    assert(activePage_==UiPage::Dashboard&&!SetupPortal::active());
    // The temporary quick menu preserves the visit; Back redraws the actual
    // Wi-Fi content, Empty is a no-op, and a real shortcut ends the visit.
    openWifiPage();controlWifi(1,activate);keypadAction(PhysicalControlId::KeyMode,false,false);
    assert(activePage_==UiPage::QuickNavigation&&SetupPortal::active()&&wifiSetupSessionOwned_);
    loadV2Quick();v2QuickEditing_=false;v2QuickSlots_[0]=0;controlKeypadMenu(0,activate);
    assert(activePage_==UiPage::QuickNavigation&&SetupPortal::active()&&wifiSetupSessionOwned_);
    controlKeypadMenu(8,activate);assert(activePage_==UiPage::WifiSetup&&SetupPortal::active());
    bool hasFinish=false;
    for(uint8_t i=0;i<pageFocusCount_;++i)hasFinish|=pageFocusTargets_[i].handler==&TransmitterUi::controlWifi&&pageFocusTargets_[i].argument==2;
    assert(hasFinish);
    keypadAction(PhysicalControlId::KeyMode,false,false);v2QuickSlots_[0]=1;
    controlKeypadMenu(0,activate);assert(activePage_==UiPage::SensorDetail&&!SetupPortal::active());
    // A session that predates this visit is never auto-closed by page changes.
    SetupPortal::start();openWifiPage();assert(!wifiSetupSessionOwned_);
    armRequested_=true;safetyResetRequested_=false;const auto preexistingExitCuts=auditCutRequests;
    navigateBack();assert(SetupPortal::active()&&!armRequested_&&safetyResetRequested_);
    assert(auditCutRequests==preexistingExitCuts+1);
    // Cancel within a pre-existing setup also clears a latent arm request,
    // while retaining the external session and discarding only the draft.
    openWifiPage();controlWifi(15,activate);armRequested_=true;safetyResetRequested_=false;
    const auto preexistingCancelCuts=auditCutRequests;navigateBack();
    assert(wifiEdit_==0&&SetupPortal::active()&&!armRequested_&&safetyResetRequested_);
    assert(auditCutRequests==preexistingCancelCuts+1);navigateBack();
    openWifiPage();armRequested_=true;safetyResetRequested_=false;
    const auto preexistingSectionCuts=auditCutRequests;controlSectionNavigation(0,activate);
    assert(activePage_==UiPage::Dashboard&&SetupPortal::active()&&!armRequested_&&safetyResetRequested_);
    assert(auditCutRequests==preexistingSectionCuts+1);
    openWifiPage();controlWifi(2,activate);assert(!SetupPortal::active()&&!armRequested_);
    // Remote navigation leaves its transport intact, even if a physical visit
    // originally started the session. The Finish button is physical-only.
    openWifiPage();controlWifi(1,activate);assert(SetupPortal::active());
    remoteTouchActive_=true;controlWifi(2,activate);remoteTouchActive_=false;
    assert(SetupPortal::active()&&activePage_==UiPage::Dashboard&&!wifiSetupSessionOwned_);
    openWifiPage();controlWifi(2,activate);assert(!SetupPortal::active());
    // API navigation has no simulated touch flag; it must still preserve the
    // browser transport rather than auto-closing the physical visit's AP.
    openWifiPage();controlWifi(1,activate);SetupPortal::tick(true);
    assert(portalRfConfigStart(0xee));
    assert(activePage_==UiPage::ElrsTxConfig&&SetupPortal::active()&&!wifiSetupSessionOwned_);
    txConfig_.stop();openWifiPage();controlWifi(2,activate);assert(!SetupPortal::active());
    openWifiPage();remoteTouchActive_=true;remoteTouchRejected_=false;
    controlWifi(1,activate);remoteTouchActive_=false;
    assert(remoteTouchRejected_&&!SetupPortal::active());
    navigateBack();
    // If recovery takes ownership while the page is open, Back preserves its
    // repair transport and the hardware-recovery output veto remains in force.
    openWifiPage();controlWifi(1,activate);hardwareRecovery_=true;navigateBack();
    assert(SetupPortal::active()&&!wifiSetupSessionOwned_&&!controlConfigurationSnapshot().armRequested);
    hardwareRecovery_=false;openWifiPage();controlWifi(2,activate);assert(!SetupPortal::active());
    openWifiPage();hardwareRecovery_=true;armRequested_=true;safetyResetRequested_=false;
    const auto recoveryExitCuts=auditCutRequests;navigateBack();
    assert(!SetupPortal::active()&&!armRequested_&&safetyResetRequested_);
    assert(auditCutRequests==recoveryExitCuts+1);hardwareRecovery_=false;
    // Restart confirmation itself has no need for an AP/session and Cancel
    // returns to read-only status without enabling outputs.
    drawDesignPage(UiPage::NetworkOptions);controlDesign(212,activate);
    assert(activePage_==UiPage::WifiSetup&&wifiEdit_==5&&!SetupPortal::active()&&!armRequested_);
    controlWifi(13,activate);assert(wifiEdit_==0&&!SetupPortal::active());navigateBack();
    assert(requestDisplayRotation());applyDisplayRotation();
}
for(unsigned i=0;i<8;++i)v2QuickSlots_[i]=wifiOriginalQuick[i];
safetyCutHandler_=wifiPriorCutHandler;
Serial.println("Wi-Fi lifecycle: read-only IP/status, explicit setup ownership, physical/remote/alternate exits, editor Cancel, delayed scan, pre-existing/recovery transport and CUT invariants; both orientations PASS");
return true;
