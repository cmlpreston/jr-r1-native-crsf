static const char *kWifiKeys[3][4]={{"1234567890","qwertyuiop","asdfghjkl","zxcvbnm-_"},{"1234567890","QWERTYUIOP","ASDFGHJKL","ZXCVBNM-_"},{"!@#$%^&*()","[]{}<>/=+",";:'\"\\|,.","`~?-_"}};
// Setup runs on the normal UI/Arduino loop; network callbacks never touch LVGL
// from another task. Input gestures are dispatched through existing hit targets.
void TransmitterUi::drawWifiPage()
{
    activePage_=UiPage::WifiSetup;startScreen();resetLvglObjects();beginPageFocus(false);
    const auto &s=SetupPortal::state();wifiRevision_=s.revision;
    makeHeader(wifiWebAccess_?"Web access":"Home Wi-Fi", (wifiEdit_==4||wifiEdit_==6)?"Cancel":wifiEdit_?"Done":"Close");
    auto text=[](const char *t,int y,int size=12){auto l=makeLabel(lv_scr_act(),t,12,y,size,cWhite);lv_obj_set_width(l,kScreenWidth-24);return l;};
    auto button=[&](int x,int y,int w,const char *label,uint8_t id){return makePageActionButton(x,y,w,32,label,12,cPanel,cWhite,cLine,&TransmitterUi::controlWifi,id);};
    if(wifiEdit_==1||wifiEdit_==2||wifiEdit_==4||wifiEdit_==6){
        text(wifiEdit_==6?(wifiSetupPasswordError_?s.message:"Hostname (without .local)"):wifiEdit_==1?"Network name (SSID)":wifiEdit_==4?(wifiSetupPasswordError_?s.message:"New setup password"):"Home Wi-Fi password",52,wifiSetupPasswordError_?12:16);
        if((wifiEdit_==4||wifiEdit_==6)&&!portraitUi)button(352,48,116,"Save",13);
        auto value=text(wifiEdit_==6?wifiHostname_:wifiEdit_==1?wifiSsid_:wifiEdit_==4?wifiSetupPassword_:wifiPassword_,78,14);
        lv_label_set_long_mode(value,LV_LABEL_LONG_WRAP);
        lv_obj_set_height(value,portraitUi?44:30);
        const int margin=4,gap=2,keyHeight=portraitUi?52:34;
        const int keyboardTop=portraitUi?130:110,rowPitch=keyHeight+4;
        const int available=kScreenWidth-2*margin;
        auto key=[&](int x,int y,int w,int h,const char *label,uint8_t id,int font){
            return makePageActionButton(x,y,w,h,label,font,cPanel,cWhite,cLine,&TransmitterUi::controlWifi,id);
        };
        for(int r=0;r<4;++r){
            const size_t count=strlen(kWifiKeys[wifiKeyMode_][r]);
            const int rowWidth=int(count)*available/10;
            const int left=(kScreenWidth-rowWidth)/2;
            for(size_t c=0;c<count;++c){
                char label[2]={kWifiKeys[wifiKeyMode_][r][c],0};
                const int x=left+int(c)*available/10;
                const int right=left+int(c+1)*available/10;
                key(x,keyboardTop+r*rowPitch,right-x-gap,keyHeight,label,20+r*10+c,20);
            }
        }
        const int actionsY=keyboardTop+4*rowPitch+4;
        const int w3=available/3,actionHeight=portraitUi?42:32;
        key(margin,actionsY,w3-gap,actionHeight,wifiKeyMode_==0?"ABC":wifiKeyMode_==1?"Symbols":"abc",10,16);
        key(margin+w3,actionsY,w3-gap,actionHeight,"Space",11,16);
        key(margin+w3*2,actionsY,w3-gap,actionHeight,"Delete",12,16);
        // Landscape finishes using the header action; portrait has a large Done key.
        if(portraitUi)key(margin,actionsY+actionHeight+10,available,40,wifiEdit_==6?"Save hostname":wifiEdit_==4?"Save password":"Done",13,18);
    }else if(wifiEdit_==5){
        text("Restart handset?",70,22);
        text("Saved models, maps and Wi-Fi are kept.",112,12);
        text("Outputs must remain CUT.",142,14);
        button(12,190,(kScreenWidth-28)/2,"Cancel",13);
        button(kScreenWidth/2,190,(kScreenWidth-28)/2,"Restart now",18);
    }else if(wifiEdit_==3){
        text(s.scanning?"Scanning...":"Choose a 2.4 GHz network",52,14);
        const int rows=portraitUi?8:4;
        for(int i=0;i<rows;++i){int n=wifiNetworkPage_*rows+i;if(n>=s.networkCount)break;button(12,80+i*36,kScreenWidth-24,s.networks[n].ssid,70+n);}
        button(12,portraitUi?382:232,(kScreenWidth-28)/2,"More",14);button(kScreenWidth/2,portraitUi?382:232,(kScreenWidth-28)/2,"Back",13);
    }else{
        text(s.message,52,12);
        char line[112];snprintf(line,sizeof(line),"Network: %s",s.ssid[0]?s.ssid:"not configured");text(line,82,14);
        if(wifiWebAccess_){
            snprintf(line,sizeof(line),"http://%s.local",s.hostname);text(line,118,18);
            snprintf(line,sizeof(line),"IP: %s",s.ip);text(line,150,14);
            text("Web / setup hotspot password",portraitUi?198:178,14);
            text(s.apPassword,portraitUi?226:200,strlen(s.apPassword)>12?20:24);
            text("Username: r1",portraitUi?274:236,14);
            button(12,portraitUi?328:274,(kScreenWidth-28)/2,"Password",15);
            button(kScreenWidth/2,portraitUi?328:274,(kScreenWidth-28)/2,"Hostname",19);
        }else{
            snprintf(line,sizeof(line),"IP: %s",s.ip);text(line,110,14);
            const int y=portraitUi?162:140,w=(kScreenWidth-28)/2;
            button(12,y,w,"Scan networks",3);button(kScreenWidth/2,y,w,"Enter SSID",4);
            button(12,y+40,w,"Home password",5);button(kScreenWidth/2,y+40,w,"Connect",6);
            button(12,y+80,w,"Forget Wi-Fi",7);button(kScreenWidth/2,y+80,w,s.autoConnect?"Auto-connect: ON":"Auto-connect: OFF",16);
            button(12,y+120,kScreenWidth-24,"Web access / credentials",80);
        }
    }
    finishPageFocus();
    if(wifiEdit_==1||wifiEdit_==2||wifiEdit_==4||wifiEdit_==6){
        wifiKeyPreview_=makeLabel(lv_scr_act(),"",0,0,32,cWhite);
        lv_obj_set_size(wifiKeyPreview_,52,54);
        lv_obj_set_style_text_align(wifiKeyPreview_,LV_TEXT_ALIGN_CENTER,0);
        lv_obj_set_style_pad_top(wifiKeyPreview_,8,0);
        lv_obj_set_style_bg_color(wifiKeyPreview_,lv_color_hex(cPanel),0);
        lv_obj_set_style_bg_opa(wifiKeyPreview_,LV_OPA_COVER,0);
        lv_obj_set_style_border_color(wifiKeyPreview_,lv_color_hex(cWhite),0);
        lv_obj_set_style_border_width(wifiKeyPreview_,2,0);
        lv_obj_add_flag(wifiKeyPreview_,LV_OBJ_FLAG_HIDDEN);
    }
}
void TransmitterUi::updateWifiKeyPreview(int16_t x,int16_t y)
{
    if(activePage_!=UiPage::WifiSetup||!wifiKeyPreview_)return;
    for(uint8_t i=0;i<pageFocusCount_;++i){
        const auto &target=pageFocusTargets_[i];
        const auto &r=target.activate;
        if(target.handler!=&TransmitterUi::controlWifi||target.argument<20||target.argument>=60)continue;
        if(x<r.x||x>=r.x+r.width||y<r.y||y>=r.y+r.height)continue;
        const unsigned row=(target.argument-20)/10,col=(target.argument-20)%10;
        if(col>=strlen(kWifiKeys[wifiKeyMode_][row]))break;
        char label[2]={kWifiKeys[wifiKeyMode_][row][col],0};
        lv_label_set_text(wifiKeyPreview_,label);
        const int left=max(2,min(int(kScreenWidth)-54,int(r.x+r.width/2-26)));
        lv_obj_set_pos(wifiKeyPreview_,left,r.y-58);
        lv_obj_clear_flag(wifiKeyPreview_,LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(wifiKeyPreview_,LV_OBJ_FLAG_HIDDEN);
}
void TransmitterUi::controlWifi(uint8_t item, PageControlEvent event)
{
    if(event!=PageControlEvent::Activate)return;
    Serial.printf("WIFI UI: control=%u\n",item);
    if(remoteTouchActive_){
        if(item==2){wifiEdit_=0;armRequested_=false;drawDashboard();return;}
        // A browser must not tear down its own transport. Network changes
        // remain available from the physical LCD, including password rotation.
        if(item==6||item==7||item==15||(item==13&&wifiEdit_==4)){
            remoteTouchRejected_=true;return;
        }
    }
    if(item==2){wifiPassword_[0]=0;armRequested_=false;safetyResetRequested_=true;SetupPortal::stop();drawDesignPage(UiPage::NetworkOptions);return;}
    if(item==80){wifiWebAccess_=true;wifiEdit_=0;drawWifiPage();return;}
    if(item==19){snprintf(wifiHostname_,sizeof(wifiHostname_),"%s",SetupPortal::state().hostname);wifiEdit_=6;wifiKeyMode_=0;wifiSetupPasswordError_=false;}
    if(item==17){wifiEdit_=5;drawWifiPage();return;}
    if(item==18){
        if(wifiEdit_!=5||armRequested_||!SetupPortal::requestRestart()){playBuzzer(180);return;}
        wifiEdit_=0;drawWifiPage();return;
    }
    if(item==16){if(!SetupPortal::setAutoConnect(!SetupPortal::state().autoConnect))playBuzzer(180);}
    if(item==3){SetupPortal::scan();wifiEdit_=3;wifiNetworkPage_=0;}
    if(item==4){wifiEdit_=1;wifiKeyMode_=0;}
    if(item==5){wifiEdit_=2;wifiKeyMode_=0;}
    if(item==6){if(!SetupPortal::connect(wifiSsid_,wifiPassword_)){playBuzzer(180);wifiEdit_=2;}}
    if(item==7){SetupPortal::forget();wifiSsid_[0]=wifiPassword_[0]=0;}
    if(item>=70&&item<78){const auto &s=SetupPortal::state();if(item-70<s.networkCount){snprintf(wifiSsid_,sizeof(wifiSsid_),"%s",s.networks[item-70].ssid);wifiPassword_[0]=0;wifiKeyMode_=0;wifiEdit_=s.networks[item-70].secure&&!SetupPortal::hasSavedNetwork(wifiSsid_)?2:0;}}
    if(item==14)wifiNetworkPage_=!wifiNetworkPage_;
    if(item==15){wifiEdit_=4;wifiKeyMode_=0;wifiSetupPassword_[0]=0;wifiSetupPasswordError_=false;}
    if(item==13){
        if(wifiEdit_==6&&!SetupPortal::setHostname(wifiHostname_)){wifiSetupPasswordError_=true;playBuzzer(180);drawWifiPage();return;}
        if(wifiEdit_==4){
            if(!SetupPortal::changePassword(wifiSetupPassword_)){wifiSetupPasswordError_=true;playBuzzer(180);drawWifiPage();return;}
            wifiSetupPassword_[0]=0;
        }
        wifiEdit_=0;
    }
    if(item==10)wifiKeyMode_=(wifiKeyMode_+1)%3;
    if((wifiEdit_==1||wifiEdit_==2||wifiEdit_==4||wifiEdit_==6)&&(item==11||item==12||(item>=20&&item<60))){
        char *value=wifiEdit_==6?wifiHostname_:wifiEdit_==1?wifiSsid_:wifiEdit_==4?wifiSetupPassword_:wifiPassword_;const size_t capacity=wifiEdit_==6?sizeof(wifiHostname_):wifiEdit_==1?sizeof(wifiSsid_):wifiEdit_==4?sizeof(wifiSetupPassword_):sizeof(wifiPassword_);size_t n=strlen(value);
        if(item==12){if(n)value[n-1]=0;}
        else {char c=' ';if(item>=20){unsigned r=(item-20)/10,col=(item-20)%10;if(col>=strlen(kWifiKeys[wifiKeyMode_][r]))return;c=kWifiKeys[wifiKeyMode_][r][col];}if(n+1<capacity){value[n]=c;value[n+1]=0;}}
    }
    drawWifiPage();
}
bool TransmitterUi::portalRenameModel(const char *name)
{
    if(!SetupPortal::canControl()||armRequested_||!name||!*name||strlen(name)>=kModelNameCapacity)return false;
    for(const char *c=name;*c;++c)if(*c<32||*c>126)return false;
    snprintf(modelNames_[activeModelSlot_],kModelNameCapacity,"%s",name);saveModelMemorySettings();
    Preferences p;if(!p.begin("jr-r1-models",true))return false;
    char names[kModelMemoryCount][kModelNameCapacity];const bool ok=p.getBytes("names",names,sizeof(names))==sizeof(names)&&memcmp(names,modelNames_,sizeof(names))==0;p.end();return ok;
}
bool TransmitterUi::portalSetChannel(uint8_t ch,const ChannelResponseProfile &profile,int trim,int subtrim)
{
    if(!SetupPortal::canControl()||armRequested_||ch<1||ch>16||trim<-200||trim>200||subtrim<-200||subtrim>200)return false;
    armRequested_=false;safetyResetRequested_=true;channelProfiles_[ch-1]=profile;sanitizeChannelResponseProfile(channelProfiles_[ch-1]);
    channelParameters_[ch-1].trimPermille=trim;channelParameters_[ch-1].subtrimPermille=subtrim;saveSettings();saveChannelParameters();
    Preferences p;char name[16];ChannelResponseProfile profiles[16];ChannelParameterValues values[16];
    if(!p.begin(modelNamespace(activeModelSlot_,"jr-r1-response","response",name,sizeof(name)),true))return false;
    bool ok=p.getBytes("profiles",profiles,sizeof(profiles))==sizeof(profiles)&&memcmp(profiles,channelProfiles_,sizeof(profiles))==0;p.end();
    if(!p.begin(modelNamespace(activeModelSlot_,"jr-r1-chparam","param",name,sizeof(name)),true))return false;
    ok=ok&&p.getBytes("values",values,sizeof(values))==sizeof(values)&&memcmp(values,channelParameters_,sizeof(values))==0;p.end();return ok;
}

ControlBinding TransmitterUi::portalBinding(uint8_t input) const
{
    const auto id=static_cast<PhysicalControlId>(input);
    const int index=bindingIndexForInput(id);
    return index>=0 ? bindings_[index] : defaultBindingForInput(id);
}
int TransmitterUi::portalFunction(unsigned index)
{
    return index<sizeof(kEditableTransmitterFunctions)/sizeof(kEditableTransmitterFunctions[0])
        ? static_cast<int>(kEditableTransmitterFunctions[index]) : -1;
}
bool TransmitterUi::portalSetBinding(uint8_t input,uint8_t target,uint16_t selection,
                                    uint8_t behaviour,uint8_t context,bool enabled,const ControlBinding *values)
{
    auto fail=[this](const char *why){snprintf(bindingError_,sizeof(bindingError_),"%s",why);return false;};
    if(!SetupPortal::canControl()||armRequested_||input<2||input>=kPhysicalControlCount)return fail("Invalid input or setup closed");
    if(context!=ContextAlways&&context!=ContextDashboard&&context!=ContextMenu&&context!=ContextCut&&context!=ContextArmed)return fail("Invalid context");
    const auto kind=physicalControlDescriptor(input).kind;
    ControlBinding b=portalBinding(input);
    const auto t=static_cast<AssignmentTarget>(target);
    const auto mode=static_cast<ControlBehaviour>(behaviour);
    if(t==AssignmentTarget::CrsfChannel){
        if(selection<1||selection>16)return fail("Invalid channel");
        bool compatible=kind==PhysicalControlKind::MaintainedSwitch ? mode==ControlBehaviour::TwoPosition
            : kind==PhysicalControlKind::SpringCentredTrim ? (mode==ControlBehaviour::SpringReturnThreePosition||mode==ControlBehaviour::Stepped||mode==ControlBehaviour::ThreePositionSelector)
            : (mode==ControlBehaviour::Momentary||mode==ControlBehaviour::Toggle||mode==ControlBehaviour::Pulse);
        if(!compatible)return fail("Behaviour does not match this control");
    }else if(t==AssignmentTarget::ModelParameter){
        if(kind!=PhysicalControlKind::SpringCentredTrim||!isChannelParameterTarget(selection)||channelParameterTargetChannel(selection)<1||channelParameterTargetChannel(selection)>16||static_cast<unsigned>(channelParameterTargetKind(selection))>1)return fail("Select a channel trim or sub-trim");
    }else if(t==AssignmentTarget::TransmitterFunction){
        bool found=false;for(auto f:kEditableTransmitterFunctions)if(static_cast<unsigned>(f)==selection)found=true;
        if(!found||kind==PhysicalControlKind::SpringCentredTrim)return fail("Unsupported function for this control");
    }else if(t==AssignmentTarget::DriveMode){
        if(kind!=PhysicalControlKind::MaintainedSwitch||(selection!=static_cast<unsigned>(DriveModeSelection::CutArm)&&selection!=static_cast<unsigned>(DriveModeSelection::WalkNormal)))return fail("Unsupported drive mode");
    }else return fail("Unsupported assignment");
    // Preserve advanced settings and trigger event when editing an existing
    // assignment. Reset target-specific values only when the target changes.
    bool changed=b.target!=t||(t==AssignmentTarget::ModelParameter&&b.targetId!=selection);
    if(changed){b=defaultBindingForInput(static_cast<PhysicalControlId>(input));b.minimum=-1000;b.maximum=1000;b.initial=b.failsafe=0;b.persistent=false;}
    b.target=t;b.enabled=enabled;b.contexts=context;b.crsfChannel=t==AssignmentTarget::CrsfChannel?selection:0;b.targetId=t==AssignmentTarget::CrsfChannel?0:selection;
    b.behaviour=mode;
    if(t==AssignmentTarget::ModelParameter){b.behaviour=ControlBehaviour::Stepped;if(changed){b.minimum=-200;b.maximum=200;b.step=10;}b.persistent=true;}
    if(t==AssignmentTarget::DriveMode||t==AssignmentTarget::TransmitterFunction){
        b.behaviour=kind==PhysicalControlKind::MaintainedSwitch?ControlBehaviour::TwoPosition:
            (selection==static_cast<unsigned>(TransmitterFunction::ArmCut)||selection==static_cast<unsigned>(TransmitterFunction::DisplayOnOff))?ControlBehaviour::Toggle:ControlBehaviour::Momentary;
        b.initial=b.failsafe=-1000;
    }
    if(values){
        if(t!=AssignmentTarget::CrsfChannel&&t!=AssignmentTarget::ModelParameter)return fail("This target has no editable range");
        b.minimum=values->minimum;b.maximum=values->maximum;b.step=values->step;
        b.initial=values->initial;b.failsafe=values->failsafe;b.inverted=values->inverted;
    }
    selectedBindingInput_=static_cast<PhysicalControlId>(input);bindingDraft_=b;
    if(!saveBindingDraft())return false;
    Preferences p;char name[16];ControlBinding check[kPhysicalControlCount];
    if(!p.begin(modelNamespace(activeModelSlot_,"jr-r1-bind","bind",name,sizeof(name)),true))return fail("Could not verify saved assignments");
    const size_t bytes=bindingCount_*sizeof(ControlBinding);
    bool ok=p.getUShort("version",0)==kBindingProfileVersion&&p.getUChar("count",255)==bindingCount_&&p.getBytes("items",check,bytes)==bytes&&memcmp(check,bindings_,bytes)==0;p.end();
    return ok?true:fail("Assignment active but storage failed; retry save");
}
bool TransmitterUi::portalSetRole(uint8_t input,uint8_t channel,bool enabled)
{
    if(!SetupPortal::canControl()||armRequested_||input>1||channel<1||channel>16)return false;
    const bool steering=input==0;
    if(channel==(steering?channelRoleSettings_.throttleChannel:channelRoleSettings_.steeringChannel)||bindingUsesChannel(channel,PhysicalControlId::Count))return false;
    if(steering){channelRoleSettings_.steeringChannel=channel;channelRoleSettings_.steeringEnabled=enabled;}
    else{channelRoleSettings_.throttleChannel=channel;channelRoleSettings_.throttleEnabled=enabled;}
    armRequested_=false;safetyResetRequested_=true;channelRolesChanged_=true;saveChannelRoleSettings();
    Preferences p;char name[16];if(!p.begin(modelNamespace(activeModelSlot_,"jr-r1-model1","roles",name,sizeof(name)),true))return false;
    bool ok=p.getUChar(steering?"steer_ch":"throt_ch",0)==channel&&p.getBool(steering?"steer_en":"throt_en",!enabled)==enabled;p.end();return ok;
}

// HTTP actions run on the UI loop. Remote arming needs a timed, page-bound hold.
bool TransmitterUi::portalEnableScreen() {
    if(!SetupPortal::state().running||!displayReady_)return false;
    lv_obj_invalidate(lv_scr_act());return true;
}
bool TransmitterUi::portalWakeScreen() {
    if(!SetupPortal::state().running||!displayReady_||!portalScreenNeedsWake())return false;
    remoteArmHold_.cancel();
    if(displaySleeping_)wakeDisplay();
    if(activePage_==UiPage::LogoSaver)leaveLogoSaver();
    lastInteractionMs_=millis();return true;
}
// Request CUT first; HTTP control remains blocked until the control loop
// publishes the disarmed snapshot and reports a safe state on its next tick.
bool TransmitterUi::portalRequestRemoteControl() {
    if(!SetupPortal::state().running||!displayReady_||touchWasDown_)return false;
    remoteArmHold_.cancel();armRequested_=false;safetyResetRequested_=true;
    lastInteractionMs_=millis();return true;
}
bool TransmitterUi::portalDriveHoldBounds(int &x,int &y,int &w,int &h) const {
    if(portalScreenNeedsWake())return false;
    if(!bootPinUnlocked_||pinPurpose_!=PinPurpose::None||hardwareRecovery_||rfBenchMode_||calibrationActive())return false;
    if(SetupPortal::active()&&(!SetupPortal::state().connected||!SetupPortal::state().autoConnect))return false;
    if(driveProgress_){x=0;y=kScreenHeight-(portraitUi?60:40);w=kScreenWidth/4;h=portraitUi?40:40;return true;}
    if(detailOutputLabel){x=portraitUi?148:246;y=5;w=portraitUi?50:86;h=34;return true;}
    if(activePage_==UiPage::OutputStatus){x=12;y=portraitUi?238:184;w=kScreenWidth-24;h=48;return true;}
    return false;
}

bool TransmitterUi::portalRemoteInput(int x,int y,int action,uint32_t hold,uint32_t pageEpoch) {
    if(action==5){if(hold!=remoteArmHold_.id())return false;remoteArmHold_.cancel();return true;}
    if(!SetupPortal::canControl()||armRequested_||touchWasDown_){remoteArmHold_.cancel();return false;}
    if(action==3||action==4||action==6){
        int left,top,width,height;
        if(!portalDriveHoldBounds(left,top,width,height)||x<left||x>=left+width||y<top||y>=top+height){remoteArmHold_.cancel();return false;}
        lastInteractionMs_=millis();
        if(action==3){if(SetupPortal::active())SetupPortal::stop();remoteArmPage_=static_cast<uint32_t>(activePage_);remoteArmHold_.begin(pageEpoch?pageEpoch:remoteArmPage_,millis());return true;}
        if(remoteArmPage_!=static_cast<uint32_t>(activePage_)){remoteArmHold_.cancel();return false;}
        if(action==6)return remoteArmHold_.heartbeat(hold,pageEpoch?pageEpoch:remoteArmPage_,millis());
        if(SetupPortal::active()||!remoteArmHold_.finish(hold,pageEpoch?pageEpoch:remoteArmPage_,millis()))return false;
        lastTouchDurationMs_=800;armGestureValid_=true;remoteTouchRejected_=false;
        remoteTouchActive_=remoteArmHoldDispatch_=true;handleTouch(x,y);
        remoteTouchActive_=remoteArmHoldDispatch_=false;armGestureValid_=false;lastTouchDurationMs_=0;
        return !remoteTouchRejected_&&armRequested_;
    }
    remoteArmHold_.cancel();
    // While locked, only the actual PIN keypad accepts remote taps.
    // Home/setup shortcuts cannot bypass the lock.
    if((!bootPinUnlocked_||pinPurpose_!=PinPurpose::None)&&(action!=0||activePage_!=UiPage::BootPin))return false;
    if(action==0&&(x<0||y<0||x>=kScreenWidth||y>=kScreenHeight))return false;
    armRequested_=false;lastInteractionMs_=millis();lastTouchDurationMs_=0;
    if(action==1){
        // Only leave the Wi-Fi landing page; don't discard an open editor.
        if(activePage_!=UiPage::WifiSetup||wifiEdit_)return false;
        drawDashboard();
    }else if(action==2){
        if(calibrationActive())return false;
        if(!SetupPortal::active())SetupPortal::start();
        wifiEdit_=0;drawWifiPage();
    }else if(action==0){
        remoteTouchRejected_=false;remoteTouchActive_=true;
        handleTouch(x,y);
        remoteTouchActive_=false;
        if(remoteTouchRejected_){armRequested_=false;return false;}
    }
    else return false;
    armRequested_=false;return true;
}

bool TransmitterUi::portalSetModelAppearance(uint8_t image,uint32_t colour,uint32_t accent,uint32_t base)
{
    if(!SetupPortal::canControl()||armRequested_||image>=kModelImageCount||!validModelColour(colour)||!validModelColour(accent)||!validModelColour(base))return false;
    modelImages_[activeModelSlot_]=image;modelColours_[activeModelSlot_]=colour;modelAccents_[activeModelSlot_]=accent;modelBases_[activeModelSlot_]=base;
    saveModelMemorySettings();
    Preferences p;if(!p.begin("jr-r1-models",true))return false;
    uint8_t icons[kModelMemoryCount];uint32_t colours[kModelMemoryCount];
    const bool ok=p.getBytes("images-v1",icons,sizeof(icons))==sizeof(icons)&&
        p.getBytes("colours-v1",colours,sizeof(colours))==sizeof(colours)&&
        memcmp(icons,modelImages_,sizeof(icons))==0&&memcmp(colours,modelColours_,sizeof(colours))==0;
    uint32_t a[kModelMemoryCount],b[kModelMemoryCount];
    const bool paintOk=p.getBytes("accents-v1",a,sizeof(a))==sizeof(a)&&p.getBytes("bases-v1",b,sizeof(b))==sizeof(b)&&memcmp(a,modelAccents_,sizeof(a))==0&&memcmp(b,modelBases_,sizeof(b))==0;
    p.end();redrawActivePage();return ok&&paintOk;
}
