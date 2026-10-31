// Versioned per-model mixer blob; never rewrite endpoint/calibration storage.
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
    char title[40];snprintf(title,sizeof(title),"CH%u %s",selectedChannel_,mixingBank_==7?"Mix curve":mixingBank_==6?"Baseline / limits":mixingBank_==5?"Mix preview":mixingBank_==2?"Switched rates":mixingBank_<2?(mixingBank_?"Mix 2":"Mix 1"):(mixingBank_==3?"Mix 1 activation":"Mix 2 activation"));
    makeHeader(title,"Back");
    const char *conditions[]={"Always","Switch low","Switch high","Walk mode","Normal mode"};
    const char *names[7]={"Enabled","Source","Positive weight","Negative weight","Operation","Follow source trim","Existing channel input"};
    char values[7][48]={};
    unsigned count=7;
    if(mixingBank_==7) {
        count=4;names[0]="Graph input";names[1]="Test existing input";names[2]="Test source 1";names[3]="Test source 2";
        snprintf(values[0],48,"%s",mixingCurveSource_==0?"Existing input":mixingCurveSource_==1?"Source 1":"Source 2");
        for(int n=0;n<3;++n)snprintf(values[n+1],48,"%+d%%",mixingPreviewValues_[n]/10);
    } else if(mixingBank_==6) {
        count=5;names[0]="Constant contribution";names[1]="Final output minimum";names[2]="Final output maximum";
        names[3]="Aux CUT override";names[4]="Aux CUT value";
        snprintf(values[0],48,"%+d%%",mixingDraft_.constantPercent);
        snprintf(values[1],48,"%+d%%",mixingDraft_.outputMinimum);
        snprintf(values[2],48,"%+d%%",mixingDraft_.outputMaximum);
        snprintf(values[3],48,"%s",mixingDraft_.cutOverride?"On":"Off");
        snprintf(values[4],48,"%+d%%",mixingDraft_.cutPercent);
    } else if(mixingBank_==5) {
        count=5;names[0]="Test existing input";names[1]="Test source 1";names[2]="Test source 2";
        names[3]="Test switch";names[4]="Test drive mode";
        for(int n=0;n<3;++n)snprintf(values[n],48,"%+d%%",mixingPreviewValues_[n]/10);
        snprintf(values[3],48,"%s",mixingPreviewHigh_?"High":"Low");
        snprintf(values[4],48,"%s",mixingPreviewWalk_?"Walk":"Normal");
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
        const int pitch=mixingBank_==5?(portraitUi?44:24):(portraitUi?51:count>=7?28:32);
        const int y=50+i*pitch,height=pitch-kPanelGap;
        auto panel=makePanel(8,y,kScreenWidth-16,height);
        makeLabel(panel,names[i],8,portraitUi?2:(height-UiText::font(UiText::Caption)->line_height)/2,UiText::Caption,cDim);
        auto value=makeLabel(panel,values[i],portraitUi?8:(mixingBank_==6?185:150),portraitUi?20:(height-UiText::font(mixingBank_==5?UiText::Caption:UiText::Menu)->line_height)/2,mixingBank_==5?UiText::Caption:UiText::Menu,cWhite);
        boundedLabel(value,portraitUi?184:(mixingBank_==6?160:200));
        const int x=kScreenWidth-108;
        makeButton(x,y+1,44,height-2,"-",UiText::Menu);makeButton(x+50,y+1,44,height-2,"+",UiText::Menu);
        addPageEditorTarget(8,y,kScreenWidth-16,height,x,y+1,44,height-2,x+50,y+1,44,height-2,&TransmitterUi::controlChannelMixing,i);
    }
    if(mixingBank_==5) {
        const auto preview=previewChannelMix(controlConfigurationSnapshot(),selectedChannel_,mixingDraft_,
            mixingPreviewValues_[0],mixingPreviewValues_+1,mixingPreviewHigh_,mixingPreviewWalk_);
        const char *labels[]={"Base+const","Mix 1","Mix 2","Mixed","Output"};
        const int values[]={(mixingDraft_.includeBase?mixingPreviewValues_[0]:0)+mixingDraft_.constantPercent*10,preview.contributions[0],preview.contributions[1],preview.mixed,preview.output};
        for(int n=0;n<5;++n){
            const int column=portraitUi?0:n/3;
            const int y=portraitUi?276+n*20:180+(n%3)*20;
            char text[32];snprintf(text,sizeof(text),"%s %+d%%",labels[n],values[n]/10);
            makeLabel(lv_scr_act(),text,12+column*kScreenWidth/2,y,UiText::Caption,cWhite);
            const int x=portraitUi?112:132+column*kScreenWidth/2,width=portraitUi?kScreenWidth-128:kScreenWidth/2-148,half=width/2;
            makePanel(x,y+3,width,7);
            const int length=constrain(abs(values[n])*half/1000,0,half);
            if(length){auto bar=makePanel(x+half+(values[n]<0?-length:0),y+3,length,7);lv_obj_set_style_bg_color(bar,colour(cCyan),0);}
            auto zero=makePanel(x+half,y+1,1,11);lv_obj_set_style_bg_color(zero,colour(cWhite),0);
        }
    }
    if(mixingBank_==7) {
        const auto config=controlConfigurationSnapshot();
        int16_t samples[kMixCurvePoints];
        previewChannelMixCurve(config,selectedChannel_,mixingDraft_,mixingPreviewValues_[0],
            mixingPreviewValues_+1,mixingPreviewHigh_,mixingPreviewWalk_,mixingCurveSource_,samples);
        const int x=40,y=portraitUi?262:178,w=kScreenWidth-56,h=portraitUi?96:48;
        makePanel(x,y,w+1,h+1,cPanel,cLine);
        auto horizontal=makePanel(x,y+h/2,w,1,cLine,cLine,0);
        (void)horizontal;makePanel(x+w/2,y,1,h,cLine,cLine,0);
        for(unsigned n=0;n<kMixCurvePoints;++n)
            mixingCurvePoints_[n]={static_cast<lv_coord_t>(x+n*w/(kMixCurvePoints-1)),
                static_cast<lv_coord_t>(y+h/2-samples[n]*h/2000)};
        auto curve=lv_line_create(lv_scr_act());lv_line_set_points(curve,mixingCurvePoints_,kMixCurvePoints);
        lv_obj_set_style_line_color(curve,colour(cCyan),0);lv_obj_set_style_line_width(curve,2,0);
        const auto current=previewChannelMix(config,selectedChannel_,mixingDraft_,mixingPreviewValues_[0],
            mixingPreviewValues_+1,mixingPreviewHigh_,mixingPreviewWalk_);
        const int px=x+(mixingPreviewValues_[mixingCurveSource_]+1000)*w/2000;
        const int py=y+h/2-current.output*h/2000;
        makePanel(px-3,py-3,7,7,cAmber,cAmber,3);
        makeLabel(lv_scr_act(),"+100",3,y-4,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"0",21,y+h/2-5,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"-100",3,y+h-8,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"-100",x,y+h+3,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"0",x+w/2-3,y+h+3,UiText::Caption,cDim);
        makeLabel(lv_scr_act(),"+100",x+w-28,y+h+3,UiText::Caption,cDim);
        char caption[64];snprintf(caption,sizeof(caption),"CH%u output %+d%% | %s / %s",selectedChannel_,current.output/10,
            mixingPreviewHigh_?"Switch high":"Switch low",mixingPreviewWalk_?"Walk":"Normal");
        makeLabel(lv_scr_act(),caption,12,portraitUi?242:162,UiText::Caption,cWhite);
    }
    makeLabel(lv_scr_act(),mixingSaveError_?"Invalid mix (role/cycle) or save failed":mixingBank_>=7?"Preview only; switches set on bars page":mixingBank_==5?"Test only; no transmission; rates settled":mixingBank_==6?"CUT value: auxiliary channels only":"Switch mode: 0/+100; weights ignored",12,portraitUi?383:248,UiText::Caption,mixingSaveError_?cAmber:cDim);
    const int y=portraitUi?410:270,w=(kScreenWidth-40)/4;
    makePageActionButton(8,y,w,34,"Sections",UiText::Secondary,cPanel2,cCyan,cLine,&TransmitterUi::controlDesign,218);
    makePageActionButton(16+w,y,w,34,mixingBank_<2?"Other mix":mixingBank_==3||mixingBank_==4?"Other mix":mixingBank_==5?"Graph":mixingBank_==7?"Bars":"Sections",UiText::Secondary,cPanel2,cCyan,cLine,&TransmitterUi::controlChannelMixing,11);
    makePageActionButton(24+w*2,y,w,34,"Reset",UiText::Secondary,cPanel2,cAmber,cLine,&TransmitterUi::controlChannelMixing,13);
    makePageActionButton(32+w*3,y,w,34,"Apply",UiText::Secondary,cPanel2,cGreen,cLine,&TransmitterUi::controlChannelMixing,12);
    finishPageFocus();
}
void TransmitterUi::controlChannelMixing(uint8_t item,PageControlEvent event)
{
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
        else if(item<=3){
            const int input=item-1;
            mixingPreviewValues_[input]=constrain(mixingPreviewValues_[input]+direction*100,-1000,1000);
            if(input>0&&mixingDraft_.rules[0].source==mixingDraft_.rules[1].source)
                mixingPreviewValues_[3-input]=mixingPreviewValues_[input];
        }
    } else if(mixingBank_==6) {
        const bool role=selectedChannel_==steeringChannel()||selectedChannel_==throttleChannel();
        if(item==0)mixingDraft_.constantPercent=constrain(mixingDraft_.constantPercent+direction*5,-100,100);
        if(item==1)mixingDraft_.outputMinimum=constrain(mixingDraft_.outputMinimum+direction*5,-100,role?min(0,int(mixingDraft_.outputMaximum)):int(mixingDraft_.outputMaximum));
        if(item==2)mixingDraft_.outputMaximum=constrain(mixingDraft_.outputMaximum+direction*5,role?max(0,int(mixingDraft_.outputMinimum)):int(mixingDraft_.outputMinimum),100);
        if(item==3&&!role)mixingDraft_.cutOverride=!mixingDraft_.cutOverride;
        if(item==4&&!role)mixingDraft_.cutPercent=constrain(mixingDraft_.cutPercent+direction*5,-100,100);
    } else if(mixingBank_==5) {
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
