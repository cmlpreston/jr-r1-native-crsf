// Invoked only with disposable simulator preferences by the focused audit.
{
    assert(runRecords_&&telemetryHistory_&&widgetMetrics_);
    bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
    startupPreflightComplete_=true;hardwareRecovery_=false;
    armRequested_=false;designSafety_=OutputSafetyStatus::Cut;
    activeModelSlot_=0;runRecords_.get()->segment=7;
    sensorStatistics_[2].update(23456,1000,1000,true);
    speedStatistics_.update(683,1000,1000,true);
    runRecords_.get()->addGps(-370000000,1450000000,1000);
    timers_[0].settings.mode=TimerMode::Lap;timers_[0].settings.minimumLapSeconds=1;
    timers_[0].resume(0);assert(timers_[0].recordLap(1500));
    const auto settings=timers_[0].settings;
    assert(saveRunRecords());
    sensorStatistics_[2]=SensorStatistics{};speedStatistics_=SpeedStatistics{};
    timers_[0]=RaceTimer{};timers_[0].settings=settings;gpsCount_=0;
    designTelemetry_.rpm=99999;designTelemetry_.rpmUpdatedMs=2000;designTelemetry_.rpmValid=true;
    loadRunRecords();sensorStatistics_[2].update(99999,2000,2000,true);
    assert(sensorStatistics_[2].result(true,millis()).maximum==23456);
    assert(!sensorStatistics_[2].result(false,millis()).valid);
    assert(speedStatistics_.maximum()==683&&gpsCount_==1);
    assert(!timers_[0].running()&&!timers_[0].ready()&&timers_[0].lap(0)==1500);
    assert(!armRequested_);
    const auto activate=PageControlEvent::Activate;
    drawDesignPage(UiPage::RunReview);v2ReviewConfirm_=false;
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
    simulatorSaveScreenshot("/tmp/r1-records-landscape.bmp");
    controlV2(52,activate); // No confirmation: must not erase.
    auto checkpoint=std::unique_ptr<RunRecords>(new RunRecords{});
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint)));
    controlV2(50,activate);assert(v2ReviewConfirm_);
    armRequested_=true;controlV2(52,activate);assert(armRequested_);
    assert(!saveRunRecords()&&armRequested_); // Flash writes cannot run during a drive session.
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint)));
    armRequested_=false;timers_[0].resume(millis());
    controlV2(50,activate);controlV2(52,activate);
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint)));
    timers_[0].pause(millis());
    assert(RunRecordStore::write(1,checkpoint.get(),sizeof(*checkpoint)));
    controlV2(50,activate);controlV2(51,activate);
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint)));
    controlV2(50,activate);controlV2(52,activate);
    assert(!RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint)));
    assert(RunRecordStore::read(1,checkpoint.get(),sizeof(*checkpoint)));
    assert(!sensorStatistics_[2].result(true,millis()).valid&&!speedStatistics_.available());
    assert(gpsCount_==0&&runRecords_.get()->gpsCount==0&&timers_[0].lapCount()==0);
    assert(!armRequested_&&!timers_[0].running());
    loadRunRecords();assert(gpsCount_==0&&!sensorStatistics_[2].result(true,millis()).valid);
    assert(requestDisplayRotation());applyDisplayRotation();drawDesignPage(UiPage::RunReview);
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);simulatorSaveScreenshot("/tmp/r1-records-portrait.bmp");
    Serial.println("Run records UI: checkpoint/reload, stale history, paused laps, confirmation, armed/running delete guards, cancel, model isolation and rotation PASS");
    return true;
}
