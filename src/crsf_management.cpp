#include "crsf_management.h"

#include <mbedtls/md5.h>

#include "crsf_channel_output.h"

namespace {
constexpr uint8_t kHandsetAddress = 0xea;
constexpr uint8_t kReceiverAddress = 0xec;
constexpr uint8_t kTransmitterAddress = 0xee;
constexpr uint8_t kFrameMspRequest = 0x7a;
constexpr uint8_t kFrameMspWrite = 0x7c;
constexpr uint8_t kMspVersion1Request = 0x30;
constexpr uint8_t kMspSetBindPhrase = 0x2d;
constexpr uint8_t kSubcommandUid = 0x00;
constexpr uint8_t kSubcommandBindPhrase = 0x01;
}

size_t buildElrsManagementFrame(const ElrsManagementRequest &request,
                                uint8_t *frame, size_t capacity)
{
    if (request.action == ElrsManagementAction::TxParameter)
        return buildTxParameterFrame(request.parameter, frame, capacity);
    const bool write = request.action == ElrsManagementAction::SetBindPhrase;
    const size_t phraseLength = write
        ? min<size_t>(request.phraseLength, kElrsMaximumBindPhraseLength) : 0;
    const size_t payloadLength = 6 + phraseLength;
    const size_t frameLength = payloadLength + 4;
    if (frame == nullptr || capacity < frameLength) return 0;

    frame[0] = CRSF_SYNC_BYTE;
    frame[1] = static_cast<uint8_t>(payloadLength + 2); // type + payload + CRC
    frame[2] = write ? kFrameMspWrite : kFrameMspRequest;
    frame[3] = request.target == ElrsManagementTarget::Transmitter
        ? kTransmitterAddress : kReceiverAddress;
    frame[4] = kHandsetAddress;
    frame[5] = kMspVersion1Request;
    frame[6] = static_cast<uint8_t>(1 + phraseLength);
    frame[7] = kMspSetBindPhrase;
    frame[8] = write ? kSubcommandBindPhrase : kSubcommandUid;
    if (phraseLength != 0)
        memcpy(&frame[9], request.phrase, phraseLength);
    frame[frameLength - 1] = crc8DvbS2(&frame[2], frameLength - 3);
    return frameLength;
}

void calculateElrsBindPhraseUid(const char *phrase, uint8_t uid[kElrsUidLength])
{
    memset(uid, 0, kElrsUidLength);
    if (phrase == nullptr || phrase[0] == '\0') return;
    const char prefix[] = "-DMY_BINDING_PHRASE=\"";
    const char suffix[] = "\"";
    mbedtls_md5_context context;
    uint8_t digest[16];
    mbedtls_md5_init(&context);
    mbedtls_md5_starts_ret(&context);
    mbedtls_md5_update_ret(&context,
        reinterpret_cast<const unsigned char *>(prefix), strlen(prefix));
    mbedtls_md5_update_ret(&context,
        reinterpret_cast<const unsigned char *>(phrase), strlen(phrase));
    mbedtls_md5_update_ret(&context,
        reinterpret_cast<const unsigned char *>(suffix), strlen(suffix));
    mbedtls_md5_finish_ret(&context, digest);
    mbedtls_md5_free(&context);
    memcpy(uid, digest, kElrsUidLength);
}
