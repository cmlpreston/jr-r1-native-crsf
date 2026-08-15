#include "crsf_management.h"

#include <openssl/md5.h>

#include <cstring>
#include <string>

#include "crsf_channel_output.h"

namespace {
constexpr uint8_t kHandsetAddress = 0xea;
constexpr uint8_t kReceiverAddress = 0xec;
constexpr uint8_t kTransmitterAddress = 0xee;
constexpr uint8_t kFrameMspRequest = 0x7a;
constexpr uint8_t kFrameMspWrite = 0x7c;
constexpr uint8_t kMspVersion1Request = 0x30;
constexpr uint8_t kMspSetBindPhrase = 0x2d;
}

size_t buildElrsManagementFrame(const ElrsManagementRequest &request,
                                uint8_t *frame, size_t capacity)
{
    const bool write = request.action == ElrsManagementAction::SetBindPhrase;
    const size_t phraseLength = write
        ? min<size_t>(request.phraseLength, kElrsMaximumBindPhraseLength) : 0;
    const size_t payloadLength = 6 + phraseLength;
    const size_t frameLength = payloadLength + 4;
    if (frame == nullptr || capacity < frameLength) return 0;
    frame[0] = CRSF_SYNC_BYTE;
    frame[1] = static_cast<uint8_t>(payloadLength + 2);
    frame[2] = write ? kFrameMspWrite : kFrameMspRequest;
    frame[3] = request.target == ElrsManagementTarget::Transmitter
        ? kTransmitterAddress : kReceiverAddress;
    frame[4] = kHandsetAddress;
    frame[5] = kMspVersion1Request;
    frame[6] = static_cast<uint8_t>(1 + phraseLength);
    frame[7] = kMspSetBindPhrase;
    frame[8] = write ? 0x01 : 0x00;
    if (phraseLength) std::memcpy(&frame[9], request.phrase, phraseLength);
    frame[frameLength - 1] = crc8DvbS2(&frame[2], frameLength - 3);
    return frameLength;
}

void calculateElrsBindPhraseUid(const char *phrase, uint8_t uid[kElrsUidLength])
{
    std::memset(uid, 0, kElrsUidLength);
    if (phrase == nullptr || phrase[0] == '\0') return;
    const std::string input = std::string("-DMY_BINDING_PHRASE=\"") + phrase + "\"";
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char *>(input.data()), input.size(), digest);
    std::memcpy(uid, digest, kElrsUidLength);
}
