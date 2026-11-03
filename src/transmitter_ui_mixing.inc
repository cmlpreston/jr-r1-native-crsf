// Versioned per-model mixer blob; never rewrite endpoint/calibration storage.
void TransmitterUi::setMixMonitorInputs(const MixInputSnapshot &inputs,const CrsfChannelOutputModel &bindings)
{
    mixingLiveSamples_=inputs;mixingLiveBindings_=bindings;mixingLiveUpdatedMs_=millis();
}
void TransmitterUi::loadMixing()
{
    for (auto &m : channelModel_.mixing) m = ChannelMixSettings{};
    if (ChannelSettingsRepository::loadMixing(activeModelSlot_, channelModel_) == ChannelSettingsLoad::Invalid)
        modelStorageFault_ = true;
}
bool TransmitterUi::saveMixing(uint8_t ch,const ChannelMixSettings &settings)
{
    if(ch<1||ch>16||!validChannelMix(settings))return false;
    if((ch==steeringChannel()||ch==throttleChannel())&&
       (settings.cutOverride||settings.outputMinimum>0||settings.outputMaximum<0))return false;
    requestConfigurationCut();
    ChannelMixSettings candidate[16];
    memcpy(candidate,channelModel_.mixing,sizeof(candidate));candidate[ch-1]=settings;
    if(!validChannelMixGraph(candidate))return false;
    auto configuration=controlConfigurationSnapshot();memcpy(configuration.mixing,candidate,sizeof(candidate));
    if(!validConfiguredMixing(configuration))return false;
    auto model = channelModel_;
    memcpy(model.mixing, candidate, sizeof(candidate));
    const bool ok = ChannelSettingsRepository::save(activeModelSlot_, model);
    if (ok) { channelModel_ = model; modelStorageFault_ = false; }
    return ok;
}
bool TransmitterUi::portalSetMixing(uint8_t ch,const ChannelMixSettings &settings)
{
    if(!SetupPortal::canControl()||armRequested_)return false;
    return saveMixing(ch,settings);
}
void TransmitterUi::drawChannelMixing()
{
    activePage_=UiPage::ChannelMixing;startScreen();resetLvglObjects();beginPageFocus();
    if(mixingBank_==5 || mixingBank_==7){drawMixMonitor();return;}
    char title[40];snprintf(title,sizeof(title),"CH%u %s",selectedChannel_,mixingBank_==6?"Baseline / limits":mixingBank_==2?"Switched rates":mixingBank_<2?(mixingBank_?"Mix 2":"Mix 1"):(mixingBank_==3?"Mix 1 activation":"Mix 2 activation"));
    makeHeader(title,"Back");
    const char *conditions[]={"Always","Switch low","Switch high","Walk mode","Normal mode"};
    const char *names[7]={"Enabled","Source","Positive weight","Negative weight","Operation","Follow source trim","Existing channel input"};
    char values[7][48]={};
    unsigned count=7;
    if(mixingBank_==6) {
        count=5;names[0]="Constant contribution";names[1]="Final output minimum";names[2]="Final output maximum";
        names[3]="Aux CUT override";names[4]="Aux CUT value";
        snprintf(values[0],48,"%+d%%",mixingDraft_.constantPercent);
        snprintf(values[1],48,"%+d%%",mixingDraft_.outputMinimum);
        snprintf(values[2],48,"%+d%%",mixingDraft_.outputMaximum);
        snprintf(values[3],48,"%s",mixingDraft_.cutOverride?"On":"Off");
        snprintf(values[4],48,"%+d%%",mixingDraft_.cutPercent);
    } else if(mixingBank_>=3) {
        const auto &r=mixingDraft_.rules[mixingBank_-3];count=5;
        names[0]="Active when";names[1]="Activation switch";names[2]="Source mode";names[3]="Trigger point";names[4]="Hysteresis";
        snprintf(values[0],48,"%s",conditions[r.condition]);
        snprintf(values[1],48,"%s",physicalControlDescriptor(r.control).name);
        const char *modes[]={"Proportional","Switch above","Switch below"};
        snprintf(values[2],48,"%s",modes[r.sourceMode]);
        snprintf(values[3],48,"%+d%%",r.thresholdPercent);
        snprintf(values[4],48,"%u%%",r.hysteresisPercent);
    } else if(mixingBank_<2) {
        const auto &r=mixingDraft_.rules[mixingBank_];
        snprintf(values[0],48,"%s",r.enabled?"On":"Off");
        if(r.source<kMixChannelSource)snprintf(values[1],48,"Raw: %s",physicalControlDescriptor(r.source).name);
        else if(r.source<kMixConfiguredSource)snprintf(values[1],48,"Channel %u",r.source-kMixChannelSource+1);
        else snprintf(values[1],48,"Input: %s",r.source==35?"Steering":"Throttle");
        snprintf(values[2],48,"%+d%%",r.weight);
        snprintf(values[3],48,"%+d%%",r.splitWeight?r.negativeWeight:r.weight);
        snprintf(values[4],48,"%s",r.replace?"Replace":"Add");
        snprintf(values[5],48,"%s",r.followTrim?"Yes":"No");
        snprintf(values[6],48,"%s",mixingDraft_.includeBase?"Include at 100%":"None (mixes only)");
    } else {
        count=7;const char *rateNames[]={"Enabled","Switch","Low: negative","Low: positive","High: negative","High: positive","Transition"};
        for(unsigned i=0;i<count;++i)names[i]=rateNames[i];
        snprintf(values[0],48,"%s",mixingDraft_.rateEnabled?"On":"Off");
        snprintf(values[1],48,"%s",physicalControlDescriptor(mixingDraft_.rateControl).name);
        const uint8_t rates[]={mixingDraft_.lowNegative,mixingDraft_.lowPositive,mixingDraft_.highNegative,mixingDraft_.highPositive};
        for(int i=0;i<4;++i)snprintf(values[i+2],48,"%u%%",rates[i]);
        snprintf(values[6],48,"%u ms",mixingDraft_.transitionMs);
    }
    for(unsigned i=0;i<count;++i) {
        const int pitch=portraitUi?51:count>=7?28:32;
        const int y=50+i*pitch,height=pitch-kPanelGap;
        auto panel=makePanel(8,y,kScreenWidth-16,height);
        makeLabel(panel,names[i],8,portraitUi?2:(height-UiText::font(UiText::Caption)->line_height)/2,UiText::Caption,cDim);
        auto value=makeLabel(panel,values[i],portraitUi?8:(mixingBank_==6?185:150),portraitUi?20:(height-UiText::font(UiText::Menu)->line_height)/2,UiText::Menu,cWhite);
        boundedLabel(value,portraitUi?184:(mixingBank_==6?160:200));
        const int x=kScreenWidth-108;
        makeButton(x,y+1,44,height-2,"-",UiText::Menu);makeButton(x+50,y+1,44,height-2,"+",UiText::Menu);
        addPageEditorTarget(8,y,kScreenWidth-16,height,x,y+1,44,height-2,x+50,y+1,44,height-2,&TransmitterUi::controlChannelMixing,i);
    }
    makeLabel(lv_scr_act(),mixingSaveError_?"Invalid mix (role/cycle) or save failed":mixingBank_==6?"CUT value: auxiliary channels only":"Switch mode: 0/+100; weights ignored",12,portraitUi?383:248,UiText::Caption,mixingSaveError_?cAmber:cDim);
    const int y=portraitUi?410:270,w=(kScreenWidth-40)/4;
    makePageActionButton(8,y,w,34,"Sections",UiText::Secondary,cPanel2,cCyan,cLine,&TransmitterUi::controlDesign,218);
    makePageActionButton(16+w,y,w,34,mixingBank_<2||mixingBank_==3||mixingBank_==4?"Other mix":"Sections",UiText::Secondary,cPanel2,cCyan,cLine,&TransmitterUi::controlChannelMixing,11);
    makePageActionButton(24+w*2,y,w,34,"Reset",UiText::Secondary,cPanel2,cAmber,cLine,&TransmitterUi::controlChannelMixing,13);
    makePageActionButton(32+w*3,y,w,34,"Apply",UiText::Secondary,cPanel2,cGreen,cLine,&TransmitterUi::controlChannelMixing,12);
    finishPageFocus();
}
void TransmitterUi::controlChannelMixing(uint8_t item,PageControlEvent event)
{
    if(item==14){mixingLiveInputs_=!mixingLiveInputs_;drawChannelMixing();return;}
    const int direction=event==PageControlEvent::Decrease?-1:1;
    auto toggleSwitch=[direction](uint8_t current) {
        const uint8_t ids[]={3,9,10};int index=current==3?0:current==9?1:2;
        return ids[(index+direction+3)%3];
    };
    if(item==10||item==11){if(mixingBank_<2)mixingBank_=1-mixingBank_;else if(mixingBank_==3||mixingBank_==4)mixingBank_=7-mixingBank_;else if(mixingBank_==5||mixingBank_==7)mixingBank_=12-mixingBank_;else{drawDesignPage(UiPage::MixingOverview);return;}}
    else if(item==13) {
        mixingDraft_=ChannelMixSettings{};mixingSaveError_=false;mixingBank_=0;
    } else if(item==12) {
        mixingSaveError_=!saveMixing(selectedChannel_,mixingDraft_);
        if(!mixingSaveError_){drawChannelPlaceholder();return;}
    } else if(mixingBank_==7) {
        if(item==0)mixingCurveSource_=(mixingCurveSource_+direction+3)%3;
    } else if(mixingBank_==6) {
        const bool role=selectedChannel_==steeringChannel()||selectedChannel_==throttleChannel();
        if(item==0)mixingDraft_.constantPercent=constrain(mixingDraft_.constantPercent+direction*5,-100,100);
        if(item==1)mixingDraft_.outputMinimum=constrain(mixingDraft_.outputMinimum+direction*5,-100,role?min(0,int(mixingDraft_.outputMaximum)):int(mixingDraft_.outputMaximum));
        if(item==2)mixingDraft_.outputMaximum=constrain(mixingDraft_.outputMaximum+direction*5,role?max(0,int(mixingDraft_.outputMinimum)):int(mixingDraft_.outputMinimum),100);
        if(item==3&&!role)mixingDraft_.cutOverride=!mixingDraft_.cutOverride;
        if(item==4&&!role)mixingDraft_.cutPercent=constrain(mixingDraft_.cutPercent+direction*5,-100,100);
    } else if(mixingBank_==5) {
        if(mixingLiveInputs_)return;
        if(item<3) {
            mixingPreviewValues_[item]=constrain(mixingPreviewValues_[item]+direction*100,-1000,1000);
            if(item>0&&mixingDraft_.rules[0].source==mixingDraft_.rules[1].source)
                mixingPreviewValues_[3-item]=mixingPreviewValues_[item];
        }
        if(item==3)mixingPreviewHigh_=!mixingPreviewHigh_;
        if(item==4)mixingPreviewWalk_=!mixingPreviewWalk_;
    } else if(mixingBank_>=3) {
        auto &r=mixingDraft_.rules[mixingBank_-3];
        if(item==0)r.condition=(r.condition+direction+5)%5;
        if(item==1)r.control=toggleSwitch(r.control);
        if(item==2)r.sourceMode=(r.sourceMode+direction+3)%3;
        if(item==3)r.thresholdPercent=constrain(r.thresholdPercent+direction,-100,100);
        if(item==4)r.hysteresisPercent=constrain(r.hysteresisPercent+direction,0,100);
        if(r.sourceMode==1)r.hysteresisPercent=min(int(r.hysteresisPercent),int(r.thresholdPercent)+100);
        if(r.sourceMode==2)r.hysteresisPercent=min(int(r.hysteresisPercent),100-int(r.thresholdPercent));
    } else if(mixingBank_<2) {
        auto &r=mixingDraft_.rules[mixingBank_];
        if(item==0)r.enabled=!r.enabled;
        if(item==1){r.source=(r.source+direction+kMixSourceCount)%kMixSourceCount;
            if(mixingDraft_.rules[0].source==mixingDraft_.rules[1].source)mixingPreviewValues_[2]=mixingPreviewValues_[1];}
        if(item==2||item==3) {
            if(!r.splitWeight){r.negativeWeight=r.weight;r.splitWeight=1;}
            auto &weight=item==2?r.weight:r.negativeWeight;
            weight=constrain(weight+direction*5,-100,100);
        }
        if(item==4)r.replace=!r.replace;
        if(item==5)r.followTrim=!r.followTrim;
        if(item==6)mixingDraft_.includeBase=!mixingDraft_.includeBase;
    } else {
        if(item==0)mixingDraft_.rateEnabled=!mixingDraft_.rateEnabled;
        if(item==1)mixingDraft_.rateControl=toggleSwitch(mixingDraft_.rateControl);
        if(item>=2&&item<=5) {
            uint8_t *rates[]={&mixingDraft_.lowNegative,&mixingDraft_.lowPositive,&mixingDraft_.highNegative,&mixingDraft_.highPositive};
            *rates[item-2]=constrain(*rates[item-2]+direction*5,0,100);
        }
        if(item==6)mixingDraft_.transitionMs=constrain(mixingDraft_.transitionMs+direction*100,0,2000);
    }
    drawChannelMixing();
}

