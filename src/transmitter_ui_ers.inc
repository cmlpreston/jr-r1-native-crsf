// Native commissioning uses the same client and management queue as the web UI.
void TransmitterUi::consumeErsReplies(CrsfTelemetryParser &parser)
{
    if(designSafety_!=OutputSafetyStatus::Cut||armRequested_||
       (moduleLinkManaged_&&!moduleLink_.enabled()))ersConfig_.cancel();
    ErsReply reply;
    for(uint32_t n=0;n<ErsReplyQueue::Capacity&&parser.popErsReply(reply);++n){
        ++ersSequence_;
        ersConfig_.receive(reply.frameType,reply.payload,reply.length,reply.receivedMs);
    }
    const uint32_t dropped=parser.ersReplyDrops();
    if(dropped!=ersReplyDropsSeen_){
        ersReplyDropsSeen_=dropped;
        snprintf(ersPageMessage_,sizeof(ersPageMessage_),"ERS reply buffer full; run Discover again");
        ersUiRevision_=~ersConfig_.revision();
    }
}

void TransmitterUi::refreshErsDraft(const ErsDevice &device)
{
    const bool useGpsMode=device.type==3;
    const uint8_t knownBit=useGpsMode?1:device.type==8?2:4;
    const bool known=(device.known&knownBit)!=0;
    const uint16_t confirmed=useGpsMode?device.gpsMode:device.type==8?device.motorPoles:device.capacityMah;
    if(ersDraftDirty_&&!ersConfig_.busy()&&known&&confirmed==ersDraft_)ersDraftDirty_=false;
    if(!ersDraftDirty_)ersDraft_=known?confirmed:device.type==4?5000:2;

    if(useGpsMode)uiSetText(designValues_[0],ersDraft_==2?"Global / position":"Speed only");
    else uiSetTextFmt(designValues_[0],"%u",unsigned(ersDraft_));
    uiSetText(designDetails_[0],ersDraftDirty_?"Draft; Apply sends it to the sensor.":
        known?"Confirmed sensor setting":"Unconfirmed default; read settings first");
    if(designDetails_[1])uiSetText(designDetails_[1],ersFine_?"Step: 1":"Step: 100");
}

void TransmitterUi::drawErsSensorContent()
{
    const bool portrait=portraitUi;
    const int bottom=kScreenHeight-(portrait?60:40),width=kScreenWidth-24;
    constexpr int actionHeight=40;
    auto text=[&](const char *s,int x,int y,UiText::Style style=UiText::Caption,uint32_t c=cWhite){
        auto l=makeLabel(lv_scr_act(),s,x,y,style,c);lv_obj_set_width(l,kScreenWidth-x-12);
        lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);return l;
    };
    auto button=[&](int x,int y,int w,const char *s,uint8_t id){
        return makePageActionButton(x,y,w,actionHeight,s,UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlErsSensor,id);
    };
    if(ersSelected_>=ersConfig_.count())ersSelected_=-1;
    text("ERS sensors",12,48,UiText::Heading);
    const char *status=ersPageMessage_[0]?ersPageMessage_:ersConfig_.status();
    if(moduleLinkManaged_ && !moduleLink_.enabled())status="Module link paused. Enable Link to discover.";
    text(status,12,portrait?82:76,UiText::Caption,cAmber);
    const int footer=bottom-actionHeight-2;
    if(ersSelected_<0){
        const int top=portrait?132:101,step=portrait?54:34;
        for(uint8_t row=0;row<4;++row){
            const unsigned i=ersListBank_*4+row;if(i>=ersConfig_.count())break;
            const auto &d=ersConfig_.device(i);char line[64];
            const char *type=d.type==2?"Cells":d.type==3?"GPS":d.type==4?"Current":"RPM";
            snprintf(line,sizeof(line),"%s / %s  #%u",d.name[0]?d.name:"ERS",type,d.id);
            drawMenuRow(line,"",top+row*step,portrait?48:34,&TransmitterUi::controlErsSensor,10+row);
        }
        if(!ersConfig_.count())text("Connect the ERS chain to CRSF UART.\nKeep CUT; establish the receiver RF link.",12,portrait?170:133,UiText::Secondary,cDim);
        button(12,footer,portrait?118:146,"Discover",0);
        const int linkX=portrait?138:166;
        makePageActionButton(linkX,footer,portrait?70:100,actionHeight,"Link",UiText::Menu,cPanel,cWhite,cLine,&TransmitterUi::controlDesign,239);
        button(kScreenWidth-100,footer,42,"<",1);button(kScreenWidth-54,footer,42,">",2);
        return;
    }
    const auto &d=ersConfig_.device(ersSelected_);
    const bool editable=d.type==3 || d.type==4 || d.type==8;
    char name[64];snprintf(name,sizeof(name),"%s / ID %u",d.name[0]?d.name:"ERS sensor",d.id);
    text(name,12,portrait?122:108,UiText::Menu,cCyan);
    const int readY=portrait?157:134;
    button(12,readY,portrait?154:144,"Read settings",3);
    const int top=portrait?209:177;
    if(editable){
        const char *title=d.type==3?"GPS mode":d.type==8?"Motor poles (total)":"Battery capacity (mAh)";
        text(title,12,top,UiText::Caption,cDim);
        designValues_[0]=makeLabel(lv_scr_act(),"",12,top+19,UiText::Menu,cWhite);
        lv_obj_set_width(designValues_[0],width-112);
        lv_label_set_long_mode(designValues_[0],LV_LABEL_LONG_DOT);
        button(kScreenWidth-108,top+14,44,"-",6);button(kScreenWidth-56,top+14,44,"+",7);
        if(d.type==4)designDetails_[1]=lv_obj_get_child(button(kScreenWidth-146,readY,134,"",9),0);
        designDetails_[0]=text("",12,portrait?286:222,UiText::Caption,cDim);
        if(!portrait)lv_obj_set_width(designDetails_[0],width-112);
        refreshErsDraft(d);
        button(kScreenWidth/2+4,footer,kScreenWidth/2-16,"Apply",8);
    }else{
        text("Cell readings appear in Data and\nthe Battery cells widget.",12,top,UiText::Secondary,cDim);
        if(d.known&8){char id[40];snprintf(id,sizeof(id),"Telemetry source: %u",d.telemetryId);text(id,12,portrait?286:229,UiText::Caption,cCyan);}
    }
    button(12,footer,kScreenWidth/2-16,"Sensor list",4);
}

