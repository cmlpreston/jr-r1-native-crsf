#include "sport_crsf.h"

namespace onboard {

bool SportParser::feed(uint8_t byte, uint32_t nowMs, SportSample &sample)
{
    if (active_ && uint32_t(nowMs - lastByteMs_) > 10) {
        active_ = false;
        escaped_ = false;
        count_ = 0;
    }
    lastByteMs_ = nowMs;
    if (byte == 0x7e) {
        active_ = true;
        count_ = 0;
        escaped_ = false;
        return false;
    }
    if (!active_) return false;
    if (escaped_) {
        escaped_ = false;
        if (byte != 0x5e && byte != 0x5d) {
            active_ = false;
            return false;
        }
        byte ^= 0x20;
    } else if (byte == 0x7d) {
        escaped_ = true;
        return false;
    }
    bytes_[count_++] = byte;
    if (count_ != sizeof(bytes_)) return false;
    active_ = false;
    uint16_t sum = 0;
    for (size_t i = 1; i < sizeof(bytes_); ++i) {
        sum += bytes_[i];
        sum = (sum & 0xff) + (sum >> 8);
    }
    if (sum != 0xff) {
        ++checksumErrors_;
        return false;
    }
    if (bytes_[1] != 0x10) return false;
    sample = {bytes_[0], uint16_t(bytes_[2] | uint16_t(bytes_[3]) << 8),
        uint32_t(bytes_[4]) | uint32_t(bytes_[5]) << 8 |
        uint32_t(bytes_[6]) << 16 | uint32_t(bytes_[7]) << 24};
    return true;
}

bool SensorKey::matches(const SportSample &s) const
{
    return physicalId == s.physicalId && appId == s.appId;
}

void BatteryBridge::accept(const SportSample &s, uint32_t nowMs)
{
    if (voltageKey_.matches(s) && s.appId >= 0x0210 && s.appId <= 0x021f) {
        // VFAS is centivolts; deployed CRSF battery telemetry is decivolts.
        haveVoltage_ = s.value >= 10 && s.value <= 655350;
        if (haveVoltage_) {
            voltage_ = uint16_t((s.value + 5) / 10);
            voltageMs_ = nowMs;
        }
    }
    if (currentKey_.matches(s) && s.appId >= 0x0200 && s.appId <= 0x020f) {
        haveCurrent_ = s.value <= 65535;
        if (haveCurrent_) {
            current_ = uint16_t(s.value); // deciamps on both sides
            currentMs_ = nowMs;
        }
    }
}

size_t BatteryBridge::nextFrame(uint32_t nowMs, uint8_t *out, size_t capacity)
{
    if (!out || capacity < 12 || !haveVoltage_ || !haveCurrent_ ||
        uint32_t(nowMs - voltageMs_) > 3000 ||
        uint32_t(nowMs - currentMs_) > 3000 ||
        (sent_ && uint32_t(nowMs - sentMs_) < 200)) return 0;
    const uint8_t frame[] = {0xc8, 10, 0x08,
        uint8_t(voltage_ >> 8), uint8_t(voltage_),
        uint8_t(current_ >> 8), uint8_t(current_), 0, 0, 0, 0, 0};
    for (size_t i = 0; i < sizeof(frame); ++i) out[i] = frame[i];
    uint8_t crc = 0;
    for (size_t i = 2; i < 11; ++i) {
        crc ^= out[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = uint8_t((crc << 1) ^ ((crc & 0x80) ? 0xd5 : 0));
    }
    out[11] = crc;
    sent_ = true;
    sentMs_ = nowMs;
    return sizeof(frame);
}

} // namespace onboard
