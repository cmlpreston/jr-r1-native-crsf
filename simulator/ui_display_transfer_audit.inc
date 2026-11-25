// The simulator has no ESP32 SPI device, but runs the actual UI-owned guards.
// Native/legacy byte equivalence and hardware subclass are tested with pinned
// library/HAL code by lcd_pixel_bus_test.py rather than by an SDL timing claim.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;startupPreflightComplete_=true;
displayReady_=lcdTransportReady=true;hardwareRecovery_=rfBenchMode_=false;
displaySleeping_=touchWasDown_=armGestureValid_=armRequested_=false;
designSafety_=OutputSafetyStatus::Cut;remoteArmHold_.cancel();
triggerInput_.cancelCalibration();steeringInput_.cancelCalibration();
drawDashboard();
const auto originalModel=channelModel_;
const auto originalPage=activePage_;
const auto originalScreen=lv_scr_act();
const auto originalCutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
const auto cuts=auditCutRequests;
safetyResetRequested_=false;
assert(displayTransferChangeAllowed());
const auto unavailable=LcdPixelTransfer::snapshot();
assert(!unavailable.supported&&!unavailable.native&&unavailable.pixelCalls==0);
assert(!setDisplayTransferMode(true)&&!setDisplayTransferMode(false)&&!resetDisplayTransferStats());
auto rejected=[&](){
    assert(!displayTransferChangeAllowed());
    assert(!setDisplayTransferMode(true)&&!setDisplayTransferMode(false)&&!resetDisplayTransferStats());
    assert(activePage_==originalPage&&lv_scr_act()==originalScreen);
    assert(memcmp(&channelModel_,&originalModel,sizeof(originalModel))==0);
    assert(auditCutRequests==cuts&&!safetyResetRequested_);
};
armRequested_=true;rejected();assert(armRequested_);armRequested_=false;
for(auto status:{OutputSafetyStatus::Armed,OutputSafetyStatus::StartupWarning,
        OutputSafetyStatus::LinkRequired,OutputSafetyStatus::NeutralRequired,
        OutputSafetyStatus::Calibration,OutputSafetyStatus::InputFault,
        OutputSafetyStatus::ControlStale,OutputSafetyStatus::ChannelDisabled,
        OutputSafetyStatus::ConfigurationFault}){
    designSafety_=status;rejected();
}
designSafety_=OutputSafetyStatus::Cut;
startupPreflightComplete_=false;rejected();startupPreflightComplete_=true;
displayReady_=false;rejected();displayReady_=true;
lcdTransportReady=false;rejected();lcdTransportReady=true;
hardwareRecovery_=true;rejected();hardwareRecovery_=false;
rfBenchMode_=true;rejected();rfBenchMode_=false;
bootPinUnlocked_=false;rejected();bootPinUnlocked_=true;
pinPurpose_=PinPurpose::ChangeCurrent;rejected();pinPurpose_=PinPurpose::None;
touchWasDown_=true;rejected();touchWasDown_=false;
armGestureValid_=true;rejected();armGestureValid_=false;
remoteArmHold_.begin(unsigned(activePage_),millis());rejected();remoteArmHold_.cancel();
displaySleeping_=true;rejected();displaySleeping_=false;
triggerInput_.startCalibration(2000);rejected();triggerInput_.cancelCalibration();
steeringInput_.startCalibration(2000);rejected();steeringInput_.cancelCalibration();
assert(displayTransferChangeAllowed()&&!armRequested_);
safetyCutHandler_=originalCutHandler;
Serial.println("Display transfer UI: CUT-only, startup/hold/PIN/input-fault/calibration/sleep/recovery vetoes, simulator unsupported and no output/page/model mutation PASS");
return true;
