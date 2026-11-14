// Remaining setup details share drawDesignPage's header/navigation and the
// same row/tab builders as the main setup menus, in either orientation.
void TransmitterUi::controlResponseTab(uint8_t tab,PageControlEvent event)
{
    if(event!=PageControlEvent::Activate||tab>2)return;
    responseCurveView_=tab==0;if(tab)portraitResponseBank_=tab-1;
    drawDesignPage(UiPage::ThrottleResponse);
}

void TransmitterUi::drawSetupDetailsContent()
{
    const int bottom=kScreenHeight-(portraitUi?60:40);
    auto title=[&](const char *text){auto l=makeLabel(lv_scr_act(),text,12,48,UiText::Heading,cWhite);lv_obj_set_width(l,kScreenWidth-24);lv_label_set_long_mode(l,LV_LABEL_LONG_CLIP);};
    auto note=[&](const char *text,int y,uint32_t ink=cDim){auto l=makeLabel(lv_scr_act(),text,12,y,UiText::Caption,ink);boundedLabel(l,kScreenWidth-24);return l;};
    auto action=[&](int x,int y,int w,const char *text,PageControlHandler handler,uint8_t id){return makePageActionButton(x,y,w,30,text,UiText::Secondary,cPanel2,cCyan,cLine,handler,id);};
    if(activePage_==UiPage::ChannelRoles){
        title("Channel roles");
        for(uint8_t i=0;i<2;++i){char value[12];snprintf(value,sizeof(value),"CH%u",i?channelRoleSettings_.throttleChannel:channelRoleSettings_.steeringChannel);
            drawSettingsRow(i?"Throttle":"Steering",value,86+i*(portraitUi?66:42),portraitUi?62:40,&TransmitterUi::controlChannelRoles,i,true);}
        drawSettingsRow("Steering input",channelRoleSettings_.steeringEnabled?"On":"Off / neutral",86+2*(portraitUi?66:42),portraitUi?62:40,&TransmitterUi::controlChannelRoles,2,false);
        note("Changes request CUT. Disabled steering holds neutral.",bottom-28,cAmber);
    }else if(activePage_==UiPage::Channel2){
        char heading[40];snprintf(heading,sizeof(heading),"Channel %u throttle",channelRoleSettings_.throttleChannel);title(heading);
        const char *names[]={"Brake / reverse endpoint","Response offset","Forward endpoint","Direction"};
        for(uint8_t i=0;i<4;++i){char value[24];
            if(i==0)snprintf(value,sizeof(value),"%u%%",throttleProfile().negativeEndpointPercent);
            if(i==1)snprintf(value,sizeof(value),"%+d%%",throttleProfile().subtrimPercent);
            if(i==2)snprintf(value,sizeof(value),"%u%%",throttleProfile().positiveEndpointPercent);
            if(i==3)snprintf(value,sizeof(value),"%s",throttleProfile().reversed?"Reversed":"Normal");
            channel2SettingValueLabels_[i]=drawSettingsRow(names[i],value,84+i*(portraitUi?62:34),portraitUi?60:32,&TransmitterUi::controlThrottleSetting,i,i!=3);
        }
        const int y=portraitUi?338:222;
        action(12,y,kScreenWidth/2-18,"Response",&TransmitterUi::controlThrottleHeader,0);
        action(kScreenWidth/2+6,y,kScreenWidth/2-18,"Calibrate",&TransmitterUi::controlThrottleHeader,1);
        note("Physical throttle input",bottom-26);
        channel2ValueLabel_=makeLabel(lv_scr_act(),"0%",kScreenWidth-64,bottom-26,UiText::Caption,cCyan);
        channel2LiveBar_=makeBar(lv_scr_act(),12,bottom-10,kScreenWidth-24,6,-1000,1000,cGreen);
    }else if(activePage_==UiPage::ThrottleResponse){
        char heading[40];snprintf(heading,sizeof(heading),"Channel %u response",selectedChannel_);title(heading);
        action(kScreenWidth-92,48,80,"Ramp",&TransmitterUi::controlThrottleHeader,0);
        const bool throttle=selectedChannel_==throttleChannel(),steering=selectedChannel_==steeringChannel();
        const char *tabs[]={"Curve",throttle?"Forward":steering?"Right":"Positive",throttle?"Brake":steering?"Left":"Negative"};const uint8_t ids[]={0,1,2};
        drawPageTabs(tabs,ids,3,responseCurveView_?0:portraitResponseBank_+1,&TransmitterUi::controlResponseTab,79);
        if(responseCurveView_){drawThrottleResponseGraph();return;}
        const auto &p=selectedResponseProfile();
        const int values[]={p.subtrimPercent,p.positiveEndpointPercent,p.positiveRatePercent,p.positiveExpoPercent,p.negativeEndpointPercent,p.negativeRatePercent,p.negativeExpoPercent};
        const char *names[]={"Response offset","Endpoint","Rate","Expo","Endpoint","Rate","Expo"};
        const uint8_t first=portraitResponseBank_?4:0,count=portraitResponseBank_?3:4;
        for(uint8_t i=0;i<count;++i){char value[24];snprintf(value,sizeof(value),(first+i==0||first+i==3||first+i==6)?"%+d%%":"%d%%",values[first+i]);
            drawSettingsRow(names[first+i],value,118+i*(portraitUi?62:34),portraitUi?60:32,&TransmitterUi::controlThrottleResponse,first+i,true);}
        note("Changes request CUT. Curve follows live physical input.",bottom-16);
    }else if(activePage_==UiPage::ChannelSpeed){
        char heading[40];snprintf(heading,sizeof(heading),"Channel %u ramp",selectedChannel_);title(heading);
        const auto &p=selectedResponseProfile();
        const bool throttle=selectedChannel_==throttleChannel(),steering=selectedChannel_==steeringChannel();
        const char *names[]={throttle?"Forward":steering?"Right":"Positive",throttle?"Brake / reverse":steering?"Left":"Negative"};
        for(uint8_t i=0;i<2;++i){const uint16_t ms=i?p.negativeRampMs:p.positiveRampMs;char text[24];if(ms)snprintf(text,sizeof(text),"%u ms",ms);else strcpy(text,"Direct");
            drawSettingsRow(names[i],text,86+i*(portraitUi?70:46),portraitUi?66:42,&TransmitterUi::controlChannelSpeed,i,true);}
        note("0 ms is direct. Ramp time runs from neutral to endpoint.",bottom-28);
    }else if(activePage_==UiPage::FirmwareInfo){
        title(JR_R1_TEMP_BENCH_STARTUP?"Firmware - temporary bench":"Firmware information");char revision[96],build[96],hardware[128];
        snprintf(revision,sizeof(revision),"%s @ %s [%s]",JR_R1_FIRMWARE_BRANCH,JR_R1_FIRMWARE_COMMIT,JR_R1_FIRMWARE_GIT_STATE);
        snprintf(build,sizeof(build),"%s %s | %s",JR_R1_FIRMWARE_BUILD_DATE,JR_R1_FIRMWARE_BUILD_TIME,JR_R1_FIRMWARE_BUILD_ENV);
#ifdef JR_R1_XIAO_PCB
        const char *board="Seeed XIAO ESP32-S3 / R1 PCB";
#elif defined(JR_R1_SIMULATOR)
        const char *board="Simulator";
#else
        const char *board="ESP32-S3 DevKitC";
#endif
        snprintf(hardware,sizeof(hardware),"%s | %s",board,DISPLAY_PROFILE_NAME);
        const char *names[]={"Firmware","Source revision","Build","Hardware"},*values[]={JR_R1_FIRMWARE_NAME,revision,build,hardware};
        for(uint8_t i=0;i<4;++i){lv_obj_t *detail=nullptr;
            drawMenuRow(names[i],"",86+i*(portraitUi?70:46),portraitUi?66:43,nullptr,0,false,10,values[i],&detail);
            if(portraitUi){lv_label_set_long_mode(detail,LV_LABEL_LONG_WRAP);lv_obj_set_pos(detail,10,27);lv_obj_set_height(detail,36);}
        }
    }else if(activePage_==UiPage::ElrsBinding){
        if(!elrsPhraseEditing_){
            title("ELRS binding");
            drawMenuRow("Binding phrase",elrsBindPhrase_[0]?elrsBindPhrase_:"Not set",84,portraitUi?66:40,&TransmitterUi::controlElrsBinding,46);
            const char *names[]={"Set transmitter phrase","Set receiver phrase","Read transmitter UID","Read receiver UID","Clear phrase entry"};
            for(uint8_t i=0;i<5;++i){const bool unavailable=(i==1||i==3)&&!elrsReceiverLinkAvailable_;
                auto value=drawMenuRow(names[i],unavailable?"No RX link":"",(portraitUi?166:132)+i*(portraitUi?48:28),portraitUi?44:26,&TransmitterUi::controlElrsBinding,i);
                if(unavailable)uiSetTextColor(value,colour(cDim),0);}
            note(elrsStatus_,portraitUi?422:282,cAmber);return;
        }
        title("Edit binding phrase");action(kScreenWidth-80,48,68,"Done",&TransmitterUi::controlElrsBinding,47);
        drawMenuRow("Phrase","",80,44,nullptr,0,false,10,elrsBindPhrase_[0]?elrsBindPhrase_:"Not set");
        const char *rows[]={"1234567890","qwertyuiop","asdfghjkl","zxcvbnm-_"};const uint8_t lengths[]={10,10,9,9};uint8_t key=5;
        for(uint8_t row=0;row<4;++row){const int width=(kScreenWidth-16)/10,start=(kScreenWidth-lengths[row]*width)/2;
            for(uint8_t col=0;col<lengths[row];++col){char text[]={rows[row][col],0};if(elrsKeyboardUppercase_&&text[0]>='a'&&text[0]<='z')text[0]-=32;
                makePageActionButton(start+col*width,(portraitUi?146:134)+row*(portraitUi?57:35),width-3,portraitUi?52:32,text,UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlElrsBinding,key++);}}
        const int y=portraitUi?386:280,w=(kScreenWidth-32)/3;
        action(8,y,w,elrsKeyboardUppercase_?"Lower":"Upper",&TransmitterUi::controlElrsBinding,43);
        action(16+w,y,w,"Space",&TransmitterUi::controlElrsBinding,44);
        action(24+w*2,y,w,LV_SYMBOL_BACKSPACE,&TransmitterUi::controlElrsBinding,45);
    }else if(activePage_==UiPage::EscProgramming){
        title("ESC programmer");note(escProgrammer_.status(),78,cAmber);
        const char *tabs[]={"Link","Drive","Brake","Motor","Limits"};const uint8_t ids[]={0,1,2,3,4};
        drawPageTabs(tabs,ids,5,escProgrammingTab_,&TransmitterUi::controlEscProgramming,96);
        const auto state=escProgrammer_.state();
        const bool editing=escEditingParameter_<15;
        if(escProgrammingTab_==0){
            const char *steps[]={
                "1. Tap Program in CUT; wait for the restart prompt.",
                "2. ESC OFF until RX LED dark; wait 5s; ON once.",
                "3. Read ESC; select a setting to review or edit.",
                "4. Tap Drive; restart again; wait for Drive confirmed."
            };
            const uint8_t step=state==EscDriveProgrammer::State::Returning||state==EscDriveProgrammer::State::Fault?3:
                state==EscDriveProgrammer::State::Ready||state==EscDriveProgrammer::State::Reading||state==EscDriveProgrammer::State::Writing?2:
                state==EscDriveProgrammer::State::AwaitProgramming&&(escProgrammer_.flags()&EscWire::Pending)?1:0;
            for(uint8_t i=0;i<4;++i){
                auto l=makeLabel(lv_scr_act(),steps[i],12,132+i*(portraitUi?54:25),UiText::Caption,i==step?cCyan:cDim);
                lv_obj_set_width(l,kScreenWidth-24);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);
            }
        }else if(editing){
            drawSettingsRow(Surpass::name(escEditingParameter_),Surpass::choice(escEditingParameter_,escDraftValue_),portraitUi?138:132,portraitUi?54:42,&TransmitterUi::controlEscProgramming,30,true);
            note("Apply sends one write, then checks the value.",portraitUi?244:180);
            const int w=(kScreenWidth-36)/2;
            action(12,bottom-78,w,"Apply one",&TransmitterUi::controlEscProgramming,31);
            action(24+w,bottom-78,w,"Cancel",&TransmitterUi::controlEscProgramming,32);
        }else{
            static const uint8_t groups[4][4]={{7,1,4,14},{2,3,8,12},{13,9,10,11},{0,5,6,255}};
            for(uint8_t n=0;n<4;++n){const uint8_t id=groups[min<uint8_t>(escProgrammingTab_-1,3)][n];if(id==255)break;
                drawMenuRow(Surpass::name(id),escProgrammer_.known(id)?Surpass::choice(id,escProgrammer_.value(id)):"--",128+n*(portraitUi?56:26),portraitUi?52:26,escProgrammer_.writable()&&escProgrammer_.known(id)?&TransmitterUi::controlEscProgramming:nullptr,40+id,false,10);
            }
        }
        // Session controls stay in one place on every tab and in the editor.
        makePanel(0,bottom-44,kScreenWidth,44,cHeader,cLine,0);
        const int w=(kScreenWidth-32)/3,y=bottom-38;
        const char *labels[]={"Program","Read ESC","Drive"};
        const bool enabled[]={state==EscDriveProgrammer::State::Idle,
            escProgrammer_.ready()&&!editing,
            escProgrammer_.blocksArm()&&state!=EscDriveProgrammer::State::Writing};
        for(uint8_t i=0;i<3;++i){
            auto b=action(8+i*(w+8),y,w,labels[i],&TransmitterUi::controlEscProgramming,11+i);
            if(!enabled[i]){lv_obj_add_state(b,LV_STATE_DISABLED);lv_obj_set_style_opa(b,LV_OPA_50,0);}
        }
    }else if(activePage_==UiPage::Calibration){
        title(calibratingSteering_?"Steering calibration":"Throttle calibration");
        const char *steps[]={calibratingSteering_?"1. Hold full left":"1. Hold full brake",calibratingSteering_?"2. Hold full right":"2. Hold full forward","3. Release and hold centre"};
        for(uint8_t i=0;i<3;++i)note(steps[i],82+i*(portraitUi?30:18),cCyan);
        char held[64];snprintf(held,sizeof(held),"CH%u held at neutral during calibration",calibratingSteering_?steeringChannel():throttleChannel());note(held,portraitUi?190:142,cAmber);
        const char *names[]={"Minimum average","Live input","Maximum average"};lv_obj_t **labels[]={&calibrationMinimumLabel_,&calibrationLiveLabel_,&calibrationMaximumLabel_};
        for(uint8_t i=0;i<3;++i)*labels[i]=drawMenuRow(names[i],"----",(portraitUi?226:166)+i*(portraitUi?58:32),portraitUi?54:30,nullptr,0,false,10);
        calibrationStatusLabel_=note("",portraitUi?410:270,cCyan);lv_label_set_long_mode(calibrationStatusLabel_,LV_LABEL_LONG_WRAP);
        drawCalibrationValues(calibratingSteering_?latestSteeringRawAdc_:latestRawAdc_);
    }
}
