// Timers never write the control configuration, CUT latch or module link.
void TransmitterUi::loadTimerSettings(){
    Preferences p;if(!p.begin("r1-timers",true))return;
    for(unsigned slot=0;slot<kModelMemoryCount;++slot){char key[12];snprintf(key,sizeof(key),"model%u",slot);
        TimerSettings saved;
        if(p.getBytesLength(key)==sizeof(saved)&&p.getBytes(key,&saved,sizeof(saved))==sizeof(saved)&&validTimerSettings(saved))timers_[slot].settings=saved;
    }
    p.end();timerLastSavedOn_=timers_[activeModelSlot_].settings.onSeconds;
}
bool TransmitterUi::saveTimerSettings(){
    Preferences p;if(!p.begin("r1-timers",false)){snprintf(timerNotice_,sizeof(timerNotice_),"Timer settings save failed");return false;}
    char key[12];snprintf(key,sizeof(key),"model%u",activeModelSlot_);
    const auto &settings=timers_[activeModelSlot_].settings;
    const bool saved=p.putBytes(key,&settings,sizeof(settings))==sizeof(settings);
    if(saved)timerLastSavedOn_=settings.onSeconds;
    else snprintf(timerNotice_,sizeof(timerNotice_),"Timer settings save failed");
    p.end();return saved;
}
const char *TransmitterUi::timerPrimaryLabel()const{
    const auto &t=timers_[activeModelSlot_];
    return t.running()?(t.settings.mode==TimerMode::Lap?"Lap":"Pause"):(t.elapsed()?"Resume":"Start");
}
void TransmitterUi::timerAction(unsigned action){
    auto &t=timers_[activeModelSlot_];const auto now=millis();const auto lapsBefore=t.lapCount();timerNotice_[0]=0;
    if(action==0){if(!t.primary(now))snprintf(timerNotice_,sizeof(timerNotice_),t.full()?"100 laps recorded; pause to clear":"Lap ignored: minimum interval");}
    else if(action==1)t.toggle(now);
    else if(action==2)t.reset(now);
    else if(action==3){if(!t.setReady(now))snprintf(timerNotice_,sizeof(timerNotice_),"Enable throttle auto-start in Settings first");}
    else if(action==8){if(!t.running())t.resume(now);else if(!t.recordLap(now))snprintf(timerNotice_,sizeof(timerNotice_),"Lap ignored: minimum interval or memory full");}
    if(t.lapCount()>lapsBefore)playBuzzer(40);
    else if(timerNotice_[0])playBuzzer(80);
    if(activePage_==UiPage::TimerControl)drawDesignPage(UiPage::TimerControl);
    else if(activePage_==UiPage::Dashboard)drawDashboard();
}
void TransmitterUi::updateRaceTimer(uint32_t now){
    auto &t=timers_[activeModelSlot_];
    const uint32_t delta=timerClockSet_?uint32_t(now-timerTickMs_):now;timerTickMs_=now;timerClockSet_=true;
    const uint64_t on=uint64_t(timerOnRemainder_)+delta;
    const uint64_t total=uint64_t(t.settings.onSeconds)+on/1000;
    t.settings.onSeconds=total>UINT32_MAX?UINT32_MAX:uint32_t(total);timerOnRemainder_=on%1000;
    // Accumulated on-time is checkpointed once per minute, not on every frame.
    if(t.settings.onSeconds-timerLastSavedOn_>=60&&static_cast<int32_t>(now-timerNextSaveMs_)>=0){saveTimerSettings();timerNextSaveMs_=now+5000;}
    const auto &input=physicalInputStates_[static_cast<unsigned>(PhysicalControlId::ThrottleTrigger)];
    const bool valid=bootPinUnlocked_&&pinPurpose_==PinPurpose::None&&!hardwareRecovery_&&!calibrationActive()&&input.mapped&&!input.faulted;
    const bool before=t.running();t.tick(now,mapAdcToPermille(latestRawAdc_),valid);
    const auto alarm=t.takeAlarms();
    if(alarm&(RaceTimer::RaceEnd|RaceTimer::Target))
        snprintf(timerNotice_,sizeof(timerNotice_),alarm&RaceTimer::RaceEnd?"Race time reached":"Target lap time");
    if(!t.running()||(t.settings.mode==TimerMode::Down&&!t.settings.raceAlarm)||!systemSettings_.buzzerEnabled){
        if(timerBuzzer_.active(now))stopBuzzer();
    }else{
        if(alarm)timerBuzzer_.begin(alarm,t.settings.mode,now);
        if(const auto duration=timerBuzzer_.poll(now))emitBuzzerTone(duration);
    }
    if(before!=t.running()&&(activePage_==UiPage::TimerControl||activePage_==UiPage::Dashboard))redrawActivePage();
}
bool TransmitterUi::portalSetTimerSettings(const TimerSettings &settings){
    if(!SetupPortal::canControl()||armRequested_||!validTimerSettings(settings))return false;
    auto &t=timers_[activeModelSlot_];const auto on=t.settings.onSeconds;t.reset(millis());t.settings=settings;t.settings.onSeconds=on;
    const bool saved=saveTimerSettings();if(activePage_==UiPage::TimerControl)drawDesignPage(activePage_);return saved;
}
bool TransmitterUi::portalTimerAction(unsigned action){
    if(!SetupPortal::canControl()||armRequested_||action>8||action==4||action==6||action==7)return false;
    if(action==5){const bool ok=timers_[activeModelSlot_].clearLaps();if(ok&&activePage_==UiPage::TimerControl)drawDesignPage(activePage_);return ok;}
    timerAction(action);return true;
}
void TransmitterUi::drawTimerPage(){
    auto &t=timers_[activeModelSlot_];timerSummary_=nullptr;designValues_[0]=nullptr;
    auto text=[&](const char *value,int x,int y,UiText::Style size=UiText::Caption){return makeLabel(lv_scr_act(),value,x,y,size,cWhite);};
    auto button=[&](int x,int y,int w,const char *value,uint8_t id){return makePageActionButton(x,y,w,28,value,UiText::Secondary,cPanel,cCyan,cLine,&TransmitterUi::controlTimer,id);};
    char title[40];snprintf(title,sizeof(title),"Timer / %s%s",timerModeName(t.settings.mode),t.ready()?" / Ready":"");text(title,12,48,UiText::Heading);
    if(timerConfirmation_){
        text(timerConfirmation_==1?"Clear all lap records?":"Reset accumulated on-time?",12,108,UiText::Menu);
        text("The clock must be paused. This cannot be undone.",12,150);
        button(12,208,kScreenWidth/2-18,"Cancel",15);button(kScreenWidth/2+6,208,kScreenWidth/2-18,"Confirm",16);return;
    }
    const int tabWidth=(kScreenWidth-32)/3;
    const char *tabs[]={"Controls","Laps","Settings"};
    for(uint8_t n=0;n<3;++n){auto b=button(10+n*(tabWidth+6),80,tabWidth,tabs[n],10+n);
        if(timerBank_==n)lv_obj_set_style_border_color(b,colour(cCyan),0);}
    if(timerBank_==0){
        char value[32];formatTimer(value,sizeof(value),t.display(),true);
        designValues_[0]=text(value,12,112,UiText::Heading);
        timerSummary_=text("",12,148);lv_obj_set_width(timerSummary_,kScreenWidth-24);
        text(timerNotice_[0]?timerNotice_:t.ready()?"Ready: centre throttle, then pull forward":"Reset keeps recorded laps; clear them on Laps",12,190);
        const int w=(kScreenWidth-42)/4;
        button(12,217,w,timerPrimaryLabel(),0);auto pause=button(18+w,217,w,t.running()?"Pause":"Paused",1);
        if(!t.running())lv_obj_add_state(pause,LV_STATE_DISABLED);
        button(24+w*2,217,w,"Reset",2);button(30+w*3,217,w,"Ready",3);
    }else if(timerBank_==1){
        char value[96],total[24];formatTimer(total,sizeof(total),t.totalLaps(),true);
        snprintf(value,sizeof(value),"%u / 100 laps | Total %s",t.lapCount(),total);text(value,12,116);
        const unsigned pages=(t.lapCount()+4)/5;timerLapPage_=pages?min<unsigned>(timerLapPage_,pages-1):0;
        for(unsigned i=0;i<5&&timerLapPage_*5+i<t.lapCount();++i){unsigned index=timerLapPage_*5+i;char lap[24];formatTimer(lap,sizeof(lap),t.lap(index),true);
            snprintf(value,sizeof(value),"%02u   %s%s",index+1,lap,t.lap(index)==t.bestLap()?"  Best":"");text(value,18,139+i*20,UiText::Secondary);}
        if(!t.lapCount())text("No laps recorded this session",18,150,UiText::Secondary);
        button(12,246,52,"<",13);button(70,246,52,">",14);button(kScreenWidth-116,246,104,"Clear laps",17);
    }else{
        const int x=kScreenWidth-112;
        const char *names[3][3]={{"Mode","Race / countdown","Minimum lap"},{"Throttle auto-start","Target lap alarm","Race-end alarm"},{"Accumulated on-time","Reset on-time","Lap history is held in RAM"}};
        for(uint8_t n=0;n<3;++n){const int y=119+n*36;char value[64];
            if(timerSettingsPage_==0){if(n==0)snprintf(value,sizeof(value),"%s",timerModeName(t.settings.mode));else snprintf(value,sizeof(value),"%u s",n==1?t.settings.durationSeconds:t.settings.minimumLapSeconds);}
            else if(timerSettingsPage_==1){if(n==1)snprintf(value,sizeof(value),"%u s (0 = off)",t.settings.targetSeconds);else snprintf(value,sizeof(value),"%s",(n==0?t.settings.autoStart:t.settings.raceAlarm)?"On":"Off");}
            else{if(n==0)snprintf(value,sizeof(value),"%lu:%02lu:%02lu",(unsigned long)(t.settings.onSeconds/3600),(unsigned long)(t.settings.onSeconds/60%60),(unsigned long)(t.settings.onSeconds%60));else snprintf(value,sizeof(value),"%s",n==1?"Confirm":"Per-model, per power session");}
            text(names[timerSettingsPage_][n],12,y);text(value,12,y+15);
            if(timerSettingsPage_<2){button(x,y,46,"-",30+n*2);button(x+52,y,46,"+",31+n*2);}
            else if(n==1)button(x,y,98,"Reset",18);
        }
        button(12,246,52,"<",20);button(kScreenWidth-64,246,52,">",21);
        const int centre=kScreenWidth/2;for(uint8_t i=0;i<3;++i){auto dot=makePanel(centre-22+i*18,255,8,8,i==timerSettingsPage_?cCyan:cLine,cLine,4);(void)dot;addPageActionTarget(centre-28+i*18,246,18,28,&TransmitterUi::controlTimer,22+i);}
    }
}
void TransmitterUi::controlTimer(uint8_t item,PageControlEvent event){
    if(event!=PageControlEvent::Activate)return;
    auto &t=timers_[activeModelSlot_];
    if(item<=3){if(item==1&&!t.running())return;timerAction(item);return;}
    if(item>=10&&item<=12){timerBank_=item-10;timerConfirmation_=0;}
    else if(item==13&&timerLapPage_)--timerLapPage_;
    else if(item==14&&timerLapPage_<19)++timerLapPage_;
    else if(item==15)timerConfirmation_=0;
    else if(item==16){
        if(t.running()||t.ready()){snprintf(timerNotice_,sizeof(timerNotice_),"Pause before clearing records or on-time");timerBank_=0;}
        else if(timerConfirmation_==1)t.clearLaps();
        else if(timerConfirmation_==2){t.settings.onSeconds=0;timerOnRemainder_=0;saveTimerSettings();}
        timerConfirmation_=0;
    }else if(item==17)timerConfirmation_=1;
    else if(item==18)timerConfirmation_=2;
    else if(item>=20&&item<=24)timerSettingsPage_=item<22?(timerSettingsPage_+(item==20?2:1))%3:item-22;
    else if(item>=30&&item<=35){
        const int row=(item-30)/2,direction=item%2?1:-1;
        t.pause(millis());
        if(timerSettingsPage_==0){
            if(row==0){t.reset(millis());t.settings.mode=static_cast<TimerMode>((int(t.settings.mode)+direction+3)%3);}
            else if(row==1)t.settings.durationSeconds=constrain(int(t.settings.durationSeconds)+direction*30,1,5999);
            else t.settings.minimumLapSeconds=constrain(int(t.settings.minimumLapSeconds)+direction,1,60);
        }else if(timerSettingsPage_==1){
            if(row==0)t.settings.autoStart=!t.settings.autoStart;
            else if(row==1)t.settings.targetSeconds=constrain(int(t.settings.targetSeconds)+direction*5,0,5999);
            else t.settings.raceAlarm=!t.settings.raceAlarm;
        }
        saveTimerSettings();
    }
    drawDesignPage(UiPage::TimerControl);
}
