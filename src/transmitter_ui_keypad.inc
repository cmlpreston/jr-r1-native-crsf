// Reserved handset navigation keys. Physical and simulator presses share this
// state machine; binding assignments cannot turn navigation into RF commands.
bool TransmitterUi::handleKeypadEvent(PhysicalControlId key,InputEvent event,uint32_t now)
{
    const int index=int(key)-int(PhysicalControlId::KeyMode);
    if(index<0||index>=8)return false;
    auto &press=keypadPresses_[index];
    if(event==InputEvent::InvalidState){press=KeypadPress{};return true;}
    if(event==InputEvent::Pressed){
        if(press.down)return true;
        press=KeypadPress{};press.down=true;press.started=now;press.repeatAt=now+450;press.page=activePage_;
        lastInteractionMs_=now;
        if(displaySleeping_||activePage_==UiPage::LogoSaver){
            if(displaySleeping_)wakeDisplay();
            if(activePage_==UiPage::LogoSaver)leaveLogoSaver();
            press.consumed=true;return true;
        }
        if(key!=PhysicalControlId::KeyMode&&key!=PhysicalControlId::KeyPage&&key!=PhysicalControlId::KeyClear)
            keypadAction(key,false);
    }else if(event==InputEvent::Released){
        const auto saved=press;press=KeypadPress{};
        if(!saved.down||saved.consumed||saved.held||saved.page!=activePage_)return true;
        if(key==PhysicalControlId::KeyMode||key==PhysicalControlId::KeyPage||key==PhysicalControlId::KeyClear)
            keypadAction(key,uint32_t(now-saved.started)>=650);
    }
    // The input processor's delayed short/double/long/repeat events are
    // consumed: updateKeypad owns timing, including simulator holds.
    return true;
}

void TransmitterUi::updateKeypad(uint32_t now)
{
    for(int i=0;i<8;++i){auto &p=keypadPresses_[i];
        if(!p.down||p.consumed||p.page!=activePage_)continue;
        const auto key=static_cast<PhysicalControlId>(int(PhysicalControlId::KeyMode)+i);
        if(key==PhysicalControlId::KeyMode||key==PhysicalControlId::KeyPage||key==PhysicalControlId::KeyClear){
            if(!p.held&&uint32_t(now-p.started)>=650){p.held=true;keypadAction(key,true);}
        }else if(key!=PhysicalControlId::KeySelect&&static_cast<int32_t>(now-p.repeatAt)>=0){
            p.repeatAt=now+100;
            const int repeats=uint32_t(now-p.started)>=2000?3:1;
            for(int n=0;n<repeats&&p.page==activePage_;++n)keypadAction(key,false,true);
        }
    }
}

void TransmitterUi::focusEditorExit()
{
    // Keep a draft intact; make its existing Apply/Cancel controls discoverable.
    pageFocusVisible_=true;
    for(uint8_t i=0;i<pageFocusCount_;++i){const auto &t=pageFocusTargets_[i];
        if((activePage_==UiPage::ValueEditor&&t.handler==&TransmitterUi::controlDesign&&t.argument==34)||
           (activePage_==UiPage::BindingEditor&&t.handler==&TransmitterUi::controlBindingEditor&&t.argument==5)){
            pageFocusIndex_=i;break;
        }
    }
    refreshPageFocus();playBuzzer(35);
}

void TransmitterUi::keypadPage(int direction)
{
    int page=0,count=0;
    switch(activePage_){
        case UiPage::Tune:page=tuneBank_;count=2;break;
        case UiPage::Telemetry:page=sensorBank_;count=(DashboardSensorCount+5)/6;break;
        case UiPage::ChannelMonitor:page=designBank_;count=4;break;
        case UiPage::MainMenu:page=setupPage_;count=2;break;
        case UiPage::InputMonitor:page=inputMonitorBank_;count=(kPhysicalControlCount+4)/5;break;
        case UiPage::Channels:designBank_=constrain(int(designBank_)+direction,0,3);drawChannelsPage();return;
        case UiPage::ModelSelect:
            if(!modelAppearanceEditing_){modelSelectBank_=constrain(int(modelSelectBank_)+direction,0,int((kModelMemoryCount+4)/5)-1);drawModelSelectPage();}return;
        case UiPage::ControlAssignments:controlAssignments(direction<0?5:8,PageControlEvent::Activate);return;
        case UiPage::DisplaySetup:displaySetupBank_=constrain(int(displaySetupBank_)+direction,0,1);drawDisplayPage();return;
        case UiPage::ResponseProfile:responseDetailBank_=constrain(int(responseDetailBank_)+direction,0,1);drawDesignPage(activePage_);return;
        case UiPage::BindingEditor:controlBindingEditor(constrain(int(bindingEditorBank_)+direction,0,3)+8,PageControlEvent::Activate);return;
        default:playBuzzer(35);return;
    }
    const int next=constrain(page+direction,0,count-1);
    if(next!=page)controlPageDot(next,PageControlEvent::Activate);
}

