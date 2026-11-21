namespace {
bool readStoredModelBackup(uint8_t slot, ModelBackup &backup, BrakeAbsSettings &abs)
{
    std::unique_ptr<uint8_t[]> bytes(new(std::nothrow) uint8_t[kStoredModelBackupSize]);
    return bytes && ChannelSettingsRepository::readModelBackup(slot, bytes.get(), kStoredModelBackupSize) &&
        decodeModelBackup(bytes.get(), kModelBackupSize, backup) &&
        decodeBrakeAbs(bytes.get() + kModelBackupSize, kBrakeAbsRecordSize, abs);
}

bool writeStoredModelBackup(uint8_t slot, const uint8_t *fileBytes, const BrakeAbsSettings &abs)
{
    std::unique_ptr<uint8_t[]> record(new(std::nothrow) uint8_t[kStoredModelBackupSize]);
    if (!record) return false;
    memcpy(record.get(), fileBytes, kModelBackupSize);
    if (!encodeBrakeAbs(abs, record.get() + kModelBackupSize, kBrakeAbsRecordSize)) return false;
    return ChannelSettingsRepository::writeModelBackup(slot, record.get(), kStoredModelBackupSize);
}

bool writeStoredModelBackup(uint8_t slot, const ModelBackup &model, const BrakeAbsSettings &abs)
{
    std::unique_ptr<uint8_t[]> bytes(new(std::nothrow) uint8_t[kModelBackupSize]);
    return bytes && encodeModelBackup(model, bytes.get(), kModelBackupSize) &&
        writeStoredModelBackup(slot, bytes.get(), abs);
}
}

void TransmitterUi::loadModelBackupNames()
{
    static_assert(kModelNameCapacity == kModelBackupNameCapacity, "Backup must fit a model name");
    std::unique_ptr<ModelBackup> backup(new(std::nothrow) ModelBackup);
    for (uint8_t slot = 0; slot < kModelMemoryCount; ++slot) {
        modelBackupInstalled_[slot] = ChannelSettingsRepository::hasModelBackup(slot);
        if (!modelBackupInstalled_[slot]) continue;
        BrakeAbsSettings abs;
        const bool read = backup && readStoredModelBackup(slot, *backup, abs);
        snprintf(modelNames_[slot], kModelNameCapacity, "%s", read ? backup->name : "Invalid model");
    }
}

void TransmitterUi::loadActiveModelControls()
{
    modelBackupReadFailed_ = false;
    if (!modelBackupInstalled_[activeModelSlot_]) {
        loadChannelRoleSettings();
        loadSettings();
        loadChannelParameters();
        loadBindings();
        return;
    }
    loadSettings(); // Physical steering/trigger calibration remains handset-wide.
    std::unique_ptr<ModelBackup> backup(new(std::nothrow) ModelBackup);
    brakeAbs_ = BrakeAbsSettings{};
    brakeAbsStorageValid_ = backup && readStoredModelBackup(activeModelSlot_, *backup, brakeAbs_);
    if (!brakeAbsStorageValid_) {
        modelStorageFault_ = modelBackupReadFailed_ = true;
        channelModel_ = ChannelModelController{};
        channelRoleSettings_ = ChannelRoleSettings{};
        driveModeSettings_.walkLimitPercent = DriveModeSettings{}.walkLimitPercent;
        restoreDefaultBindings();
        Serial.println("MODEL STORAGE: invalid imported configuration; outputs inhibited");
        return;
    }
    applyModelBackupControls(*backup);
}

void TransmitterUi::applyModelBackupControls(const ModelBackup &model)
{
    channelModel_ = model.channels;
    channelRoleSettings_ = model.roles;
    driveModeSettings_.walkLimitPercent = model.walkLimitPercent;
    bindingCount_ = model.bindingCount;
    for (size_t i = 0; i < bindingCount_; ++i) bindings_[i] = model.bindings[i];
}

