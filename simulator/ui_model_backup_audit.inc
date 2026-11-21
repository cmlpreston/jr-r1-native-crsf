SetupPortal::start();
assert(SetupPortal::canControl());
bootPinUnlocked_=true;
pinPurpose_=PinPurpose::None;
startupPreflightComplete_=true;
armRequested_=false;
designSafety_=OutputSafetyStatus::Cut;
latestRawAdc_=triggerInput_.centre();
const uint8_t original=activeModelSlot_;
const uint8_t destination=kModelMemoryCount-1;
assert(destination!=original);
const uint32_t revision=portalModelRevision();
const uint8_t originalSteering=steeringChannel();

ModelBackup backup;
strcpy(backup.name,"Track R1");
backup.roles.steeringChannel=4;
backup.roles.throttleChannel=6;
backup.channels.responses[5].reversed=true;
backup.channels.parameters[5].trimPermille=-25;
backup.walkLimitPercent=30;
auto &binding=backup.bindings[backup.bindingCount++];
binding.input=PhysicalControlId::IdleUpSwitch;
binding.event=InputEvent::PositionHigh;
binding.target=AssignmentTarget::CrsfChannel;
binding.behaviour=ControlBehaviour::TwoPosition;
binding.crsfChannel=8;
uint8_t file[kModelBackupSize];
assert(encodeModelBackup(backup,file,sizeof(file)));
assert(!portalImportModel(original,file,sizeof(file)));
armRequested_=true;
assert(!portalImportModel(destination,file,sizeof(file)));
armRequested_=false;
file[10]^=1;
assert(!portalImportModel(destination,file,sizeof(file)));
file[10]^=1;
assert(portalModelRevision()==revision && portalModelSlot()==original);

// The destination's old ABS verification must never follow new controls.
Preferences legacy;
char name[16];
BrakeAbsSettings old;
old.forwardBrakeVerified=old.enabled=true;
uint8_t absBytes[kBrakeAbsRecordSize];
assert(encodeBrakeAbs(old,absBytes,sizeof(absBytes)));
assert(legacy.begin(modelNamespace(destination,"jr-r1-abs","abs",name,sizeof(name)),false));
assert(legacy.putBytes("config",absBytes,sizeof(absBytes))==sizeof(absBytes));
legacy.end();
timers_[destination].settings.durationSeconds=149;
assert(portalImportModel(destination,file,sizeof(file)));
assert(portalModelRevision()!=revision && portalModelSlot()==original);
assert(steeringChannel()==originalSteering && !armRequested_);

loadModelMemorySettings();
assert(!strcmp(portalModelName(destination),"Track R1"));
assert(selectModelMemory(destination));
assert(steeringChannel()==4 && throttleChannel()==6 && throttleProfile().reversed);
assert(driveModeSettings_.walkLimitPercent==30 && timers_[destination].settings.durationSeconds==149);
assert(!brakeAbs_.enabled && !brakeAbs_.forwardBrakeVerified);
assert(portalBinding(static_cast<uint8_t>(PhysicalControlId::IdleUpSwitch)).crsfChannel==8);
assert(portalExportModel(file,sizeof(file)));
assert(decodeModelBackup(file,sizeof(file),backup) && backup.channels.responses[5].reversed);

assert(portalRenameModel("Track R1 V2"));
assert(portalSetRole(0,5,true));
auto response=throttleProfile();response.positiveEndpointPercent=70;
assert(portalSetChannel(6,response,17,0));
brakeAbs_=old;
assert(saveBrakeAbs());
assert(selectModelMemory(original));
assert(steeringChannel()==originalSteering);
loadModelMemorySettings();
assert(selectModelMemory(destination));
assert(!strcmp(portalModelName(),"Track R1 V2") && steeringChannel()==5);
assert(throttleProfile().positiveEndpointPercent==70 && brakeAbs_.forwardBrakeVerified);

SetupPortal::stop();
assert(!SetupPortal::canControl());
using Action = ModelManagementAction;
const uint8_t copySlot = 6;
const uint8_t newSlot = 7;
const uint8_t renameSlot = 8;
const auto originalSlotName = std::string(portalModelName(original));
const auto untouchedSlotName = std::string(portalModelName(newSlot));
const auto managementRevision = portalModelRevision();
armRequested_ = true;
assert(!manageModelMemory(Action::Copy,copySlot,"Track setup"));
armRequested_ = false;
designSafety_ = OutputSafetyStatus::Armed;
assert(!manageModelMemory(Action::New,newSlot,"New buggy"));
designSafety_ = OutputSafetyStatus::Cut;
latestRawAdc_ = triggerInput_.centre()+ARM_NEUTRAL_ADC_WINDOW+1;
assert(!manageModelMemory(Action::New,newSlot,"New buggy"));
latestRawAdc_ = triggerInput_.centre();
assert(!manageModelMemory(Action::Rename,renameSlot,""));
assert(!manageModelMemory(Action::Copy,destination,"Overwrite"));
assert(!manageModelMemory(Action::New,destination,"Overwrite"));
assert(portalModelRevision()==managementRevision);
assert(portalModelName(newSlot)==untouchedSlotName);

timers_[copySlot].settings.durationSeconds=172;
assert(manageModelMemory(Action::Copy,copySlot,"Track setup"));
assert(manageModelMemory(Action::New,newSlot,"New buggy"));
assert(!modelBackupInstalled_[renameSlot]);
assert(manageModelMemory(Action::Rename,renameSlot,"Spare car"));
assert(!modelBackupInstalled_[renameSlot]);
assert(manageModelMemory(Action::Rename,destination,"Track race"));
assert(!strcmp(portalModelName(),"Track race") && brakeAbs_.forwardBrakeVerified);
assert(portalModelSlot()==destination && steeringChannel()==5 && !armRequested_);

