#pragma once
#include <cstddef>
#include <cstdint>
#include "ui_cold_storage.h"

struct TxParameterRequest {
    TxParameterRequest(uint8_t t=0x28, uint8_t i=0, uint8_t v=0, uint8_t a=0xee) : type(t), id(i), value(v), address(a) {}
    uint8_t type = 0x28;
    uint8_t id = 0;
    uint8_t value = 0; // read chunk or selection/command value
    uint8_t address = 0xee; // TX module or receiver; never broadcast writes
};
size_t buildTxParameterFrame(const TxParameterRequest &, uint8_t *, size_t);

struct TxParameter {
    bool valid = false;
    bool hidden = false;
    uint8_t parent = 0, type = 0, value = 0, minimum = 0, maximum = 0;
    uint8_t timeout = 0;
    char name[48] = {};
    char options[384] = {};
    char info[112] = {};
    bool option(uint8_t index, char *out, size_t capacity) const;
};

// UI-task owned. Only one request is outstanding; replies must match the TX,
// requested parameter and chunk sequence. Never retries a mutating command.
class CrsfTxConfig {
public:
    static constexpr uint8_t kMaxParameters = 64;
    explicit CrsfTxConfig(UiColdAllocator::Allocate allocate=UiColdAllocator::allocate,
                          UiColdAllocator::Release release=UiColdAllocator::release)
        :parameters_(allocate,release){}
    void start(uint32_t now, uint8_t address=0xee);
    uint8_t address() const { return address_; }
    void stop();
    bool poll(uint32_t now, TxParameterRequest &request);
    void receive(uint8_t type, const uint8_t *payload, size_t length, uint32_t now);
    bool apply(uint8_t id, uint8_t value, uint32_t now);
    bool command(uint8_t id, uint8_t action, uint32_t now);
    const TxParameter *parameter(uint8_t id) const;
    bool ready() const;
    bool confirming() const;
    bool busyCommand() const { return commandId_ != 0; }
    const char *status() const { return status_; }
    const char *device() const { return device_; }
    uint8_t count() const { return count_; }
    uint32_t revision() const { return revision_; }
private:
    bool pollRequest(uint32_t now, TxParameterRequest &request);
    uint8_t address_ = 0xee;
    enum class State { Off, Discover, Read, Ready, Write, Command, Failed };
    void fail(const char *message);
    void read(uint8_t id, uint32_t now);
    bool decode(TxParameter &parameter);
    void status(const char *message);
    State state_ = State::Off;
    struct ParameterCache { TxParameter values[kMaxParameters] = {}; };
    UiColdStorage<ParameterCache> parameters_;
    uint8_t count_ = 0, id_ = 0, chunk_ = 0, remaining_ = 0, attempts_ = 0;
    uint8_t verifyId_ = 0, desired_ = 0, commandId_ = 0, commandAction_ = 0;
    bool waiting_ = false, heartbeat_ = false;
    uint32_t due_ = 0, lastReply_ = 0, revision_ = 0;
    size_t used_ = 0;
    uint8_t data_[640] = {};
    char device_[48] = {};
    char status_[112] = "Refresh to discover TX module";
};