void TransmitterUi::collectModelBackup(ModelBackup &model, const ChannelModelController &channels) const
{
    snprintf(model.name, sizeof(model.name), "%s", modelNames_[activeModelSlot_]);
    model.roles = channelRoleSettings_;
    model.channels = channels;
    model.bindingCount = bindingCount_;
    model.walkLimitPercent = driveModeSettings_.walkLimitPercent;
    for (size_t i = 0; i < bindingCount_; ++i) model.bindings[i] = bindings_[i];
}

bool TransmitterUi::persistModelBackup(const ChannelModelController &channels)
{
    if (modelBackupReadFailed_) return false;
    std::unique_ptr<ModelBackup> backup(new(std::nothrow) ModelBackup);
    if (!backup) return false;
    collectModelBackup(*backup, channels);
    return writeStoredModelBackup(activeModelSlot_, *backup, brakeAbs_);
}

bool TransmitterUi::portalExportModel(uint8_t *data, size_t size) const
{
    if (modelStorageFault_) return false;
    std::unique_ptr<ModelBackup> model(new(std::nothrow) ModelBackup);
    if (!model) return false;
    collectModelBackup(*model, channelModel_);
    return encodeModelBackup(*model, data, size);
}

bool TransmitterUi::portalImportModel(uint8_t slot, const uint8_t *data, size_t size)
{
    if (slot >= kModelMemoryCount || slot == activeModelSlot_ || !SetupPortal::canControl() ||
        armRequested_ || designSafety_ == OutputSafetyStatus::Armed || SetupPortal::firmwareUpdating())
        return false;
    std::unique_ptr<ModelBackup> model(new(std::nothrow) ModelBackup);
    if (!model || !decodeModelBackup(data, size, *model) || !installModelBackup(slot, data, model->name)) return false;
    if (activePage_ == UiPage::ModelSelect) drawModelSelectPage();
    return true;
}

bool TransmitterUi::installModelBackup(uint8_t slot, const uint8_t *bytes, const char *name)
{
    // Verification belongs to the actual ESC and must not survive a profile replacement.
    if (!writeStoredModelBackup(slot, bytes, BrakeAbsSettings{})) return false;
    modelBackupInstalled_[slot] = true;
    snprintf(modelNames_[slot], kModelNameCapacity, "%s", name);
    ++modelRevision_;
    return true;
}

bool TransmitterUi::renameModelMemory(uint8_t slot, const char *name)
{
    if (slot >= kModelMemoryCount || !validModelBackupName(name)) return false;
    if (!strcmp(modelNames_[slot], name)) return true;

    char replacement[kModelNameCapacity] = {};
    snprintf(replacement, sizeof(replacement), "%s", name);
    if (modelBackupInstalled_[slot]) {
        std::unique_ptr<ModelBackup> model(new(std::nothrow) ModelBackup);
        BrakeAbsSettings abs;
        if (!model || !readStoredModelBackup(slot, *model, abs)) return false;
        memcpy(model->name, replacement, sizeof(replacement));
        if (!writeStoredModelBackup(slot, *model, abs)) return false;
    } else {
        Preferences preferences;
        if (!preferences.begin("jr-r1-models", false)) return false;
        char names[kModelMemoryCount][kModelNameCapacity] = {};
        const bool alreadyStored = preferences.getBytesLength("names") == sizeof(names);
        if (alreadyStored && preferences.getBytes("names", names, sizeof(names)) != sizeof(names)) {
            preferences.end();
            return false;
        }
        if (!alreadyStored) {
            for (uint8_t index = 0; index < kModelMemoryCount; ++index) {
                if (modelBackupInstalled_[index])
                    snprintf(names[index], kModelNameCapacity, "MODEL %u", unsigned(index + 1));
                else
                    memcpy(names[index], modelNames_[index], kModelNameCapacity);
            }
        }
        memcpy(names[slot], replacement, sizeof(replacement));
        char readback[kModelMemoryCount][kModelNameCapacity];
        const bool saved = preferences.putBytes("names", names, sizeof(names)) == sizeof(names) &&
            preferences.getBytes("names", readback, sizeof(readback)) == sizeof(readback) &&
            !memcmp(names, readback, sizeof(names));
        preferences.end();
        if (!saved) return false;
    }
    memcpy(modelNames_[slot], replacement, sizeof(replacement));
    ++modelRevision_;
    return true;
}