loadModelMemorySettings();
assert(!strcmp(portalModelName(renameSlot),"Spare car") && portalModelName(original)==originalSlotName);
assert(selectModelMemory(newSlot));
assert(!strcmp(portalModelName(),"New buggy") && steeringChannel()==1 && throttleChannel()==2);
assert(!throttleProfile().reversed && throttleProfile().positiveEndpointPercent==100);
assert(!brakeAbs_.forwardBrakeVerified);
assert(selectModelMemory(copySlot));
assert(!strcmp(portalModelName(),"Track setup") && steeringChannel()==5 && throttleChannel()==6);
assert(throttleProfile().reversed && throttleProfile().positiveEndpointPercent==70);
assert(!brakeAbs_.enabled && !brakeAbs_.forwardBrakeVerified);
assert(timers_[copySlot].settings.durationSeconds==172);
assert(selectModelMemory(destination));
assert(!strcmp(portalModelName(),"Track race") && brakeAbs_.forwardBrakeVerified);

auto &timer = timers_[destination];
timer.settings.mode = TimerMode::Lap;
timer.settings.minimumLapSeconds = 1;
const auto lapEnd = millis();
timer.resume(lapEnd-1500);
assert(timer.recordLap(lapEnd));
timer.pause(lapEnd);
assert(saveRunRecords());

// Reset is a repair path for an active slot whose stored configuration fails.
designSafety_ = OutputSafetyStatus::ConfigurationFault;
modelStorageFault_ = modelBackupReadFailed_ = true;
settingsSavePending_ = channelParameterSavePending_ = true;
bindingsChanged_ = channelRolesChanged_ = false;
safetyResetRequested_ = false;
assert(manageModelMemory(Action::Reset,destination,nullptr));
assert(portalModelSlot()==destination && !strcmp(portalModelName(),"Track race"));
assert(!armRequested_ && !modelStorageFault_ && !modelBackupReadFailed_);
assert(!settingsSavePending_ && !channelParameterSavePending_);
assert(bindingsChanged_ && channelRolesChanged_ && safetyResetRequested_);
assert(steeringChannel()==1 && throttleChannel()==2 && !brakeAbs_.forwardBrakeVerified);
assert(timer.settings.durationSeconds==149 && timer.lapCount()==1);
designSafety_ = OutputSafetyStatus::Cut;
assert(selectModelMemory(copySlot));
assert(selectModelMemory(destination));
assert(steeringChannel()==1 && !throttleProfile().reversed && !brakeAbs_.forwardBrakeVerified);
assert(timer.lapCount()==1 && timer.lap(0)==1500);

// Drive the same touch targets that the handset uses, including the separate
// Back and Apply steps. Check both layout modes with the same active model.
using ManagementControl = ModelManagementControl;
using ManagementStage = ModelManagementStage;
auto fitManagementTargets = [&] {
    for (uint8_t i=0; i<pageFocusCount_; ++i) {
        const auto &target = pageFocusTargets_[i];
        if (!target.enabled) continue;
        const auto &area = target.activate;
        assert(area.x>=0 && area.y>=0 && area.x+area.width<=kScreenWidth &&
            area.y+area.height<=kScreenHeight);
        for (uint8_t j=0; j<i; ++j) {
            if (!pageFocusTargets_[j].enabled) continue;
            const auto &other = pageFocusTargets_[j].activate;
            assert(area.x+area.width<=other.x || other.x+other.width<=area.x ||
                area.y+area.height<=other.y || other.y+other.height<=area.y);
        }
    }
};
auto touchManagement = [&](ManagementControl action) {
    for (uint8_t index=0; index<pageFocusCount_; ++index) {
        const auto &target = pageFocusTargets_[index];
        if (target.handler!=&TransmitterUi::controlModelManagement ||
            target.argument!=uint8_t(action) || !target.enabled) continue;
        const auto &area = target.activate;
        lastTouchDurationMs_ = 100;
        handleTouch(area.x+area.width/2,area.y+area.height/2);
        return;
    }
    assert(false && "Expected a reachable model management action");
};
for (uint8_t orientation=0; orientation<2; ++orientation) {
    const uint8_t editedSlot = 2+orientation;
    const auto slotTarget = ManagementControl(uint8_t(ManagementControl::FirstSlot)+editedSlot);
    const auto before = std::string(portalModelName(editedSlot));
    drawModelSelectPage();
    touchManagement(ManagementControl::Open);
    assert(activePage_==UiPage::ModelManagement && modelManagementStage_==ManagementStage::Actions);
    fitManagementTargets();
    touchManagement(ManagementControl::New);
    fitManagementTargets();
    touchManagement(slotTarget);
    assert(modelManagementStage_==ManagementStage::Name);
    fitManagementTargets();
    touchManagement(ManagementControl::Clear);
    touchManagement(ManagementControl::FirstKey);
    const auto draft = std::string(modelManagementName_);
    assert(draft.size()==1 && portalModelName(editedSlot)==before);
    touchManagement(ManagementControl::NameContinue);
    assert(modelManagementStage_==ManagementStage::Confirm);
    fitManagementTargets();
    navigateBack();
    assert(modelManagementStage_==ManagementStage::Name && modelManagementName_==draft);
    touchManagement(ManagementControl::NameContinue);
    touchManagement(ManagementControl::Apply);
    assert(portalModelName(editedSlot)==draft && portalModelSlot()==destination && !armRequested_);
    assert(requestDisplayRotation());
    applyDisplayRotation();
}
Serial.println("Model profiles: handset and portal slot guards, touch navigation, copy/new/rename/reset, CUT, ABS and durable state PASS");
return true;