void TransmitterUi::controlErsSensor(uint8_t item,PageControlEvent event)
{
    if(activePage_!=UiPage::ErsSensors)return;
    if(item==4){
        if(ersSelected_<0){if(v2SensorSetupReturn_){v2SensorSetupReturn_=false;drawDesignPage(UiPage::SensorDetail);}else{setupBank_=0;setupPage_=1;drawMainMenu();}}
        else{ersSelected_=-1;ersDraftDirty_=false;ersPageMessage_[0]=0;drawDesignPage(UiPage::ErsSensors);}return;
    }
    if(item==1 || item==2){ersListBank_=item==1?0:ersConfig_.count()>4?1:0;drawDesignPage(UiPage::ErsSensors);return;}
    if(item>=10 && item<=13 && ersSelected_<0){
        const unsigned index=ersListBank_*4+item-10;if(index>=ersConfig_.count())return;
        ersSelected_=index;ersDraftDirty_=false;ersFine_=false;ersPageMessage_[0]=0;
        if(ersUsePackCapacity_ && ersConfig_.device(index).type==4){ersDraft_=driveBatterySettings_.capacityMah;ersDraftDirty_=true;ersUsePackCapacity_=false;}
        drawDesignPage(UiPage::ErsSensors);return;
    }
    if(item==6||item==7||item==9){
        if(ersSelected_<0||ersSelected_>=ersConfig_.count()||!designValues_[0])return;
        const auto &d=ersConfig_.device(ersSelected_);
        if(item==9){
            if(!designDetails_[1])return;
            ersFine_=!ersFine_;
        }else{
            if(ersConfig_.busy())return;
            const int step=d.type==4&&!ersFine_?100:1;
            const int direction=event==PageControlEvent::Decrease?-1:event==PageControlEvent::Increase?1:item==6?-1:1;
            ersDraft_=constrain(int(ersDraft_)+direction*step,1,d.type==3?2:d.type==8?100:65535);
            ersDraftDirty_=true;
        }
        refreshErsDraft(d);
        return;
    }
    ErsRequest request;
    if(item==0){request.type=255;request.id=1;request.command=0;request.value=255;ersSelected_=-1;ersListBank_=0;}
    else if((item==3 || item==8) && ersSelected_>=0){
        const auto &d=ersConfig_.device(ersSelected_);request.type=d.type;request.id=d.id;
        request.command=item==3?12:d.type==3?15:d.type==8?10:d.type==4?17:255;
        request.value=item==3?255:ersDraft_;
    }else return;
    if(portalErsRequest(request)){ersPageMessage_[0]=0;if(item!=8)ersDraftDirty_=false;}
    else snprintf(ersPageMessage_,sizeof(ersPageMessage_),"Use CUT with Link enabled; wait for pending operation.");
    playKeyClick();drawDesignPage(UiPage::ErsSensors);
}
