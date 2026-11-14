#include <cassert>
bool TransmitterUi::runSimulatorSetupDetailsAudit()
{
    const auto activate=PageControlEvent::Activate;
    bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;armRequested_=false;
    const auto oldRoles=channelRoleSettings_;const uint8_t oldSelected=selectedChannel_;
    const auto oldModel=channelModel_;const auto oldReturn=throttleResponseReturnPage_;
    const bool oldPending=elrsRequestPending_,oldLink=elrsReceiverLinkAvailable_,oldUpper=elrsKeyboardUppercase_;
    const auto oldRequest=elrsPendingRequest_;char oldPhrase[sizeof(elrsBindPhrase_)];memcpy(oldPhrase,elrsBindPhrase_,sizeof(oldPhrase));
    auto tap=[&](PageControlHandler handler,uint8_t id,PageControlEvent event=PageControlEvent::Activate){
        for(uint8_t i=0;i<pageFocusCount_;++i){const auto t=pageFocusTargets_[i];if(t.handler!=handler||t.argument!=id)continue;
            const auto r=event==PageControlEvent::Decrease?t.decrease:event==PageControlEvent::Increase?t.increase:t.activate;
            assert(r.width>0&&r.height>0);assert(dispatchPageControlTouch(r.x+r.width/2,r.y+r.height/2));return;}assert(false);
    };
    auto check=[&](){lv_obj_update_layout(lv_scr_act());assert(!detailContentCompacted_);auditTypography(lv_scr_act());
        for(uint8_t i=0;i<pageFocusCount_;++i){const auto &t=pageFocusTargets_[i];for(const auto r:{t.focus,t.activate,t.decrease,t.increase})if(r.width)assert(r.x>=0&&r.y>=0&&r.x+r.width<=kScreenWidth&&r.y+r.height<=kScreenHeight);}
    };
    auto shot=[&](const char *name){check();if(const char *prefix=getenv("JR_R1_SETUP_DETAILS_IMAGES")){lv_refr_now(nullptr);char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);simulatorSaveScreenshot(path);}};
    drawChannelRolesPage();shot("roles");armRequested_=true;safetyResetRequested_=false;
    tap(&TransmitterUi::controlChannelRoles,0,PageControlEvent::Increase);assert(!armRequested_&&safetyResetRequested_);
    channelRoleSettings_=oldRoles;saveChannelRoleSettings();
    drawPage();shot("throttle");const bool reversed=throttleProfile().reversed;armRequested_=true;safetyResetRequested_=false;
    tap(&TransmitterUi::controlThrottleSetting,3);assert(throttleProfile().reversed!=reversed&&!armRequested_&&safetyResetRequested_);
    assert(strcmp(lv_label_get_text(channel2SettingValueLabels_[3]),throttleProfile().reversed?"Reversed":"Normal")==0);
    const auto timeout=systemSettings_.screenTimeoutSeconds;systemSettings_.screenTimeoutSeconds=0;
    CrsfTelemetryState telemetry;CrsfChannelOutputModel outputs;
    for(int16_t input:{int16_t(-1000),int16_t(0),int16_t(1000)}){
        nextDisplayUpdateMs_=0;update(triggerInput_.centre(),steeringInput_.centre(),input,0,kCrsfMid,OutputSafetyStatus::Cut,telemetry,0,false,outputs,false);
        assert(lv_bar_get_value(channel2LiveBar_)==input&&!armRequested_);
    }
    systemSettings_.screenTimeoutSeconds=timeout;
    channelModel_=oldModel;selectedChannel_=steeringChannel();throttleResponseReturnPage_=UiPage::ChannelResponseSetup;
    drawThrottleResponsePage();shot("curve");assert(throttleGraphMarker_&&throttleGraphLiveLabel_);
    const auto point=throttleCurvePoints_[0];refreshThrottleResponseGraph(500);assert(strstr(lv_label_get_text(throttleGraphLiveLabel_),"IN"));assert(throttleCurvePoints_[0].x==point.x);
    tap(&TransmitterUi::controlResponseTab,1);shot("positive");const int expo=selectedResponseProfile().positiveExpoPercent;
    armRequested_=true;safetyResetRequested_=false;
    const auto expoCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;const auto expoCuts=auditCutRequests;
    tap(&TransmitterUi::controlThrottleResponse,3,PageControlEvent::Increase);
    assert(selectedResponseProfile().positiveExpoPercent==min(100,expo+kResponseExpoStepPercent)&&armRequested_&&!safetyResetRequested_&&auditCutRequests==expoCuts);
    safetyCutHandler_=expoCutHandler;
    tap(&TransmitterUi::controlResponseTab,2);shot("negative");
    keypadPage(-1);assert(!responseCurveView_&&portraitResponseBank_==0);
    assert(handlePageSwipe(240,150,80,150,300));assert(!responseCurveView_&&portraitResponseBank_==1);
    tap(&TransmitterUi::controlThrottleHeader,0);assert(activePage_==UiPage::ChannelSpeed);shot("ramp");
    const int ramp=selectedResponseProfile().negativeRampMs;armRequested_=true;safetyResetRequested_=false;
    tap(&TransmitterUi::controlChannelSpeed,1,PageControlEvent::Increase);assert(selectedResponseProfile().negativeRampMs==min(2000,ramp+50)&&!armRequested_&&safetyResetRequested_);
    navigateBack();assert(activePage_==UiPage::ThrottleResponse);navigateBack();assert(activePage_==UiPage::ChannelResponseSetup);
    drawFirmwareInfoPage();shot("firmware");navigateBack();assert(activePage_==UiPage::SystemMenu);
    elrsRequestPending_=false;elrsReceiverLinkAvailable_=false;elrsKeyboardUppercase_=false;elrsBindPhrase_[0]=0;
    drawElrsBindingPage();shot("binding");tap(&TransmitterUi::controlElrsBinding,1);assert(!elrsRequestPending_);
    tap(&TransmitterUi::controlElrsBinding,0);assert(!elrsRequestPending_); // blank does not unbind
    tap(&TransmitterUi::controlElrsBinding,46);assert(elrsPhraseEditing_);shot("keyboard");
    tap(&TransmitterUi::controlElrsBinding,5);assert(strcmp(elrsBindPhrase_,"1")==0);
    tap(&TransmitterUi::controlElrsBinding,43);tap(&TransmitterUi::controlElrsBinding,15);assert(strcmp(elrsBindPhrase_,"1Q")==0);
    tap(&TransmitterUi::controlElrsBinding,44);tap(&TransmitterUi::controlElrsBinding,45);assert(strcmp(elrsBindPhrase_,"1Q")==0);
    tap(&TransmitterUi::controlElrsBinding,47);assert(!elrsPhraseEditing_&&!elrsRequestPending_);
    armRequested_=true;safetyResetRequested_=false;tap(&TransmitterUi::controlElrsBinding,2);
    assert(elrsRequestPending_&&elrsPendingRequest_.action==ElrsManagementAction::ReadUid&&!armRequested_&&safetyResetRequested_);
    elrsRequestPending_=false;tap(&TransmitterUi::controlElrsBinding,4);assert(!elrsBindPhrase_[0]&&!elrsRequestPending_);
    drawEscProgrammingPage();for(uint8_t tab=0;tab<5;++tab){tap(&TransmitterUi::controlEscProgramming,tab);shot(tab==0?"esc-link":tab==1?"esc-drive":tab==2?"esc-brake":tab==3?"esc-motor":"esc-heat");}
    tap(&TransmitterUi::controlEscProgramming,0);
    armRequested_=true;safetyResetRequested_=false;escPairCommissioned_=false;tap(&TransmitterUi::controlEscProgramming,11);
    assert(!armRequested_&&safetyResetRequested_&&!elrsRequestPending_&&!escProgrammer_.selected());
    armRequested_=false;calibrationReturnPage_=UiPage::Channel2;startCalibration(latestRawAdc_,false);assert(calibrationActive());shot("calibration");
    tap(&TransmitterUi::controlNavigation,0);assert(!calibrationActive()&&!armRequested_);
    channelModel_=oldModel;channelRoleSettings_=oldRoles;saveChannelRoleSettings();selectedChannel_=oldSelected;throttleResponseReturnPage_=oldReturn;
    elrsRequestPending_=oldPending;elrsReceiverLinkAvailable_=oldLink;elrsPendingRequest_=oldRequest;elrsKeyboardUppercase_=oldUpper;memcpy(elrsBindPhrase_,oldPhrase,sizeof(oldPhrase));
    settingsSavePending_=false;safetyResetRequested_=false;drawMainMenu();
    Serial.println("Setup details: common scaffold/rows, real touch targets, response pages/live curve, safe configuration CUT, binding keyboard/link/blank guards, CUT-only uncommissioned ESC guard, calibration Cancel, Back and bounds PASS");return true;
}
