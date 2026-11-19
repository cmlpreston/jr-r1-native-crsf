// Palette persistence and real picker/dashboard rendering, synthetic state only.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
startupPreflightComplete_=true;hardwareRecovery_=false;armRequested_=false;
designSafety_=OutputSafetyStatus::Cut;
const auto original=systemSettings_;
systemSettings_.displayBrightnessPercent=100;uiBrightnessPercent=100;
const auto oldErs=ersConfig_;const auto oldErsSequence=ersSequence_;
for(uint8_t rotation=0;rotation<2;++rotation){
    for(uint8_t light=0;light<lightColourSchemeCount;++light){
        const uint8_t id=light?darkColourSchemeCount+light:0;
        controlColourScheme(id,activate);loadSystemSettings();
        assert(!uiNightMode&&uiLightColourScheme==light&&systemSettings_.lightColourScheme==light);
        assert(colour(cBlack).full==lv_color_hex(lightColourSchemes[light].background).full);
        assert(colour(cCyan).full==lv_color_hex(lightColourSchemes[light].accent).full);
        drawColourSchemePage();lv_obj_update_layout(lv_scr_act());
        assert(colourSchemeBank_==0);
        unsigned choices=0;
        for(uint8_t i=0;i<pageFocusCount_;++i){
            const auto &t=pageFocusTargets_[i];
            if(t.handler==&TransmitterUi::controlColourScheme&&t.argument<colourSchemeChoiceCount){
                ++choices;assert(t.activate.y+t.activate.height<=kScreenHeight-(portraitUi?60:40)-22);
            }
        }
        assert(choices==lightColourSchemeCount);auditTypography(lv_scr_act());lv_refr_now(nullptr);
        char path[160];snprintf(path,sizeof(path),"/tmp/r1-light-scheme-%s-%u-picker.bmp",portraitUi?"portrait":"landscape",light);
        simulatorSaveScreenshot(path);
        drawDashboard();lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);auditTypography(lv_scr_act());
        snprintf(path,sizeof(path),"/tmp/r1-light-scheme-%s-%u-dashboard.bmp",portraitUi?"portrait":"landscape",light);
        simulatorSaveScreenshot(path);
        // Selecting each dark scheme and the assignable Night action retains
        // the independently saved light selection and restores it afterwards.
        for(uint8_t dark=1;dark<=darkColourSchemeCount;++dark){
            controlColourScheme(dark,activate);loadSystemSettings();
            assert(uiNightMode&&uiDarkColourScheme==dark-1&&uiLightColourScheme==light);
        }
        applyTransmitterFunctionAction(static_cast<uint16_t>(TransmitterFunction::ToggleNightMode),1);
        loadSystemSettings();assert(!uiNightMode&&uiLightColourScheme==light);
        drawColourSchemePage();controlColourScheme(251,activate);assert(colourSchemeBank_==1);
        controlColourScheme(250,activate);assert(colourSchemeBank_==0);
        assert(!armRequested_&&!memcmp(&oldErs,&ersConfig_,sizeof(oldErs))&&ersSequence_==oldErsSequence);
    }
    assert(requestDisplayRotation());applyDisplayRotation();
}
{Preferences p;p.begin("jr-r1-system",false);p.putUChar("light_scheme",255);p.end();}
loadSystemSettings();assert(uiLightColourScheme==0);
char oldPreference[512];snprintf(oldPreference,sizeof(oldPreference),"%s/jr-r1-system/light_scheme",getenv("JR_R1_SIM_STATE"));
assert(remove(oldPreference)==0);
loadSystemSettings();assert(uiLightColourScheme==0); // Pre-upgrade preferences.
systemSettings_=original;saveSystemSettings();loadSystemSettings();
Serial.println("Light schemes: four palettes, picker bounds, two orientations, saved selection, Night toggle, old/corrupt preferences and unchanged ERS/arm state PASS");
return true;
