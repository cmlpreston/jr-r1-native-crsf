void TransmitterUi::loadRfWarningSettings(){
    Preferences p;char ns[16];p.begin(modelNamespace(activeModelSlot_,"jr-r1-rfwarn","rfwarn",ns,sizeof(ns)),true);
    auto &s=rfWarningSettings_;s.enabled=p.getBool("enabled",true);
    s.elrsMajor=p.getUChar("major",4)==3?3:4;s.marginDb=constrain(p.getUChar("margin",8),5,20);
    s.minimumLq=constrain(p.getUChar("lq",90),50,99);p.end();
    rfRssiWarning_=RfWarningLatch{};rfLqWarning_=RfWarningLatch{};rfTelemetryWarning_=RfWarningLatch{};rfTelemetrySeen_=false;
}
void TransmitterUi::saveRfWarningSettings(){
    Preferences p;char ns[16];p.begin(modelNamespace(activeModelSlot_,"jr-r1-rfwarn","rfwarn",ns,sizeof(ns)),false);
    const auto &s=rfWarningSettings_;p.putBool("enabled",s.enabled);p.putUChar("major",s.elrsMajor);
    p.putUChar("margin",s.marginDb);p.putUChar("lq",s.minimumLq);p.end();
}
void TransmitterUi::controlRfWarning(uint8_t row,PageControlEvent event){
    const int direction=event==PageControlEvent::Decrease?-1:1;auto &s=rfWarningSettings_;
    if(row==0)s.enabled=!s.enabled;
    if(row==1)s.elrsMajor=s.elrsMajor==4?3:4;
    if(row==2)s.marginDb=constrain(int(s.marginDb)+direction,5,20);
    if(row==3)s.minimumLq=constrain(int(s.minimumLq)+direction,50,99);
    saveRfWarningSettings();rfRssiWarning_=RfWarningLatch{};rfLqWarning_=RfWarningLatch{};rfTelemetryWarning_=RfWarningLatch{};
    drawDesignPage(UiPage::RfWarningSetup);
}
void TransmitterUi::updateRfWarning(const CrsfTelemetryState &t,uint32_t now){
    const auto &s=rfWarningSettings_;
    const bool fresh=t.linkValid && uint32_t(now-t.linkUpdatedMs)<=1500;
    if(fresh)rfTelemetrySeen_=true;
    const bool enabled=s.enabled && armRequested_ && bootPinUnlocked_ && pinPurpose_==PinPurpose::None && !hardwareRecovery_;
    const int limit=elrs24Sensitivity(t.rfMode,s.elrsMajor),margin=t.uplinkRssiDbm-limit;
    const bool rssiCue=rfRssiWarning_.update(enabled&&fresh&&limit!=0,margin<=s.marginDb,margin>=s.marginDb+3,now);
    const bool lqCue=rfLqWarning_.update(enabled&&fresh,t.uplinkLinkQuality<=s.minimumLq,t.uplinkLinkQuality>=min(100,int(s.minimumLq)+5),now);
    const bool lostCue=rfTelemetryWarning_.update(enabled&&rfTelemetrySeen_,!fresh,fresh,now);
    if(rssiCue||lqCue||lostCue)playBuzzer(250);
    const bool show=(rfRssiWarning_.active()||rfLqWarning_.active()||rfTelemetryWarning_.active())&&!displaySleeping_;
    if(!show){if(rfWarningPanel_)uiSetHidden(rfWarningPanel_,true);return;}
    if(!rfWarningPanel_){
        rfWarningPanel_=makePanel(12,72,kScreenWidth-24,26,cPanel2,cAmber);
        rfWarningText_=makeLabel(rfWarningPanel_,"",8,4,UiText::Caption,cAmber);
        lv_obj_set_width(rfWarningText_,kScreenWidth-40);lv_label_set_long_mode(rfWarningText_,LV_LABEL_LONG_CLIP);
        lv_obj_clear_flag(rfWarningPanel_,LV_OBJ_FLAG_CLICKABLE);lv_obj_clear_flag(rfWarningText_,LV_OBJ_FLAG_CLICKABLE);
    }
    if(rfTelemetryWarning_.active())uiSetText(rfWarningText_,"RF TELEMETRY LOST - signal data unavailable");
    else if(limit)uiSetTextFmt(rfWarningText_,"RF LOW: margin %d dB | LQ %u%%",margin,t.uplinkLinkQuality);
    else uiSetTextFmt(rfWarningText_,"RF LOW LQ %u%% | unknown RF mode",t.uplinkLinkQuality);
    uiSetHidden(rfWarningPanel_,false);lv_obj_move_foreground(rfWarningPanel_);
}
