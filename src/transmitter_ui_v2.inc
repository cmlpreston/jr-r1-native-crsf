// Native V2 presentation. Uses existing telemetry, settings and safety paths;
// no parallel control/mixing pipeline or changes to V1 persistence formats.
namespace {
const char *const v2ShortcutNames[]={"Empty","GPS speed","Motor RPM","Drive pack","Lowest cell","GPS map","Steering trim","Live mixing","Run review","Timer","Control map","RF warnings","Models","Widgets"};
constexpr uint8_t v2ShortcutCount=sizeof(v2ShortcutNames)/sizeof(*v2ShortcutNames);
}

void TransmitterUi::loadV2Quick()
{
    if(v2QuickModel_==activeModelSlot_)return;
    const uint8_t defaults[]={1,3,6,7,8,5,9,10};Preferences p;char ns[16];
    p.begin(modelNamespace(activeModelSlot_,"r1-v2-quick","v2q",ns,sizeof(ns)),true);
    for(unsigned i=0;i<8;++i){char key[8];snprintf(key,sizeof(key),"slot%u",i);v2QuickSlots_[i]=p.getUChar(key,defaults[i]);if(v2QuickSlots_[i]>=v2ShortcutCount)v2QuickSlots_[i]=defaults[i];}
    p.end();v2QuickModel_=activeModelSlot_;
}
void TransmitterUi::saveV2Quick()
{
    Preferences p;char ns[16];p.begin(modelNamespace(activeModelSlot_,"r1-v2-quick","v2q",ns,sizeof(ns)),false);
    for(unsigned i=0;i<8;++i){char key[8];snprintf(key,sizeof(key),"slot%u",i);p.putUChar(key,v2QuickSlots_[i]);}p.end();
}
void TransmitterUi::drawV2Quick()
{
    loadV2Quick();
    makePageActionButton(kHeaderActionX,headerCenterY(kHeaderButtonHeight),kHeaderActionWidth,kHeaderButtonHeight,
        "Back",UiText::Secondary,cPanel,cWhite,cLine,&TransmitterUi::controlKeypadMenu,8);
    makeLabel(lv_scr_act(),"Quick menu",12,49,UiText::Heading,cWhite);
    makePageActionButton(kScreenWidth-85,47,73,30,v2QuickEditing_?"Done":"Edit",UiText::Caption,cPanel2,cCyan,cLine,&TransmitterUi::controlV2,4);
    const int w=(kScreenWidth-30)/2,step=portraitUi?65:39,h=portraitUi?56:35;
    const char *keys[]={"UP","PAGE","SEL","DOWN","+","-","CLR","MODE"};
    for(uint8_t i=0;i<8;++i){
        const int x=10+(i%2)*(w+10),y=86+(i/2)*step;char text[48];
        snprintf(text,sizeof(text),"%s: %s",keys[i],v2ShortcutNames[v2QuickSlots_[i]]);
        makePageActionButton(x,y,w-(v2QuickEditing_?35:0),h,text,UiText::Caption,cPanel,cWhite,cLine,&TransmitterUi::controlKeypadMenu,i);
        if(v2QuickEditing_)makePageActionButton(x+w-31,y,31,h,"^",UiText::Caption,cPanel2,cCyan,cLine,&TransmitterUi::controlV2,60+i);
    }
    makeLabel(lv_scr_act(),v2QuickEditing_?"Tap slot: next page | ^: move earlier | Empty: remove":"Per-model shortcuts | MODE key opens this menu",12,portraitUi?358:250,UiText::Caption,cDim);
}
void TransmitterUi::openV2Shortcut(uint8_t id)
{
    if(id>=1&&id<=4){const uint8_t source[]={0,2,6,7};sensorSelected_=source[id-1];v2GraphPaused_=false;drawDesignPage(UiPage::SensorDetail);}
    else if(id==5)drawDesignPage(UiPage::Gps);
    else if(id==6){tuneRole_=0;tuneBank_=1;selectedChannel_=steeringChannel();drawDesignPage(UiPage::Tune);}
    else if(id==7){selectedChannel_=steeringChannel();drawDesignPage(UiPage::MixingOverview);}
    else if(id==8){v2ReviewConfirm_=false;drawDesignPage(UiPage::RunReview);}
    else if(id==9)drawDesignPage(UiPage::TimerControl);
    else if(id==10){assignmentChannelContext_=0;drawControlAssignmentsPage();}
    else if(id==11){
        if(keypadReturnPage_==UiPage::RadioDiagnostics&&radioDiagnosticsReturnPage_==UiPage::RfWarningSetup)
            returnFromRadioDiagnostics();
        else{rfWarningReturnPage_=UiPage::QuickNavigation;drawDesignPage(UiPage::RfWarningSetup);}
    }
    else if(id==12)drawModelSelectPage();
    else if(id==13)drawDashboard();
}

