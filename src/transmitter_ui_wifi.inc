static const char *kWifiKeys[3][4]={{"1234567890","qwertyuiop","asdfghjkl","zxcvbnm-_"},{"1234567890","QWERTYUIOP","ASDFGHJKL","ZXCVBNM-_"},{"!@#$%^&*()","[]{}<>/=+",";:'\"\\|,.","`~?-_"}};
// Setup runs on the normal UI/Arduino loop; network callbacks never touch LVGL
// from another task. Input gestures are dispatched through existing hit targets.
void TransmitterUi::drawWifiPage()
{
    activePage_=UiPage::WifiSetup;startScreen();resetLvglObjects();beginPageFocus(false);
    const auto &s=SetupPortal::state();wifiRevision_=s.revision;
    makeHeader(wifiWebAccess_?"Web access":"Home Wi-Fi", (wifiEdit_==4||wifiEdit_==6)?"Cancel":wifiEdit_?"Done":"Close");
    if(!wifiEdit_ && SetupPortal::active()){
        // Navigation is independent of ending a manual setup session. Keep an
        // explicit finish action so physical arming is not trapped in setup.
        lv_obj_set_width(lv_obj_get_child(lv_obj_get_child(lv_scr_act(),0),0),kScreenWidth-250);
        makePageActionButton(kScreenWidth-146,5,106,34,"Finish setup",UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlWifi,2);
    }
    auto text=[](const char *t,int y,UiText::Style style=UiText::Caption){auto l=makeLabel(lv_scr_act(),t,12,y,style,cWhite);lv_obj_set_width(l,kScreenWidth-24);return l;};
    auto button=[&](int x,int y,int w,const char *label,uint8_t id){return makePageActionButton(x,y,w,32,label,UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlWifi,id);};
    if(wifiEdit_==1||wifiEdit_==2||wifiEdit_==4||wifiEdit_==6){
        text(wifiEdit_==6?(wifiSetupPasswordError_?s.message:"Hostname (without .local)"):wifiEdit_==1?"Network name (SSID)":wifiEdit_==4?(wifiSetupPasswordError_?s.message:"New setup password"):"Home Wi-Fi password",52,wifiSetupPasswordError_?UiText::Caption:UiText::Menu);
        if((wifiEdit_==4||wifiEdit_==6)&&!portraitUi)button(352,48,116,"Save",13);
        auto value=text(wifiEdit_==6?wifiHostname_:wifiEdit_==1?wifiSsid_:wifiEdit_==4?wifiSetupPassword_:wifiPassword_,78,UiText::Secondary);
        lv_label_set_long_mode(value,LV_LABEL_LONG_WRAP);
        lv_obj_set_height(value,portraitUi?44:30);
        const int margin=4,gap=2,keyHeight=portraitUi?52:34;
        const int keyboardTop=portraitUi?130:110,rowPitch=keyHeight+4;
        const int available=kScreenWidth-2*margin;
        auto key=[&](int x,int y,int w,int h,const char *label,uint8_t id,UiText::Style style){
            return makePageActionButton(x,y,w,h,label,style,cPanel,cWhite,cLine,&TransmitterUi::controlWifi,id);
        };
        for(int r=0;r<4;++r){
            const size_t count=strlen(kWifiKeys[wifiKeyMode_][r]);
            const int rowWidth=int(count)*available/10;
            const int left=(kScreenWidth-rowWidth)/2;
            for(size_t c=0;c<count;++c){
                char label[2]={kWifiKeys[wifiKeyMode_][r][c],0};
                const int x=left+int(c)*available/10;
                const int right=left+int(c+1)*available/10;
                key(x,keyboardTop+r*rowPitch,right-x-gap,keyHeight,label,20+r*10+c,UiText::Heading);
            }
        }
        const int actionsY=keyboardTop+4*rowPitch+4;
        const int w3=available/3,actionHeight=portraitUi?42:32;
        key(margin,actionsY,w3-gap,actionHeight,wifiKeyMode_==0?"ABC":wifiKeyMode_==1?"Symbols":"abc",10,UiText::Menu);
        key(margin+w3,actionsY,w3-gap,actionHeight,"Space",11,UiText::Menu);
        key(margin+w3*2,actionsY,w3-gap,actionHeight,"Delete",12,UiText::Menu);
        // Landscape finishes using the header action; portrait has a large Done key.
        if(portraitUi)key(margin,actionsY+actionHeight+10,available,40,wifiEdit_==6?"Save hostname":wifiEdit_==4?"Save password":"Done",13,UiText::Menu);
    }else if(wifiEdit_==5){
        text("Restart handset?",70,UiText::Heading);
        text("Saved models, maps and Wi-Fi are kept.",112,UiText::Caption);
        text("Outputs must remain CUT.",142,UiText::Secondary);
        button(12,190,(kScreenWidth-28)/2,"Cancel",13);
        button(kScreenWidth/2,190,(kScreenWidth-28)/2,"Restart now",18);
    }else if(wifiEdit_==3){
        text(s.scanning?"Scanning...":"Choose a 2.4 GHz network",52,UiText::Secondary);
        const int rows=portraitUi?8:4;
        for(int i=0;i<rows;++i){int n=wifiNetworkPage_*rows+i;if(n>=s.networkCount)break;button(12,80+i*36,kScreenWidth-24,s.networks[n].ssid,70+n);}
        button(12,portraitUi?382:232,(kScreenWidth-28)/2,"More",14);button(kScreenWidth/2,portraitUi?382:232,(kScreenWidth-28)/2,"Back",13);
    }else{
        text(s.message,52,UiText::Caption);
        char line[112];snprintf(line,sizeof(line),"Network: %s",s.ssid[0]?s.ssid:"not configured");text(line,82,UiText::Secondary);
        if(wifiWebAccess_){
            snprintf(line,sizeof(line),"http://%s.local",s.hostname);text(line,118,UiText::Menu);
            snprintf(line,sizeof(line),"IP: %s",s.ip);text(line,150,UiText::Secondary);
            text("Web / setup hotspot password",portraitUi?198:178,UiText::Secondary);
            text(s.apPassword,portraitUi?226:200,strlen(s.apPassword)>12?UiText::Heading:UiText::Heading);
            text("Username: r1",portraitUi?274:236,UiText::Secondary);
            button(12,portraitUi?328:274,(kScreenWidth-28)/2,"Password",15);
            button(kScreenWidth/2,portraitUi?328:274,(kScreenWidth-28)/2,"Hostname",19);
        }else{
            snprintf(line,sizeof(line),"IP: %s",s.ip);text(line,110,UiText::Secondary);
            const int y=portraitUi?162:140,w=(kScreenWidth-28)/2;
            button(12,y,w,"Scan networks",3);button(kScreenWidth/2,y,w,"Enter SSID",4);
            button(12,y+40,w,"Home password",5);button(kScreenWidth/2,y+40,w,"Connect",6);
            button(12,y+80,w,"Forget Wi-Fi",7);button(kScreenWidth/2,y+80,w,s.autoConnect?"Auto-connect: ON":"Auto-connect: OFF",16);
            button(12,y+120,kScreenWidth-24,"Web access / credentials",80);
        }
    }
    finishPageFocus();
    if(wifiEdit_==1||wifiEdit_==2||wifiEdit_==4||wifiEdit_==6){
        wifiKeyPreview_=makeLabel(lv_scr_act(),"",0,0,UiText::Reading,cWhite);
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
    if(item==2){wifiPassword_[0]=0;requestConfigurationCut();SetupPortal::stop();drawDesignPage(UiPage::NetworkOptions);return;}
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
