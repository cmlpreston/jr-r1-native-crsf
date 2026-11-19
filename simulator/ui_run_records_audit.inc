// Invoked only with disposable simulator preferences by the focused audit.
{
    using RunRecordStore::ReadResult;
    assert(runRecords_&&telemetryHistory_&&widgetMetrics_);
    bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
    startupPreflightComplete_=true;hardwareRecovery_=false;
    armRequested_=false;designSafety_=OutputSafetyStatus::Cut;
    activeModelSlot_=0;runRecords_.get()->segment=7;
    sensorStatistics_[2].update(23456,1000,1000,true);
    speedStatistics_.update(683,1000,1000,true);
    runRecords_.get()->addGps(-370000000,1450000000,1000);
    timers_[0].settings.mode=TimerMode::Lap;timers_[0].settings.minimumLapSeconds=1;
    const auto timerNow = millis();
    timers_[0].resume(timerNow - 1500);assert(timers_[0].recordLap(timerNow));
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
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint))==ReadResult::Loaded);
    controlV2(50,activate);assert(v2ReviewConfirm_);
    armRequested_=true;controlV2(52,activate);assert(armRequested_);
    assert(!saveRunRecords()&&armRequested_); // Flash writes cannot run during a drive session.
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint))==ReadResult::Loaded);
    armRequested_=false;timers_[0].resume(millis());
    controlV2(50,activate);controlV2(52,activate);
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint))==ReadResult::Loaded);
    timers_[0].pause(millis());
    assert(RunRecordStore::write(1,checkpoint.get(),sizeof(*checkpoint)));
    controlV2(50,activate);controlV2(51,activate);
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint))==ReadResult::Loaded);
    controlV2(50,activate);controlV2(52,activate);
    assert(RunRecordStore::read(0,checkpoint.get(),sizeof(*checkpoint))==ReadResult::Missing);
    assert(RunRecordStore::read(1,checkpoint.get(),sizeof(*checkpoint))==ReadResult::Loaded);
    assert(!sensorStatistics_[2].result(true,millis()).valid&&!speedStatistics_.available());
    assert(gpsCount_==0&&runRecords_.get()->gpsCount==0&&timers_[0].lapCount()==0);
    assert(!armRequested_&&!timers_[0].running());
    loadRunRecords();assert(gpsCount_==0&&!sensorStatistics_[2].result(true,millis()).valid);

    // Speed can be the only observation. Both entry points must commit the
    // reset even when it empties the session, without writing during LIVE.
    speedStatistics_.update(683, 3000, 3000, true);
    assert(saveRunRecords());
    drawDashboard();
    controlDesign(148, activate);
    loadRunRecords();
    assert(!speedStatistics_.available());
    speedStatistics_.update(720, 4000, 4000, true);
    assert(saveRunRecords());
    armRequested_ = true;
    designSafety_ = OutputSafetyStatus::Armed;
    assert(applyTransmitterFunctionAction(static_cast<uint16_t>(TransmitterFunction::ResetSpeedStatistics), 1));
    updateRunRecords(millis());
    assert(runRecordsSavePending_ && !hasRunRecordData(millis()));
    assert(RunRecordStore::read(0, checkpoint.get(), sizeof(*checkpoint)) == ReadResult::Loaded);
    assert(checkpoint->speed.maximum() == 720);
    armRequested_ = false;
    designSafety_ = OutputSafetyStatus::Cut;
    updateRunRecords(millis());
    assert(!runRecordsSavePending_);
    loadRunRecords();
    assert(!speedStatistics_.available());

#ifndef __EMSCRIPTEN__
    const auto record = std::filesystem::path(getenv("JR_R1_SIM_STATE")) / "run-records" / "model-0.dat";
    const auto pending = record.string() + ".pending";
    speedStatistics_.update(730, 5000, 5000, true);
    assert(saveRunRecords());
    speedStatistics_.update(750, 6000, 6000, true);
    assert(std::filesystem::create_directory(pending));
    latestRawAdc_ = triggerInput_.centre();
    modelSelectBank_ = 0;
    drawModelSelectPage();
    controlDesign(101, activate); // Normal model-selection action.
    assert(activeModelSlot_ == 0 && modelSelectPendingSlot_ == 1);
    assert(speedStatistics_.maximum() == 750);
    assert(RunRecordStore::read(0, checkpoint.get(), sizeof(*checkpoint)) == ReadResult::Loaded);
    assert(checkpoint->speed.maximum() == 730);
    controlDesign(167, activate); // Stay here.
    assert(activeModelSlot_ == 0 && modelSelectPendingSlot_ == kModelMemoryCount);
    controlDesign(101, activate);
    assert(std::filesystem::remove(pending));
    controlDesign(166, activate); // Retry save and change model.
    assert(activeModelSlot_ == 1);
    assert(RunRecordStore::read(0, checkpoint.get(), sizeof(*checkpoint)) == ReadResult::Loaded);
    assert(checkpoint->speed.maximum() == 750);

    { std::ofstream damaged(record, std::ios::binary | std::ios::trunc); damaged << "corrupt"; }
    controlDesign(100, activate);
    assert(activeModelSlot_ == 0 && runRecordsReadError_);
    speedStatistics_.update(760, 7000, 7000, true);
    const auto discardLapNow = millis();
    timers_[0].settings.onSeconds = 123;
    timers_[0].resume(discardLapNow - 1500);
    assert(timers_[0].recordLap(discardLapNow));
    runRecordsNextSaveMs_ = millis();
    updateRunRecords(millis());
    drawDesignPage(UiPage::RunReview);
    controlV2(53, activate); // Manual save must preserve the unreadable file too.
    assert(std::filesystem::file_size(record) == 7);
    drawModelSelectPage();
    controlDesign(101, activate);
    assert(activeModelSlot_ == 0 && modelSelectPendingSlot_ == 1);
    assert(timers_[0].lapCount() == 1 && timers_[0].running());
    controlDesign(168, activate); // Explicitly discard unsaved observations.
    assert(activeModelSlot_ == 1 && std::filesystem::file_size(record) == 7);
    controlDesign(100, activate);
    assert(timers_[0].lapCount() == 0 && timers_[0].elapsed() == 0);
    assert(timers_[0].settings.onSeconds == 123);
    drawDesignPage(UiPage::RunReview);
    controlV2(50, activate);controlV2(52, activate);
    assert(!runRecordsReadError_ && saveRunRecords());
#endif

    assert(requestDisplayRotation());applyDisplayRotation();drawDesignPage(UiPage::RunReview);
    lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);simulatorSaveScreenshot("/tmp/r1-records-portrait.bmp");
    Serial.println("Run records UI: reload, queued resets, failed model checkpoint, corrupt file protection, deletion and rotation PASS");
    return true;
}
