// SDL-only comparison against the same pages with their full rebuild path.
{
    bootPinUnlocked_ = true;
    pinPurpose_ = PinPurpose::None;
    startupPreflightComplete_ = true;
    hardwareRecovery_ = false;
    armRequested_ = false;
    designSafety_ = OutputSafetyStatus::Cut;
    designTelemetry_ = CrsfTelemetryState{};
    sensorBank_ = designBank_ = 0;
    constexpr UiPage pages[] = {UiPage::DisplaySetup, UiPage::TxBatterySetup,
        UiPage::ClockSetup, UiPage::DisplaySetup, UiPage::Telemetry,
        UiPage::ChannelMonitor, UiPage::Telemetry};
    constexpr bool shared[] = {false, true, true, true, false, true, true};
    constexpr size_t pageCount = sizeof(pages) / sizeof(*pages);
    constexpr unsigned rounds = 7;
    const auto countTree = [](auto &&self, lv_obj_t *root) -> uint32_t {
        uint32_t count = 1;
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(root); ++i) {
            count += self(self, lv_obj_get_child(root, i));
        }
        return count;
    };

    for (unsigned orientation = 0; orientation < 2; ++orientation) {
        struct Totals {
            uint32_t objects = 0, kept = 0, rendered = 0, sent = 0;
            uint32_t buildUs = 0, layoutUs = 0, flushUs = 0, refreshUs = 0;
        } totals[2];
        const size_t pixels = size_t(kScreenWidth) * kScreenHeight;
        std::vector<uint32_t> expected[pageCount];
        std::vector<uint32_t> frame(pixels);
        for (unsigned mode = 0; mode < 2; ++mode) {
            auditForceChromeRebuild = mode == 0;
            auto &total = totals[mode];
            for (unsigned round = 0; round < rounds; ++round) {
                drawDashboard();
                lv_refr_now(nullptr);
                for (size_t step = 0; step < pageCount; ++step) {
                    const auto lastEpoch = SetupPortal::screenEpochForTesting();
                    auto *previousLink = designLink_;
                    auto *previousDrive = driveIcon_;
                    pageFocusIndex_ = 3;
                    drawDesignPage(pages[step]);
                    assert(SetupPortal::screenEpochForTesting() != lastEpoch);
                    assert(pageFocusIndex_ == 0 && pageFocusCount_ > 0);
                    updateUiLayout(lv_scr_act());
                    const uint32_t refreshStarted = micros();
                    lv_refr_now(nullptr);
                    total.refreshUs += uint32_t(micros()) - refreshStarted;
                    total.buildUs += uiMetrics.buildUs;
                    total.layoutUs += uiMetrics.layoutUs;
                    total.flushUs += uiMetrics.flushUs;
                    total.rendered += uiMetrics.rendered;
                    total.sent += uiMetrics.sent;

                    uint32_t kept = 0;
                    auto *screen = lv_scr_act();
                    if (mode && shared[step]) {
                        assert(designLink_ == previousLink && driveIcon_ == previousDrive);
                        for (uint32_t i = 0; i < lv_obj_get_child_cnt(screen); ++i) {
                            auto *child = lv_obj_get_child(screen, i);
                            if (lv_obj_has_flag(child, kDesignChrome)) kept += countTree(countTree, child);
                        }
                        assert(kept > 0);
                    }
                    total.kept += kept;
                    total.objects += countTree(countTree, screen) - 1 - kept;
                    assert(simulatorCopyFramebuffer(frame.data(), frame.size()));
                    if (mode == 0 && round == 0) {
                        expected[step] = frame;
                    } else {
                        if (frame != expected[step]) {
                            size_t mismatch = 0;
                            while (frame[mismatch] == expected[step][mismatch]) ++mismatch;
                            Serial.printf("Chrome framebuffer mismatch: page=%u mode=%u x=%u y=%u\n",
                                unsigned(pages[step]), mode, unsigned(mismatch % kScreenWidth),
                                unsigned(mismatch / kScreenWidth));
                        }
                        assert(frame == expected[step]);
                    }
                }
            }
            Serial.printf("Chrome reuse simulator %s %s (%u transitions): rebuilt objects=%lu kept=%lu rendered=%lu sent=%lu build=%luus layout=%luus flush=%luus refresh=%luus\n",
                portraitUi ? "portrait" : "landscape", mode ? "retained" : "full",
                unsigned(rounds * pageCount), (unsigned long)total.objects,
                (unsigned long)total.kept, (unsigned long)total.rendered, (unsigned long)total.sent,
                (unsigned long)total.buildUs, (unsigned long)total.layoutUs,
                (unsigned long)total.flushUs, (unsigned long)total.refreshUs);
        }
        assert(totals[1].objects < totals[0].objects);
        assert(totals[1].rendered < totals[0].rendered);
        assert(totals[1].sent <= totals[0].sent);
        if (orientation == 0) {
            assert(requestDisplayRotation());
            applyDisplayRotation();
        }
    }
    Serial.println("Chrome reuse: identical frames, fresh page epochs and focus, fewer objects and rendered pixels PASS");
    return true;
}