void TransmitterUi::keypadTab(int direction)
{
    if(activePage_==UiPage::Tune){tuneRole_=constrain(int(tuneRole_)+direction,tuneRole_<3?0:3,tuneRole_<3?2:4);drawDesignPage(UiPage::Tune);}
    else if(activePage_==UiPage::MainMenu)controlDesign(direction<0?140:141,PageControlEvent::Activate);
    else if(activePage_==UiPage::Telemetry||activePage_==UiPage::Gps||activePage_==UiPage::ChannelMonitor||
            (activePage_==UiPage::InputMonitor&&inputMonitorReturnPage_==UiPage::Telemetry)){
        const int tab=activePage_==UiPage::Telemetry?0:activePage_==UiPage::Gps?1:activePage_==UiPage::ChannelMonitor?2:3;
        const uint8_t actions[]={60,61,62,223};controlDesign(actions[constrain(tab+direction,0,3)],PageControlEvent::Activate);
    }else playBuzzer(35);
}

void TransmitterUi::keypadAction(PhysicalControlId key,bool held,bool repeat)
{
    lastInteractionMs_=millis();
    const auto activate=PageControlEvent::Activate;
    if(!bootPinUnlocked_||pinPurpose_!=PinPurpose::None){
        if(activePage_!=UiPage::BootPin)return;
        if(key==PhysicalControlId::KeyUp)movePageFocus(-1);
        else if(key==PhysicalControlId::KeyDown)movePageFocus(1);
        else if(key==PhysicalControlId::KeySelect&&!repeat)activatePageFocus();
        else if(key==PhysicalControlId::KeyClear&&!held){
            if(pinRecoveryHelp_)controlBootPin(14,activate);
            else controlBootPin(pinPurpose_==PinPurpose::Boot?10:12,activate);
        }
        return;
    }
    if(activePage_==UiPage::QuickNavigation){
        if(held&&key==PhysicalControlId::KeyMode){drawDashboard();return;}
        if(!repeat)controlKeypadMenu(key==PhysicalControlId::KeyUp?0:key==PhysicalControlId::KeyPage?1:
            key==PhysicalControlId::KeySelect?2:key==PhysicalControlId::KeyDown?3:key==PhysicalControlId::KeyPlus?4:
            key==PhysicalControlId::KeyMinus?5:key==PhysicalControlId::KeyClear?6:7,activate);
        return;
    }
    if(key==PhysicalControlId::KeyMode){
        if(navigationIsModal()||calibrationActive()){focusEditorExit();return;}
        if(held)drawDashboard();
        else{keypadReturnPage_=activePage_;drawDesignPage(UiPage::QuickNavigation);}
    }else if(key==PhysicalControlId::KeyPage)keypadPage(held?-1:1);
    else if(key==PhysicalControlId::KeyClear){
        if(held){if(activePage_==UiPage::ValueEditor)drawDesignPage(UiPage::FieldResetConfirmation);else playBuzzer(35);}
        else navigateBack();
    }else if(key==PhysicalControlId::KeyUp)movePageFocus(-1);
    else if(key==PhysicalControlId::KeyDown)movePageFocus(1);
    else if(key==PhysicalControlId::KeySelect&&!repeat)activatePageFocus();
    else if(key==PhysicalControlId::KeyPlus||key==PhysicalControlId::KeyMinus){
        const int direction=key==PhysicalControlId::KeyPlus?1:-1;
        if(activePage_==UiPage::ValueEditor)controlDesign(direction<0?30:31,activate);
        else if(navigationIsModal()||activePage_==UiPage::Channel2||activePage_==UiPage::ThrottleResponse||activePage_==UiPage::ChannelSpeed)
            adjustPageFocus(direction);
        else if(!repeat)keypadTab(direction);
    }
}

