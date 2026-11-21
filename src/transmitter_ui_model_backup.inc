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
    channelModel_ = backup->channels;
    channelRoleSettings_ = backup->roles;
    driveModeSettings_.walkLimitPercent = backup->walkLimitPercent;
    bindingCount_ = backup->bindingCount;
    for (size_t i = 0; i < bindingCount_; ++i) bindings_[i] = backup->bindings[i];
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
    std::unique_ptr<uint8_t[]> bytes(new(std::nothrow) uint8_t[kModelBackupSize]);
    if (!backup || !bytes) return false;
    collectModelBackup(*backup, channels);
    return encodeModelBackup(*backup, bytes.get(), kModelBackupSize) &&
        writeStoredModelBackup(activeModelSlot_, bytes.get(), brakeAbs_);
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
    // ESC verification belongs to a physical model and cannot transfer in a
    // portable control profile or be inherited from the old destination.
    if (!model || !decodeModelBackup(data, size, *model) || !writeStoredModelBackup(slot, data, BrakeAbsSettings{}))
        return false;
    modelBackupInstalled_[slot] = true;
    snprintf(modelNames_[slot], kModelNameCapacity, "%s", model->name);
    ++modelRevision_;
    if (activePage_ == UiPage::ModelSelect) drawModelSelectPage();
    return true;
}