void TransmitterUi::drawV2RunReview()
{
    makeLabel(lv_scr_act(),"Run review",12,49,UiText::Heading,cWhite);
    if (v2ReviewConfirm_) {
        char title[64];
        snprintf(title, sizeof(title), "Delete records for %s?", modelNames_[activeModelSlot_]);
        designDetails_[0] = makeLabel(lv_scr_act(),title,12,100,UiText::Menu,cWhite);
        lv_obj_set_width(designDetails_[0],kScreenWidth-24);
        lv_label_set_long_mode(designDetails_[0],LV_LABEL_LONG_WRAP);
        auto *help = makeLabel(lv_scr_act(),
            "Clears saved and current peaks, totals, laps and GPS. Pause the timer and keep CUT. Model settings are kept.",
            12,144,UiText::Secondary,cDim);
        lv_obj_set_width(help,kScreenWidth-24);
        lv_label_set_long_mode(help,LV_LABEL_LONG_WRAP);
        makePageActionButton(12,210,kScreenWidth/2-18,40,"Keep records",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlV2,51);
        makePageActionButton(kScreenWidth/2+6,210,kScreenWidth/2-18,40,"Delete records",UiText::Menu,cPanel2,cAmber,cLine,&TransmitterUi::controlV2,52);
        return;
    }
    if (telemetryHistory_) {
        designDetails_[1] = makeLabel(lv_scr_act(),"",12,72,UiText::Caption,cDim);
        lv_obj_set_width(designDetails_[1],kScreenWidth-24);
        lv_label_set_long_mode(designDetails_[1],LV_LABEL_LONG_CLIP);
    } else {
        makeLabel(lv_scr_act(),"History unavailable: memory",12,72,UiText::Caption,cAmber);
    }
    const char *names[]={"MAX SPEED","PEAK RPM","MIN PACK","MIN CELL","USED CHARGE","LOWEST LQ"};
    const char *units[]={"km/h","rpm","V","V","mAh","%"};
    const int cols=portraitUi?2:3,w=(kScreenWidth-20-(cols-1)*4)/cols;
    for(unsigned i=0;i<6;++i){auto p=makePanel(10+(i%cols)*(w+4),86+(i/cols)*73,w,67,cPanel,cLine);
        makeLabel(p,names[i],6,5,UiText::Caption,cDim);designValues_[i]=makeLabel(p,"--",6,24,UiText::Heading,cWhite);makeLabel(p,units[i],w-42,49,UiText::Caption,cDim);}
    const int y=portraitUi?318:232;
    makePageActionButton(12,y,kScreenWidth/2-18,30,"Save records",UiText::Caption,cPanel,cCyan,cLine,&TransmitterUi::controlV2,53);
    makePageActionButton(kScreenWidth/2+6,y,kScreenWidth/2-18,30,"Delete records...",UiText::Caption,cPanel,cAmber,cLine,&TransmitterUi::controlV2,50);
    designDetails_[0]=makeLabel(lv_scr_act(),runRecordsNotice_,12,y+31,UiText::Caption,cDim);
    lv_obj_set_width(designDetails_[0],kScreenWidth-24);
    lv_label_set_long_mode(designDetails_[0],portraitUi?LV_LABEL_LONG_WRAP:LV_LABEL_LONG_CLIP);
}
void TransmitterUi::updateV2RunReview()
{
    if(v2ReviewConfirm_)return;
    uiSetText(designDetails_[0],runRecordsNotice_);
    if (auto *label = designDetails_[1]) {
        if (runRecordsCheckpoint_ == RunRecordsCheckpoint::Saved) {
            const auto &time = runRecordsSavedAtLocal_;
            if (time.valid) {
                uiSetTextFmt(label,"Last save: %04u-%02u-%02u %02u:%02u:%02u local",
                    unsigned(time.year),unsigned(time.month),unsigned(time.day),
                    unsigned(time.hour),unsigned(time.minute),unsigned(time.second));
            } else {
                const auto seconds = static_cast<unsigned long>(runRecordsSavedAtMs_ / 1000);
                uiSetTextFmt(label,"Last save: %lu:%02lu:%02lu after boot",seconds/3600,seconds/60%60,seconds%60);
            }
        } else if (runRecordsCheckpoint_ == RunRecordsCheckpoint::Restored) {
            uiSetText(label,"Restored checkpoint. Save time unavailable.");
        } else {
            uiSetText(label,"No save this session");
        }
    }
    const uint8_t source[]={0,2,6,7,16,4};const uint32_t now=millis();
    for(unsigned i=0;i<6;++i){auto result=telemetryHistory_?sensorStatistics_[source[i]].result(true,now):SensorStatistics::Result{};char text[24];
        int32_t value=(i==2||i==3||i==5)?result.minimum:result.maximum;
        if(i==0){result.valid=speedStatistics_.available();value=speedStatistics_.maximum();}
        sensorText(source[i],value,text,sizeof(text));uiSetText(designValues_[i],result.valid?text:"--");}
}
void TransmitterUi::controlV2(uint8_t item,PageControlEvent event)
{
    if(event!=PageControlEvent::Activate)return;
    if(item==1)drawDashboard();
    else if(item==2){v2ReviewConfirm_=false;drawDesignPage(UiPage::RunReview);}
    else if(item==3){keypadReturnPage_=activePage_;drawDesignPage(UiPage::QuickNavigation);}
    else if(item==4){v2QuickEditing_=!v2QuickEditing_;drawDesignPage(UiPage::QuickNavigation);}
    else if(item>=10&&item<=13){if(item==13)openRadioDiagnostics();else{const uint8_t sources[]={0,6,2};sensorSelected_=sources[item-10];drawDesignPage(UiPage::SensorDetail);}}
    else if(item>=20&&item<=22){
        if(activePage_!=UiPage::SensorDetail)return;
        if (!telemetryHistory_ || !sensorDetailChart_) return;
        constexpr int newest = SensorHistory::Capacity - 1;
        if (item == 20) {
            v2GraphPaused_ = !v2GraphPaused_;
            if (v2GraphPaused_) {
                auto &history = *telemetryHistory_.get();
                history.frozenRange = history.graphs[sensorSelected_].project(millis(), history.frozen, history.frozenValid);
                v2GraphCursor_ = newest;
                while (v2GraphCursor_ && !history.frozenValid[v2GraphCursor_]) --v2GraphCursor_;
            }
        } else if (v2GraphPaused_) {
            v2GraphCursor_ = constrain(int(v2GraphCursor_) + (item == 21 ? -1 : 1), 0, newest);
        }
        drawDesignPage(UiPage::SensorDetail);
    }else if(item==30){
        v2SensorSetupReturn_=true;
        if(sensorSelected_==0)drawDesignPage(UiPage::WidgetSelect);
        else if(sensorSelected_==4){rfWarningReturnPage_=UiPage::SensorDetail;drawDesignPage(UiPage::RfWarningSetup);}
        else if(sensorSelected_>=6&&sensorSelected_<=11)drawDesignPage(UiPage::DriveBatterySetup);
        else{ersSelected_=-1;ersListBank_=0;ersPageMessage_[0]=0;drawDesignPage(UiPage::ErsSensors);}
    }else if(item>=40&&item<=43){
        tuneRole_=item==40||item==41?0:item==42?1:2;
        selectedChannel_=tuneRole_==0?steeringChannel():throttleChannel();
        openDesignEditor(item==40||item==42?5:0,UiPage::Dashboard);
    }else if(item==50||item==51){v2ReviewConfirm_=item==50;drawDesignPage(UiPage::RunReview);}
    else if(item==52){
        if(!v2ReviewConfirm_)return;
        if (SetupPortal::firmwareUpdating()) {
            snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"Wait for firmware update before deleting");
        } else if(armRequested_||designSafety_==OutputSafetyStatus::Armed||timers_[activeModelSlot_].running()){
            snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"CUT and pause timer before deleting");
        }else deleteRunRecords();
        v2ReviewConfirm_=false;drawDesignPage(UiPage::RunReview);
    }
    else if(item==53){if(armRequested_||designSafety_==OutputSafetyStatus::Armed){snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"CUT before saving manually");}else saveRunRecords();drawDesignPage(UiPage::RunReview);}
    else if(item>=60&&item<68){const unsigned i=item-60;if(i){const auto value=v2QuickSlots_[i];v2QuickSlots_[i]=v2QuickSlots_[i-1];v2QuickSlots_[i-1]=value;saveV2Quick();drawDesignPage(UiPage::QuickNavigation);}}
}
