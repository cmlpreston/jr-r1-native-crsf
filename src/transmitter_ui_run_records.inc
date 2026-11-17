// UI owner only. Flash I/O never runs inside the input/control/RF tasks.
void TransmitterUi::loadRunRecords()
{
    runRecordsLoaded_=true;runRecordsHadLive_=false;runRecordsNextSaveMs_=millis()+30000;
    runRecordsGpsStamp_=designTelemetry_.gpsUpdatedMs;
    if(!runRecords_.initialize()){snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"Records unavailable: memory");return;}
    auto invalidateLive=[&](){
        if(telemetryHistory_)for(unsigned i=0;i<DashboardSensorCount;++i){
            int32_t unused=0;uint32_t stamp=0;sensorReading(designTelemetry_,i,unused,stamp);
            sensorStatistics_[i].resumeAfterPowerCycle(stamp);
        }
        speedStatistics_.resumeAfterPowerCycle(designTelemetry_.gpsSpeedUpdatedMs);
        if(widgetMetrics_){auto &m=*widgetMetrics_.get();
            m.power.resumeAfterPowerCycle(designTelemetry_.ersBatteryUpdatedMs,designTelemetry_.gpsSpeedUpdatedMs);
            m.tripRun.resumeAfterPowerCycle(millis(),designTelemetry_.ersBatteryUpdatedMs,designTelemetry_.gpsSpeedUpdatedMs);
            m.batteryLoad.resumeAfterPowerCycle(designTelemetry_.ersBatteryUpdatedMs,designTelemetry_.vehicleCellsUpdatedMs);
        }
        timers_[activeModelSlot_].resumeAfterPowerCycle(millis());
    };
    auto &r=*runRecords_.get();r.clear();
    if(!RunRecordStore::read(activeModelSlot_,&r,sizeof(r))||!r.valid()){
        r.clear();invalidateLive();snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"No saved records for this model");return;
    }
    r.resumeAfterPowerCycle(millis());
    if(telemetryHistory_)for(unsigned i=0;i<DashboardSensorCount;++i)sensorStatistics_[i]=r.sensors[i];
    speedStatistics_=r.speed;
    if(widgetMetrics_){auto &m=*widgetMetrics_.get();m.power=r.power;m.tripRun=r.trip;m.batteryLoad=r.battery;}
    // Current timer settings are authoritative. Only observations are restored;
    // the timer is always paused and auto-start is never resumed.
    const auto settings=timers_[activeModelSlot_].settings;
    timers_[activeModelSlot_]=r.timer;timers_[activeModelSlot_].settings=settings;
    invalidateLive();
    gpsCount_=min<unsigned>(48,r.gpsCount);
    for(unsigned i=0;i<gpsCount_;++i){const auto index=gpsCount_>1?i*(r.gpsCount-1)/(gpsCount_-1):0;
        gpsLat_[i]=r.gps[index].latitude;gpsLon_[i]=r.gps[index].longitude;}
    ++gpsTrailRevision_;
    snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"Saved records restored; timer paused");
}
bool TransmitterUi::saveRunRecords()
{
    if(armRequested_||designSafety_==OutputSafetyStatus::Armed||SetupPortal::firmwareUpdating()){
        runRecordsNextSaveMs_=millis();snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"Save queued until CUT");return false;
    }
    if(!runRecords_||!telemetryHistory_||!widgetMetrics_)return false;
    auto &r=*runRecords_.get();const auto now=millis();
    for(unsigned i=0;i<DashboardSensorCount;++i)r.sensors[i]=sensorStatistics_[i];
    r.speed=speedStatistics_;const auto &m=*widgetMetrics_.get();r.power=m.power;r.trip=m.tripRun;r.trip.checkpointClock(now);r.battery=m.batteryLoad;
    r.timer=timers_[activeModelSlot_];
    const bool saved=RunRecordStore::write(activeModelSlot_,&r,sizeof(r));
    snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),saved?"Saved / %u GPS points; 30s checkpoints in CUT":"Save failed; check handset storage",unsigned(r.gpsCount));
    return saved;
}
void TransmitterUi::updateRunRecords(uint32_t now)
{
    if(!runRecordsLoaded_)loadRunRecords();
    if(runRecords_&&gpsPositionAvailable(designTelemetry_,now)&&designTelemetry_.gpsUpdatedMs!=runRecordsGpsStamp_){
        runRecordsGpsStamp_=designTelemetry_.gpsUpdatedMs;
        runRecords_.get()->addGps(designTelemetry_.latitudeE7,designTelemetry_.longitudeE7,now);
    }
    const bool live=designSafety_==OutputSafetyStatus::Armed;
    const bool cut=runRecordsHadLive_&&!live;runRecordsHadLive_=live;
    if(!runRecords_||live||armRequested_||SetupPortal::firmwareUpdating()||(int32_t(now-runRecordsNextSaveMs_)<0&&!cut))return;
    // Empty sessions do not create files merely because the UI is ticking.
    bool haveData=runRecords_.get()->gpsCount||timers_[activeModelSlot_].lapCount()||timers_[activeModelSlot_].elapsed();
    for(unsigned i=0;telemetryHistory_&&!haveData&&i<DashboardSensorCount;++i)haveData=sensorStatistics_[i].result(true,now).valid;
    if(haveData)saveRunRecords();
    runRecordsNextSaveMs_=now+30000;
}
bool TransmitterUi::deleteRunRecords()
{
    if(armRequested_||designSafety_==OutputSafetyStatus::Armed||timers_[activeModelSlot_].running())return false;
    if(!RunRecordStore::erase(activeModelSlot_)){snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"Delete failed; records retained");return false;}
    for(unsigned i=0;i<DashboardSensorCount;++i)resetSensorStatistics(i);
    resetSpeedStatistics();resetWidgetPowerMetrics();resetTripRunMetrics();resetBatteryLoadMetrics();
    auto &timer=timers_[activeModelSlot_];timer.reset(millis());timer.clearLaps();
    gpsCount_=0;++gpsTrailRevision_;
    if(runRecords_)runRecords_.get()->clear();
    runRecordsNextSaveMs_=millis()+30000;
    snprintf(runRecordsNotice_,sizeof(runRecordsNotice_),"This model's records deleted");return true;
}
