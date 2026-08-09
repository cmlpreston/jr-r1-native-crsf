#include "crsf_telemetry.h"

namespace {

constexpr uint8_t kCrsfSyncByte = 0xC8;
constexpr uint8_t kRadioTransmitterAddress = 0xEA;
constexpr uint8_t kCrsfReceiverAddress = 0xEC;
constexpr uint8_t kCrsfTransmitterAddress = 0xEE;
constexpr uint8_t kFrameTypeBatterySensor = 0x08;
constexpr uint8_t kFrameTypeLinkStatistics = 0x14;
constexpr uint32_t kLinkFreshMs = 3000;
// ER6G battery packets are substantially less frequent than link-statistics
// packets. Retain the last valid voltage between reports without making a
// disconnected receiver appear live indefinitely.
constexpr uint32_t kBatteryFreshMs = 30000;

int16_t decodeRssiDbm(uint8_t raw)
{
    // Current ELRS stores the negative dBm value in the uint8_t CRSF field
    // (e.g. E1 = -31). Some older producers follow the CRSF prose spec and
    // send a positive magnitude instead (31 = -31 dBm). Accept both forms.
    return (raw & 0x80U) ? static_cast<int8_t>(raw)
                         : -static_cast<int16_t>(raw);
}

} // namespace

uint8_t CrsfTelemetryParser::crc8DvbS2(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    while (length-- != 0) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80U) ? static_cast<uint8_t>((crc << 1U) ^ 0xD5U)
                                : static_cast<uint8_t>(crc << 1U);
        }
    }
    return crc;
}

void CrsfTelemetryParser::feed(uint8_t value)
{
    ++state_.rawBytes;
    if (index_ == 0) {
        // Current ELRS source normalises handset-bound frames to 0xC8, while
        // released module firmware can preserve the CRSF device address.
        if (value != kCrsfSyncByte && value != kRadioTransmitterAddress &&
            value != kCrsfReceiverAddress && value != kCrsfTransmitterAddress) {
            return;
        }
        frame_[index_++] = value;
        return;
    }

    if (index_ == 1) {
        // Length counts type + payload + CRC, but excludes sync and itself.
        if (value < 2 || value > kMaximumFrameBytes - 2) {
            index_ = 0;
            return;
        }
        frame_[index_++] = value;
        expectedBytes_ = value + 2;
        return;
    }

    frame_[index_++] = value;
    if (index_ < expectedBytes_) return;

    const uint8_t length = frame_[1];
    const uint8_t calculated = crc8DvbS2(&frame_[2], length - 1);
    if (calculated == frame_[expectedBytes_ - 1]) {
        ++state_.validFrames;
        processFrame();
    } else {
        ++state_.crcErrors;
    }
    index_ = 0;
    expectedBytes_ = 0;
}

void CrsfTelemetryParser::processFrame()
{
    const uint8_t frameType = frame_[2];
    const uint8_t payloadLength = frame_[1] - 2;
    const uint8_t *payload = &frame_[3];

    if (frameType == kFrameTypeLinkStatistics && payloadLength >= 10) {
        state_.activeAntenna = payload[4];
        state_.uplinkRssiDbm =
            decodeRssiDbm(payload[state_.activeAntenna == 1 ? 1 : 0]);
        state_.uplinkLinkQuality = payload[2];
        state_.uplinkSnrDb = static_cast<int8_t>(payload[3]);
        state_.rfMode = payload[5];
        state_.txPowerCode = payload[6];
        state_.linkUpdatedMs = millis();
        state_.linkValid = true;
        return;
    }

    if (frameType == kFrameTypeBatterySensor && payloadLength >= 8) {
        state_.receiverVoltageDecivolts =
            static_cast<uint16_t>((payload[0] << 8) | payload[1]);
        state_.receiverVoltageValid = state_.receiverVoltageDecivolts != 0;
        state_.batteryUpdatedMs = millis();
    }
}

void CrsfTelemetryParser::updateFreshness()
{
    const uint32_t now = millis();
    if (state_.linkValid && now - state_.linkUpdatedMs > kLinkFreshMs) {
        state_.linkValid = false;
    }
    if (state_.receiverVoltageValid &&
        now - state_.batteryUpdatedMs > kBatteryFreshMs) {
        state_.receiverVoltageValid = false;
    }
}