namespace {
void mixSourceCaption(uint8_t source,char *text,size_t size)
{
    if(source<kMixChannelSource)snprintf(text,size,"%s",physicalControlDescriptor(source).name);
    else if(source<kMixConfiguredSource)snprintf(text,size,"CH%u",source-kMixChannelSource+1);
    else snprintf(text,size,"%s input",source==35?"Steering":"Throttle");
}
}

void TransmitterUi::drawMixMonitor()
{
    const bool graph=mixingBank_==7;
    char title[40];snprintf(title,sizeof(title),"CH%u %s",selectedChannel_,graph?"Mix curve":"Mix inputs");makeHeader(title,"Back");
    auto editor=[&](int y,const char *name,const char *value,uint8_t action,int height){
        const bool stacked=portraitUi && height>=36;
        auto panel=makePanel(8,y,kScreenWidth-16,height,cPanel,cLine);
        makeLabel(panel,name,8,2,UiText::Caption,cDim);
        auto label=makeLabel(panel,value,stacked?8:portraitUi?110:145,stacked?20:2,UiText::Caption,cWhite);
        boundedLabel(label,stacked?kScreenWidth-132:portraitUi?kScreenWidth-230:kScreenWidth-270);
        const int x=kScreenWidth-108;
        const auto font=height<24?UiText::Caption:UiText::Menu;
        makeButton(x,y+1,44,height-2,"-",font);makeButton(x+50,y+1,44,height-2,"+",font);
        addPageEditorTarget(8,y,kScreenWidth-16,height,x,y+1,44,height-2,x+50,y+1,44,height-2,&TransmitterUi::controlChannelMixing,action);
    };
    if(graph){
        char input[64],source[40];
        if(mixingCurveSource_==0)snprintf(input,sizeof(input),"Existing input");
        else {mixSourceCaption(mixingDraft_.rules[mixingCurveSource_-1].source,source,sizeof(source));snprintf(input,sizeof(input),"S%u: %s",mixingCurveSource_,source);}
        editor(50,"Horizontal axis",input,0,portraitUi?38:26);
        editor(portraitUi?94:82,"Inputs",mixingLiveInputs_?"Live controls":"Test values",14,portraitUi?38:26);
        mixingReadouts_[0]=makeLabel(lv_scr_act(),"",12,portraitUi?148:114,UiText::Caption,cWhite);
        boundedLabel(mixingReadouts_[0],kScreenWidth-24);
        lv_label_set_long_mode(mixingReadouts_[0],LV_LABEL_LONG_CLIP);lv_obj_set_height(mixingReadouts_[0],UiText::font(UiText::Caption)->line_height);
        makeLabel(lv_scr_act(),"Target: amber | Sent: green (%)",40,portraitUi?172:130,UiText::Caption,cDim);
        const int x=40,y=portraitUi?192:149,w=kScreenWidth-56,h=portraitUi?144:66;
        auto panel=makePanel(x,y,w+1,h+1,cPanel,cLine);
        auto axis=makePanel(0,0,w,1,cLine,cLine,0);lv_obj_set_parent(axis,panel);lv_obj_set_pos(axis,0,h/2);
        axis=makePanel(0,0,1,h,cLine,cLine,0);lv_obj_set_parent(axis,panel);lv_obj_set_pos(axis,w/2,0);
        mixingCurve_=lv_line_create(panel);lv_obj_set_style_line_color(mixingCurve_,colour(cCyan),0);lv_obj_set_style_line_width(mixingCurve_,2,0);
        uiSetHidden(mixingCurve_,true);
        mixingCursor_=makePanel(0,0,7,7,cAmber,cAmber,3);lv_obj_set_parent(mixingCursor_,panel);uiSetHidden(mixingCursor_,true);
        mixingSentCursor_=makePanel(0,0,9,9,cPanel,cGreen,0);lv_obj_set_parent(mixingSentCursor_,panel);
        lv_obj_set_style_bg_opa(mixingSentCursor_,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(mixingSentCursor_,2,0);uiSetHidden(mixingSentCursor_,true);
        makeLabel(lv_scr_act(),"+100",3,y-4,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"0",21,y+h/2-5,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"-100",3,y+h-8,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"-100",x,y+h+3,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"0",x+w/2-3,y+h+3,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"+100",x+w-28,y+h+3,UiText::Caption,cDim);
    }else{
        if(mixingLiveInputs_)editor(50,"Inputs","Live controls",14,portraitUi?38:26);
        else{
            const char *names[]={"Inputs","Existing input","Source 1","Source 2","Test switch","Test drive mode"};
            for(unsigned i=0;i<6;++i){
                char value[32];
                if(!i)snprintf(value,sizeof(value),"Test values");
                else if(i<4)snprintf(value,sizeof(value),"%+d%%",mixingPreviewValues_[i-1]/10);
                else snprintf(value,sizeof(value),"%s",i==4?(mixingPreviewHigh_?"High":"Low"):(mixingPreviewWalk_?"Walk":"Normal"));
                editor(50+i*(portraitUi?32:22),names[i],value,i?i-1:14,portraitUi?28:20);
            }
        }
        for(unsigned n=0;n<(mixingLiveInputs_?6u:5u);++n){
            const bool live=mixingLiveInputs_;
            const int col=(!portraitUi&&!live)?n/3:0;
            const int y=live?(portraitUi?110+n*42:88+n*24):(portraitUi?254+n*23:188+(n%3)*20);
            const int x=12+col*kScreenWidth/2;
            mixingReadouts_[n]=makeLabel(lv_scr_act(),"",x,y,UiText::Caption,cWhite);
            boundedLabel(mixingReadouts_[n],live?(portraitUi?kScreenWidth-24:230):(portraitUi?180:kScreenWidth/2-18));
            lv_label_set_long_mode(mixingReadouts_[n],LV_LABEL_LONG_CLIP);lv_obj_set_height(mixingReadouts_[n],UiText::font(UiText::Caption)->line_height);
            const int bx=live?(portraitUi?12:248):(portraitUi?196:132+col*kScreenWidth/2);
            const int by=live?(portraitUi?y+22:y+5):y+6;
            const int width=live?(portraitUi?kScreenWidth-24:kScreenWidth-264):(portraitUi?kScreenWidth-212:kScreenWidth/2-148);
            mixingLiveBars_[n]=makeBar(lv_scr_act(),bx,by,width,5,-1000,1000);
            lv_bar_set_mode(mixingLiveBars_[n],LV_BAR_MODE_SYMMETRICAL);
        }
    }
    mixingSummary_=makeLabel(lv_scr_act(),"",12,portraitUi?383:246,UiText::Caption,cWhite);
    boundedLabel(mixingSummary_,kScreenWidth-24);
    lv_label_set_long_mode(mixingSummary_,LV_LABEL_LONG_CLIP);lv_obj_set_height(mixingSummary_,UiText::font(UiText::Caption)->line_height);
    const int y=portraitUi?410:270,w=(kScreenWidth-40)/4;
    makePageActionButton(8,y,w,34,"Sections",UiText::Secondary,cPanel2,cCyan,cLine,&TransmitterUi::controlDesign,218);
    makePageActionButton(16+w,y,w,34,graph?"Inputs":"Curve",UiText::Secondary,cPanel2,cCyan,cLine,&TransmitterUi::controlChannelMixing,11);
    makePageActionButton(24+w*2,y,w,34,portraitUi?"Reset":"Reset mix",UiText::Secondary,cPanel2,cAmber,cLine,&TransmitterUi::controlChannelMixing,13);
    makePageActionButton(32+w*3,y,w,34,"Apply",UiText::Secondary,cPanel2,cGreen,cLine,&TransmitterUi::controlChannelMixing,12);
    finishPageFocus();lv_obj_update_layout(lv_scr_act());refreshChannelMixing();
}

void TransmitterUi::refreshChannelMixing()
{
    if(activePage_!=UiPage::ChannelMixing || (mixingBank_!=5 && mixingBank_!=7))return;
    const bool graph=mixingBank_==7;
    const auto config=controlConfigurationSnapshot();ChannelMixMonitor monitor;
    if(mixingLiveInputs_){
        if(mixingLiveUpdatedMs_ && uint32_t(millis()-mixingLiveUpdatedMs_)<=500)
            monitor=monitorChannelMix(config,mixingLiveSamples_,mixingLiveBindings_,selectedChannel_,mixingDraft_,graph?mixingCurveSource_:-1);
    }else{
        monitor.valid=validChannelMix(mixingDraft_);monitor.base=mixingPreviewValues_[0];
        for(unsigned n=0;n<2;++n)monitor.sources[n]=mixingPreviewValues_[n+1];
        monitor.result=previewChannelMix(config,selectedChannel_,mixingDraft_,monitor.base,monitor.sources,mixingPreviewHigh_,mixingPreviewWalk_);
        if(graph)previewChannelMixCurve(config,selectedChannel_,mixingDraft_,monitor.base,monitor.sources,mixingPreviewHigh_,mixingPreviewWalk_,mixingCurveSource_,monitor.curve);
    }
    const int sent=permilleFromCrsfValue(dashboardChannelValues_[selectedChannel_-1]);
    if(graph){
        if(monitor.valid)uiSetTextFmt(mixingReadouts_[0],"Base %+d%% | S1 %+d%% | S2 %+d%%",monitor.base/10,monitor.sources[0]/10,monitor.sources[1]/10);
        else uiSetText(mixingReadouts_[0],"Live inputs unavailable / check mix sources");
        uiSetHidden(mixingCurve_,!monitor.valid);uiSetHidden(mixingCursor_,!monitor.valid);uiSetHidden(mixingSentCursor_,!monitor.valid);
        if(monitor.valid){
            auto panel=lv_obj_get_parent(mixingCurve_);const int w=lv_obj_get_width(panel)-1,h=lv_obj_get_height(panel)-1;
            bool changed=false;
            for(unsigned n=0;n<kMixCurvePoints;++n){
                const lv_point_t point={lv_coord_t(n*w/(kMixCurvePoints-1)),lv_coord_t(h/2-monitor.curve[n]*h/2000)};
                changed|=mixingCurvePoints_[n].x!=point.x || mixingCurvePoints_[n].y!=point.y;mixingCurvePoints_[n]=point;
            }
            if(changed)lv_line_set_points(mixingCurve_,mixingCurvePoints_,kMixCurvePoints);
            const int input=mixingCurveSource_?monitor.sources[mixingCurveSource_-1]:monitor.base;
            lv_obj_set_pos(mixingCursor_,constrain((input+1000)*w/2000,0,w)-3,h/2-monitor.result.output*h/2000-3);
            lv_obj_set_pos(mixingSentCursor_,constrain((input+1000)*w/2000,0,w)-4,h/2-sent*h/2000-4);
        }
    }else{
        const int valuesLive[]={monitor.base,monitor.sources[0],monitor.sources[1],monitor.result.contributions[0],monitor.result.contributions[1],monitor.result.output};
        const int valuesTest[]={(mixingDraft_.includeBase?monitor.base:0)+mixingDraft_.constantPercent*10,monitor.result.contributions[0],monitor.result.contributions[1],monitor.result.mixed,monitor.result.output};
        const char *liveNames[]={"Existing input","Source 1","Source 2","Mix 1 effect","Mix 2 effect","Draft target"};
        const char *testNames[]={"Baseline","Mix 1 effect","Mix 2 effect","Mixed","Draft target"};
        for(unsigned n=0;n<(mixingLiveInputs_?6u:5u);++n){
            char name[64];snprintf(name,sizeof(name),"%s",mixingLiveInputs_?liveNames[n]:testNames[n]);
            if(mixingLiveInputs_ && (n==1||n==2)){
                char source[40];mixSourceCaption(mixingDraft_.rules[n-1].source,source,sizeof(source));
                snprintf(name,sizeof(name),"S%u %s",n,source);
            }
            const int value=mixingLiveInputs_?valuesLive[n]:valuesTest[n];
            if(monitor.valid)uiSetTextFmt(mixingReadouts_[n],"%s: %+d%%",name,value/10);
            else uiSetTextFmt(mixingReadouts_[n],"%s: --",name);
            const int shown=monitor.valid?constrain(value,-1000,1000):0;
            if(lv_bar_get_value(mixingLiveBars_[n])!=shown)lv_bar_set_value(mixingLiveBars_[n],shown,LV_ANIM_OFF);
        }
    }
    if(mixingSaveError_)uiSetText(mixingSummary_,"Invalid mix (role/cycle) or save failed");
    else if(monitor.valid)uiSetTextFmt(mixingSummary_,"Draft %+d%% | Sent %+d%% %s | Apply saves",monitor.result.output/10,sent/10,dashboardOutputsLive_?"LIVE":"CUT");
    else uiSetTextFmt(mixingSummary_,"No live inputs | Sent %+d%% %s",sent/10,dashboardOutputsLive_?"LIVE":"CUT");
}
