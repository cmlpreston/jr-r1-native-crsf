#pragma once

#include <Arduino.h>

enum class ElrsManagementTarget : uint8_t {
    Transmitter,
    Receiver,
};

enum class ElrsManagementAction : uint8_t {
    ReadUid,
    SetBindPhrase,
};

struct ElrsManagementRequest {
    ElrsManagementAction action = ElrsManagementAction::ReadUid;
    ElrsManagementTarget target = ElrsManagementTarget::Transmitter;
    uint8_t phraseLength = 0;
    char phrase[52] = {};
};

constexpr size_t kElrsMaximumBindPhraseLength = 51;
constexpr size_t kElrsUidLength = 6;

// Builds a complete CRSF extended frame ready for the handset/module UART.
// Set Bindphrase From Handset is MSP function 0x2d, carried by CRSF MSP_REQ
// (read) or MSP_WRITE (write), as implemented by ExpressLRS 4.1.
size_t buildElrsManagementFrame(const ElrsManagementRequest &request,
                                uint8_t *frame, size_t capacity);

// ExpressLRS hashes the literal compiler define string, not just the phrase.
void calculateElrsBindPhraseUid(const char *phrase, uint8_t uid[kElrsUidLength]);