bool TransmitterUi::modelManagementAllowed() const
{
    const int throttleDelta = static_cast<int>(latestRawAdc_) - static_cast<int>(triggerInput_.centre());
    return bootPinUnlocked_ && pinPurpose_ == PinPurpose::None && startupPreflightComplete_ &&
        !hardwareRecovery_ && !rfBenchMode_ && !armRequested_ &&
        designSafety_ != OutputSafetyStatus::Armed &&
        designSafety_ != OutputSafetyStatus::LinkRequired &&
        designSafety_ != OutputSafetyStatus::NeutralRequired &&
        abs(throttleDelta) <= ARM_NEUTRAL_ADC_WINDOW &&
        !calibrationActive() && !escProgrammer_.blocksArm() && !SetupPortal::firmwareUpdating();
}

bool TransmitterUi::manageModelMemory(ModelManagementAction action, uint8_t slot, const char *name)
{
    auto failed = [&](const char *reason) {
        snprintf(modelSelectStatus_, sizeof(modelSelectStatus_), "%s", reason);
        return false;
    };
    if (!modelManagementAllowed()) return failed("CUT and centre throttle before editing models");
    if (slot >= kModelMemoryCount) return failed("Choose a model slot");
    if ((action == ModelManagementAction::New || action == ModelManagementAction::Copy) &&
        slot == activeModelSlot_) return failed("Choose an inactive model slot");
    if (action == ModelManagementAction::Reset) name = modelNames_[slot];
    if (!validModelBackupName(name)) return failed("Enter a name of 1-12 printable characters");
    if (action == ModelManagementAction::Rename) {
        if (!renameModelMemory(slot, name)) return failed("Could not save model name");
        snprintf(modelSelectStatus_, sizeof(modelSelectStatus_), "Renamed slot %u", unsigned(slot + 1));
        return true;
    }

    std::unique_ptr<ModelBackup> model(new(std::nothrow) ModelBackup);
    std::unique_ptr<uint8_t[]> bytes(new(std::nothrow) uint8_t[kModelBackupSize]);
    if (!model || !bytes) return failed("Could not allocate model settings");
    if (action == ModelManagementAction::Copy) {
        if (modelStorageFault_ || !brakeAbsStorageValid_) return failed("Current model settings are invalid");
        collectModelBackup(*model, channelModel_);
    } else {
        model->bindings[0] = defaultTimerBinding();
        model->bindingCount = 1;
    }
    snprintf(model->name, sizeof(model->name), "%s", name);
    if (!encodeModelBackup(*model, bytes.get(), kModelBackupSize)) return failed("Invalid model controls");

    const bool replacingActive = slot == activeModelSlot_;
    if (replacingActive) requestConfigurationCut();
    if (!installModelBackup(slot, bytes.get(), model->name)) return failed("Could not save model controls");
    if (replacingActive) {
        applyModelBackupControls(*model);
        brakeAbs_ = BrakeAbsSettings{};
        brakeAbsStorageValid_ = true;
        modelStorageFault_ = modelBackupReadFailed_ = false;
        settingsSavePending_ = channelParameterSavePending_ = false;
        selectedChannel_ = channelRoleSettings_.throttleChannel;
        bindingsChanged_ = channelRolesChanged_ = true;
    }
    snprintf(modelSelectStatus_, sizeof(modelSelectStatus_), "%s slot %u%s",
        action == ModelManagementAction::Copy ? "Copied controls to" :
            action == ModelManagementAction::New ? "Created model in" : "Reset controls in",
        unsigned(slot + 1), replacingActive ? "; re-arm required" : "");
    return true;
}
