// Remote actions share the model/controller and CUT policy with local edits.
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
    requestConfigurationCut();channelModel_.setResponse(ch,profile);
    channelModel_.parameters[ch-1].trimPermille=trim;channelModel_.parameters[ch-1].subtrimPermille=subtrim;
    return persistChannelModel();
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
    requestConfigurationCut();channelRolesChanged_=true;saveChannelRoleSettings();
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
    remoteArmHold_.cancel();requestConfigurationCut();
    lastInteractionMs_=millis();return true;
}
bool TransmitterUi::portalDriveHoldBounds(int &x,int &y,int &w,int &h) const {
    if(portalScreenNeedsWake() || (moduleLinkManaged_&&!moduleLink_.enabled()))return false;
    if(!bootPinUnlocked_||pinPurpose_!=PinPurpose::None||hardwareRecovery_||rfBenchMode_||calibrationActive())return false;
    if(SetupPortal::active()&&(!SetupPortal::state().connected||!SetupPortal::state().autoConnect))return false;
    if(driveProgress_){x=0;y=kScreenHeight-(portraitUi?60:40);w=kScreenWidth/4;h=portraitUi?40:40;return true;}
    if(activePage_==UiPage::OutputStatus){x=12;y=portraitUi?238:184;w=kScreenWidth-24;h=48;return true;}
    return false;
}

bool TransmitterUi::portalPageSwipe(int x,int y,int endX,int endY,uint32_t duration)
{
    if(!SetupPortal::canControl()||armRequested_||touchWasDown_||!bootPinUnlocked_||
       pinPurpose_!=PinPurpose::None||x<0||endX<0||y<0||endY<0||
       x>=kScreenWidth||endX>=kScreenWidth||y>=kScreenHeight||endY>=kScreenHeight)return false;
    remoteArmHold_.cancel();lastInteractionMs_=millis();
    return handlePageSwipe(x,y,endX,endY,duration);
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