void TransmitterUi::drawKeypadPage()
{
    if(activePage_==UiPage::TimerControl){
        makeLabel(lv_scr_act(),"Timer",12,48,UiText::Heading,cWhite);
        designValues_[0]=makeLabel(lv_scr_act(),"00:00",12,100,UiText::Hero,cWhite);
        makePageActionButton(12,portraitUi?220:180,kScreenWidth/2-18,42,runTimerRunning_?"Stop":"Start",UiText::Menu,cPanel2,cWhite,cLine,&TransmitterUi::controlKeypadMenu,9);
        makePageActionButton(kScreenWidth/2+6,portraitUi?220:180,kScreenWidth/2-18,42,"Reset",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlKeypadMenu,10);return;
    }
    if(activePage_==UiPage::FieldResetConfirmation){
        makeLabel(lv_scr_act(),"Reset this field?",12,58,UiText::Heading,cWhite);
        auto hint=makeLabel(lv_scr_act(),"Only the draft changes. Apply is still required to save.",12,100,UiText::Secondary,cDim);
        lv_obj_set_width(hint,kScreenWidth-24);lv_label_set_long_mode(hint,LV_LABEL_LONG_WRAP);
        makePageActionButton(12,portraitUi?220:164,kScreenWidth/2-18,42,"Cancel",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlKeypadMenu,0);
        makePageActionButton(kScreenWidth/2+6,portraitUi?220:164,kScreenWidth/2-18,42,"Reset draft",UiText::Menu,cPanel2,cWhite,cLine,&TransmitterUi::controlKeypadMenu,1);
        return;
    }
    makePageActionButton(8,5,76,34,"Back",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlKeypadMenu,8);
    makeLabel(lv_scr_act(),"Quick menu",12,48,UiText::Heading,cWhite);
    const char *labels[]={"UP: Steering expo","PAGE: Endpoints","SEL: Throttle","DOWN: Trims","+: Models","-: Timer","CLR: Drive","MODE: Setup"};
    const int w=(kScreenWidth-30)/2,h=portraitUi?58:36,step=portraitUi?72:42;
    for(uint8_t i=0;i<8;++i)makePageActionButton(10+(i%2)*(w+10),86+(i/2)*step,w,h,labels[i],UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlKeypadMenu,i);
}

void TransmitterUi::controlKeypadMenu(uint8_t item,PageControlEvent event)
{
    if(event!=PageControlEvent::Activate)return;
    if(activePage_==UiPage::TimerControl){
        if(item==9)toggleRunTimer();
        else if(item==10){runTimerElapsedMs_=0;runTimerStartedMs_=millis();}
        drawDesignPage(UiPage::TimerControl);return;
    }
    if(activePage_==UiPage::FieldResetConfirmation){
        if(item==1){editorDraft_=(editorField_==0||editorField_==2)?100:0;
            if(editorField_==2&&tuneRole_==0)steeringEndpointDrafts_[steeringEndpointSide_]=editorDraft_;
        }
        drawDesignPage(UiPage::ValueEditor);return;
    }
    if(item==8){drawDesignPage(keypadReturnPage_);return;}
    if(item==0||item==1){tuneRole_=0;tuneBank_=0;selectedChannel_=steeringChannel();openDesignEditor(item==0?1:2,UiPage::Tune);}
    else if(item==2){tuneRole_=1;tuneBank_=0;selectedChannel_=throttleChannel();drawDesignPage(UiPage::Tune);}
    else if(item==3){tuneRole_=0;tuneBank_=1;drawDesignPage(UiPage::Tune);}
    else if(item==4)drawModelSelectPage();
    else if(item==5)drawDesignPage(UiPage::TimerControl);
    else if(item==6)drawDashboard();
    else if(item==7)drawMainMenu();
}
