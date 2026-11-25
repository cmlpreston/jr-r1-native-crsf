#include "model_backup.h"
#include "binding_resolver.h"
#include <cstring>
#include <memory>
#include <new>

namespace {
struct Writer {
    uint8_t *p;
    void byte(unsigned value) { *p++ = uint8_t(value); }
    void word(uint16_t value) { byte(value); byte(value >> 8); }
};

struct Reader {
    const uint8_t *p;
    uint8_t byte() { return *p++; }
    uint16_t word() { const unsigned low = byte(); return low | unsigned(byte()) << 8; }
    int16_t signedWord() { const int n = word(); return n < 32768 ? n : n - 65536; }
};

uint32_t checksum(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffU;
    for (size_t n = 0; n < size; ++n) {
        crc ^= data[n];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
    }
    return ~crc;
}

bool validControls(const ModelBackup &model)
{
    const auto &roles = model.roles;
    if (roles.steeringChannel < 1 || roles.steeringChannel > CRSF_CHANNEL_COUNT ||
        roles.throttleChannel < 1 || roles.throttleChannel > CRSF_CHANNEL_COUNT ||
        roles.steeringChannel == roles.throttleChannel ||
        model.bindingCount > kPhysicalControlCount || model.walkLimitPercent < 10 || model.walkLimitPercent > 80)
        return false;

    uint32_t inputs = 0;
    for (size_t i = 0; i < model.bindingCount; ++i) {
        const auto &b = model.bindings[i];
        const unsigned input = static_cast<unsigned>(b.input);
        constexpr unsigned contexts = ContextAlways | ContextDashboard | ContextMenu | ContextCut | ContextArmed;
        if (input < 2 || input >= kPhysicalControlCount || (inputs & (1U << input)) ||
            static_cast<unsigned>(b.event) > static_cast<unsigned>(InputEvent::StateRestored) ||
            static_cast<unsigned>(b.behaviour) > static_cast<unsigned>(ControlBehaviour::Pulse) ||
            static_cast<unsigned>(b.target) > static_cast<unsigned>(AssignmentTarget::CrsfChannel) ||
            (b.contexts & ~contexts))
            return false;
        inputs |= 1U << input;
        if (b.target == AssignmentTarget::CrsfChannel) {
            if (b.minimum < -1000 || b.maximum > 1000 || b.step < 1 || b.step > 2000 ||
                (b.enabled && (b.crsfChannel == roles.steeringChannel || b.crsfChannel == roles.throttleChannel)))
                return false;
        }
        if (b.target == AssignmentTarget::ModelParameter) {
            const uint8_t channel = channelParameterTargetChannel(b.targetId);
            const auto kind = channelParameterTargetKind(b.targetId);
            if (!isChannelParameterTarget(b.targetId) || channel < 1 || channel > CRSF_CHANNEL_COUNT ||
                (kind != ChannelParameterKind::Trim && kind != ChannelParameterKind::Subtrim) ||
                encodeChannelParameterTarget(channel, kind) != b.targetId ||
                b.minimum < -200 || b.maximum > 200 || b.step < 1 || b.step > 400)
                return false;
            const auto &p = model.channels.parameters[channel - 1];
            int value = kind == ChannelParameterKind::Trim ? p.trimPermille : p.subtrimPermille;
            if (model.channels.responses[channel - 1].reversed) value = -value;
            if (value < b.minimum || value > b.maximum) return false;
        }
    }
    {
        BindingResolver validator;
        if (!validator.setBindings(model.bindings, model.bindingCount).ok()) return false;
    }
    ControlConfigurationSnapshot config;
    config.steeringChannel = roles.steeringChannel;
    config.throttleChannel = roles.throttleChannel;
    memcpy(config.mixing, model.channels.mixing, sizeof(config.mixing));
    return validConfiguredMixing(config);
}

void writeBinding(Writer &w, const ControlBinding &b)
{
    w.byte(static_cast<unsigned>(b.input)); w.byte(static_cast<unsigned>(b.event));
    w.byte(b.contexts); w.byte(static_cast<unsigned>(b.target));
    w.byte(static_cast<unsigned>(b.behaviour)); w.byte(b.crsfChannel); w.word(b.targetId);
    w.word(b.minimum); w.word(b.maximum); w.word(b.step); w.word(b.initial); w.word(b.failsafe);
    w.word(b.pulseMs);
    w.byte((b.enabled ? 1 : 0) | (b.persistent ? 2 : 0) | (b.inverted ? 4 : 0) | (b.safetyCritical ? 8 : 0));
}

bool readBinding(Reader &r, ControlBinding &b)
{
    b.input = static_cast<PhysicalControlId>(r.byte()); b.event = static_cast<InputEvent>(r.byte());
    b.contexts = r.byte(); b.target = static_cast<AssignmentTarget>(r.byte());
    b.behaviour = static_cast<ControlBehaviour>(r.byte()); b.crsfChannel = r.byte(); b.targetId = r.word();
    b.minimum = r.signedWord(); b.maximum = r.signedWord(); b.step = r.signedWord();
    b.initial = r.signedWord(); b.failsafe = r.signedWord(); b.pulseMs = r.word();
    const unsigned flags = r.byte();
    b.enabled = flags & 1; b.persistent = flags & 2; b.inverted = flags & 4; b.safetyCritical = flags & 8;
    return flags < 16;
}
}

