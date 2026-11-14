// Actual editor events, LVGL object lifetime/dirty pixels and page epochs.
// Before-change measurements were captured with this event sequence before
// replacing the rebuild implementation; the final fixture always checks reuse.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;bindingCount_=0;
struct EditorChurn {uint32_t created=0;} editorChurn;
auto editorObjects=[](lv_event_t *event){
    auto *churn=static_cast<EditorChurn *>(lv_event_get_user_data(event));
    if(lv_event_get_code(event)==LV_EVENT_CHILD_CREATED)++churn->created;
};
auto *editorDisplay=lv_disp_get_default();const auto priorEditorMonitor=editorDisplay->driver->monitor_cb;
editorDisplay->driver->monitor_cb=auditRefreshMonitor;
lv_obj_add_event_cb(lv_scr_act(),editorObjects,LV_EVENT_ALL,&editorChurn);
const auto editorCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
uint64_t editorPixels=0,editorCallsUs=0;uint32_t editorCreated=0,editorActions=0;
for(unsigned orientation=0;orientation<2;++orientation){
    for(uint8_t role:{0,1,2,3,4})for(uint8_t field:{0,1,2,3,4,5,6,7}){
        tuneRole_=role;selectedChannel_=role==0?steeringChannel():role<3?throttleChannel():3;
        channelModel_.setResponse(selectedChannel_,ChannelResponseProfile{});
        channelModel_.parameters[selectedChannel_-1]=ChannelParameterValues{};
        drawDesignPage(UiPage::Tune);armRequested_=true;safetyResetRequested_=false;
        settingsSavePending_=channelParameterSavePending_=false;
        const auto cuts=auditCutRequests;const auto original=channelModel_;
        openDesignEditor(field,UiPage::Tune);lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        auto *const header=lv_obj_get_child(lv_scr_act(),0);auto *const curve=designCurve_;
        auto *const step=editorStepButton_;const auto focusCount=pageFocusCount_;
        auto *const number=editorValueLabel_;
        PageFocusTarget targets[kMaximumPageFocusTargets];memcpy(targets,pageFocusTargets_,sizeof(targets));
        const auto epoch=SetupPortal::screenEpochForTesting();
        lv_mem_monitor_t before;lv_mem_monitor(&before);
        editorChurn=EditorChurn{};auditRedrawnPixels=0;uint32_t callUs=0;
        // The real touch dispatcher exercises focus registration and coordinates.
        for(uint8_t action:{32,30,31,33}){
            bool touched=false;
            for(uint8_t n=0;n<pageFocusCount_;++n){const auto target=pageFocusTargets_[n];
                if(target.handler==&TransmitterUi::controlDesign&&target.argument==action){
                    const auto began=micros();
                    handleTouch(target.activate.x+target.activate.width/2,target.activate.y+target.activate.height/2);
                    callUs+=uint32_t(micros()-began);touched=true;break;
                }
            }
            assert(touched&&activePage_==UiPage::ValueEditor);
            lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
            assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==cuts);
            assert(memcmp(&original,&channelModel_,sizeof(original))==0);
            assert(header==lv_obj_get_child(lv_scr_act(),0)&&curve==designCurve_&&step==editorStepButton_);
            assert(editorValueLabel_==number);
            if(number){
                assert(abs(lv_obj_get_x(number)+lv_obj_get_width(number)/2-kScreenWidth/2)<=1);
                assert(lv_obj_get_y(number)==83);
            }
            assert(pageFocusCount_==focusCount&&memcmp(targets,pageFocusTargets_,sizeof(targets))==0);
            assert(SetupPortal::screenEpochForTesting()==epoch);
        }
        lv_mem_monitor_t after;lv_mem_monitor(&after);
        editorPixels+=auditRedrawnPixels;editorCallsUs+=callUs;
        editorCreated+=editorChurn.created;editorActions+=4;
        assert(editorChurn.created==0);
        if(role==0||field==1)Serial.printf("Editor refresh in-place %s role=%u field=%u calls=%luus pixels=%llu created=%lu blocks=%lu->%lu used=%lu->%lu\n",
            portraitUi?"portrait":"landscape",role,field,
            (unsigned long)callUs,(unsigned long long)auditRedrawnPixels,(unsigned long)editorChurn.created,
            (unsigned long)before.used_cnt,(unsigned long)after.used_cnt,(unsigned long)(before.total_size-before.free_size),(unsigned long)(after.total_size-after.free_size));
        if(const char *prefix=getenv("JR_R1_EDITOR_REFRESH_IMAGES"))if(role==0){
            char path[256];snprintf(path,sizeof(path),"%s-%s-%u.bmp",prefix,portraitUi?"portrait":"landscape",field);simulatorSaveScreenshot(path);
        }
        // Resetting an already-reset draft must not redraw or allocate objects.
        editorChurn=EditorChurn{};auditRedrawnPixels=0;
        for(unsigned repeat=0;repeat<10;++repeat){controlDesign(33,activate);lv_refr_now(nullptr);}
        assert(auditRedrawnPixels==0&&editorChurn.created==0);
        controlDesign(34,activate);assert(activePage_==UiPage::Tune);
        assert(memcmp(&original,&channelModel_,sizeof(original))==0&&armRequested_);
        assert(SetupPortal::screenEpochForTesting()!=epoch);
    }
    armRequested_=false;assert(requestDisplayRotation());applyDisplayRotation();
}
lv_obj_remove_event_cb(lv_scr_act(),editorObjects);
editorDisplay->driver->monitor_cb=priorEditorMonitor;safetyCutHandler_=editorCutHandler;
Serial.printf("Editor total in-place actions=%lu calls=%lluus pixels=%llu created=%lu\n",
    (unsigned long)editorActions,(unsigned long long)editorCallsUs,(unsigned long long)editorPixels,(unsigned long)editorCreated);
Serial.println("Editor refresh: actual touch/focus, unchanged objects/chrome, draft/safety/epoch invariants, cancellation and idle redraw in both orientations PASS");
return true;
