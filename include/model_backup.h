#pragma once

#include "channel_model.h"
#include "control_assignment.h"

constexpr uint8_t kModelBackupVersion = 1;
constexpr size_t kModelBackupNameCapacity = 13;
constexpr size_t kModelBackupBindingSize = 21;
constexpr size_t kModelBackupSize = 5 + kModelBackupNameCapacity + 6 +
    kChannelModelRecordSize + kPhysicalControlCount * kModelBackupBindingSize + 4;
constexpr size_t kStoredModelBackupSize = kModelBackupSize + kBrakeAbsRecordSize;

struct ModelBackup {
    char name[kModelBackupNameCapacity] = {};
    ChannelRoleSettings roles;
    ChannelModelController channels;
    ControlBinding bindings[kPhysicalControlCount];
    uint8_t bindingCount = 0;
    uint8_t walkLimitPercent = 35;
};

bool validModelBackupName(const char *name);
bool encodeModelBackup(const ModelBackup &model, uint8_t *data, size_t size);
bool decodeModelBackup(const uint8_t *data, size_t size, ModelBackup &model);