bool validModelBackupName(const char *name)
{
    if (!name || !*name) return false;
    for (size_t i = 0; i < kModelBackupNameCapacity; ++i) {
        const char c = name[i];
        if (!c) return true;
        if (c < 32 || c > 126) return false;
    }
    return false;
}

bool encodeModelBackup(const ModelBackup &model, uint8_t *data, size_t size)
{
    if (!data || size != kModelBackupSize || !validModelBackupName(model.name) || !validControls(model))
        return false;
    memset(data, 0, size);
    memcpy(data, "R1MB", 4);
    data[4] = kModelBackupVersion;
    memcpy(data + 5, model.name, strlen(model.name));
    Writer w{data + 5 + kModelBackupNameCapacity};
    w.byte(model.roles.steeringChannel); w.byte(model.roles.throttleChannel);
    w.byte(model.roles.steeringEnabled); w.byte(model.roles.throttleEnabled); w.byte(model.bindingCount);
    w.byte(model.walkLimitPercent);
    if (!encodeChannelModel(model.channels, w.p, kChannelModelRecordSize)) return false;
    w.p += kChannelModelRecordSize;
    for (size_t i = 0; i < model.bindingCount; ++i) writeBinding(w, model.bindings[i]);
    const auto crc = checksum(data, size - 4);
    for (unsigned n = 0; n < 4; ++n) data[size - 4 + n] = uint8_t(crc >> (8 * n));
    return true;
}

bool decodeModelBackup(const uint8_t *data, size_t size, ModelBackup &model)
{
    if (!data || size != kModelBackupSize || memcmp(data, "R1MB", 4) || data[4] != kModelBackupVersion)
        return false;
    uint32_t crc = 0;
    for (unsigned n = 0; n < 4; ++n) crc |= uint32_t(data[size - 4 + n]) << (8 * n);
    if (crc != checksum(data, size - 4)) return false;

    std::unique_ptr<ModelBackup> decoded(new(std::nothrow) ModelBackup);
    if (!decoded) return false;
    auto &candidate = *decoded;
    memcpy(candidate.name, data + 5, sizeof(candidate.name));
    if (!validModelBackupName(candidate.name)) return false;
    Reader r{data + 5 + kModelBackupNameCapacity};
    candidate.roles.steeringChannel = r.byte(); candidate.roles.throttleChannel = r.byte();
    const auto steering = r.byte(), throttle = r.byte();
    if (steering > 1 || throttle > 1) return false;
    candidate.roles.steeringEnabled = steering;
    candidate.roles.throttleEnabled = throttle;
    candidate.bindingCount = r.byte();
    candidate.walkLimitPercent = r.byte();
    if (candidate.bindingCount > kPhysicalControlCount ||
        !decodeChannelModel(r.p, kChannelModelRecordSize, candidate.channels))
        return false;
    r.p += kChannelModelRecordSize;
    for (size_t i = 0; i < candidate.bindingCount; ++i)
        if (!readBinding(r, candidate.bindings[i])) return false;
    if (!validControls(candidate)) return false;
    model = candidate;
    return true;
}
