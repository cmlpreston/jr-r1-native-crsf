void TransmitterUi::loadDriveBatterySettings(){
    loadRfWarningSettings();
    Preferences p;char ns[16];p.begin(modelNamespace(activeModelSlot_,"jr-r1-pack","pack",ns,sizeof(ns)),true);
    driveBatterySettings_=DriveBatterySettings{};auto &s=driveBatterySettings_;
    s.chemistry=constrain(p.getUChar("chem",1),0,3);s.cells=constrain(p.getUChar("cells",4),1,12);
    s.packMv=constrain(p.getUShort("pack_mv",14000),1000,60000);s.cellMv=constrain(p.getUShort("cell_mv",3500),800,4400);
    s.capacityMah=constrain(p.getUShort("capacity",6500),100,65000);
    s.packEnabled=p.getBool("pack_on",true);s.cellEnabled=p.getBool("cell_on",true);
    for(unsigned n=0;n<DashboardSensorCount;++n){char key[12];snprintf(key,sizeof(key),"stat%u",n);sensorSessionStats_[n]=p.getBool(key,n==0||n==2);}
    p.end();drivePackAlarm_=DriveBatteryAlarm{};driveCellAlarm_=DriveBatteryAlarm{};
}
void TransmitterUi::saveDriveBatterySettings(){
    Preferences p;char ns[16];p.begin(modelNamespace(activeModelSlot_,"jr-r1-pack","pack",ns,sizeof(ns)),false);
    const auto &s=driveBatterySettings_;p.putUChar("chem",s.chemistry);p.putUChar("cells",s.cells);
    p.putUShort("pack_mv",s.packMv);p.putUShort("cell_mv",s.cellMv);p.putUShort("capacity",s.capacityMah);
    p.putBool("pack_on",s.packEnabled);p.putBool("cell_on",s.cellEnabled);
    for(unsigned n=0;n<DashboardSensorCount;++n){char key[12];snprintf(key,sizeof(key),"stat%u",n);p.putBool(key,sensorSessionStats_[n]);}
    p.end();
}
void TransmitterUi::controlDriveBattery(uint8_t row,PageControlEvent event){
    const int direction=event==PageControlEvent::Decrease?-1:1;auto &s=driveBatterySettings_;
    if(row==0){s.chemistry=(s.chemistry+direction+4)%4;const uint16_t defaults[]={3000,3500,1000,3200};s.cellMv=defaults[s.chemistry];s.packMv=s.cells*s.cellMv;}
    if(row==1){s.cells=constrain(int(s.cells)+direction,1,12);s.packMv=s.cells*s.cellMv;}
    if(row==2)s.packMv=constrain(int(s.packMv)+direction*100,1000,60000);
    if(row==3)s.cellMv=constrain(int(s.cellMv)+direction*50,800,4400);
    if(row==4)s.packEnabled=!s.packEnabled;
    if(row==5)s.cellEnabled=!s.cellEnabled;
    if(row==6)s.capacityMah=constrain(int(s.capacityMah)+direction*100,100,65000);
    saveDriveBatterySettings();drawDesignPage(UiPage::DriveBatterySetup);
}
void TransmitterUi::updateDriveBatteryWarning(const CrsfTelemetryState &t,uint32_t now){
    const bool cells=t.vehicleCellsValid && uint32_t(now-t.vehicleCellsUpdatedMs)<=3000;
    const bool ers=t.ersBatteryValid && uint32_t(now-t.ersBatteryUpdatedMs)<=3000;
    const bool fas=t.fasVoltageValid && uint32_t(now-t.fasVoltageUpdatedMs)<=3000;
    const uint32_t pack=cells?t.vehiclePackMv:ers?uint32_t(t.ersVoltageDeciV)*100:fas?t.fasVoltageMv:0;
    const auto &s=driveBatterySettings_;
    const bool packCue=drivePackAlarm_.update(s.packEnabled&&(cells||ers||fas),pack,s.packMv,400,now);
    const bool cellCue=driveCellAlarm_.update(s.cellEnabled&&cells,t.vehicleMinimumCellMv,s.cellMv,100,now);
    if(packCue||cellCue)playBuzzer(300);
    const bool show=(drivePackAlarm_.active()||driveCellAlarm_.active())&&bootPinUnlocked_&&pinPurpose_==PinPurpose::None&&!displaySleeping_;
    if(!show){if(driveBatteryWarning_)uiSetHidden(driveBatteryWarning_,true);return;}
    if(!driveBatteryWarning_){
        driveBatteryWarning_=makePanel(12,44,kScreenWidth-24,26,cPanel2,cRed);
        driveBatteryWarningText_=makeLabel(driveBatteryWarning_,"",8,4,UiText::Caption,cRed);
        lv_obj_set_width(driveBatteryWarningText_,kScreenWidth-40);lv_label_set_long_mode(driveBatteryWarningText_,LV_LABEL_LONG_CLIP);
        lv_obj_clear_flag(driveBatteryWarning_,LV_OBJ_FLAG_CLICKABLE);lv_obj_clear_flag(driveBatteryWarningText_,LV_OBJ_FLAG_CLICKABLE);
    }
    if(driveCellAlarm_.active())uiSetTextFmt(driveBatteryWarningText_,"LOW CELL %u.%02u V | Pack %u.%1u V",t.vehicleMinimumCellMv/1000,(t.vehicleMinimumCellMv%1000)/10,pack/1000,(pack%1000)/100);
    else uiSetTextFmt(driveBatteryWarningText_,"LOW DRIVE PACK %u.%1u V",pack/1000,(pack%1000)/100);
    uiSetHidden(driveBatteryWarning_,false);lv_obj_move_foreground(driveBatteryWarning_);
}
