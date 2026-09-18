#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cassert>
struct FakeWire {
    struct Write { uint8_t address, reg, value; };
    std::array<std::array<uint8_t, 256>, 128> regs{};
    std::array<uint16_t, 128> heldLow{};
    std::array<bool, 128> missing{};
    std::vector<Write> writes;
    std::vector<uint8_t> tx, rx;
    uint8_t address = 0, pointer = 0;
    unsigned failAtWrite = 0;
    std::array<unsigned,128> failedReadsRemaining{};
    void beginTransmission(uint8_t value) { address=value; tx.clear(); }
    void write(uint8_t value) { tx.push_back(value); }
    int endTransmission(bool = true) {
        if (missing[address]) return 2;
        pointer=tx.at(0);
        if (tx.size()==2) {
            writes.push_back({address,pointer,tx[1]});
            if (failAtWrite && writes.size()==failAtWrite) return 2;
            regs[address][pointer]=tx[1];
        }
        return 0;
    }
    size_t requestFrom(uint8_t device, size_t count) {
        rx.clear();
        if (missing[device]) return 0;
        if (failedReadsRemaining[device]) { --failedReadsRemaining[device]; return 0; }
        for (size_t i=0;i<count;++i) {
            auto reg=uint8_t(pointer+i);
            uint8_t value=regs[device][reg];
            if (reg==0x12 || reg==0x13) {
                const unsigned bank=reg-0x12;
                const uint8_t inputs=regs[device][bank];
                value=((inputs & regs[device][0x0c+bank]) |
                       (~inputs & regs[device][0x14+bank])) &
                      ~(heldLow[device] >> (8*bank));
            }
            rx.push_back(value);
        }
        return count;
    }
    size_t available() const { return rx.size(); }
    int read() { auto value=rx.front(); rx.erase(rx.begin()); return value; }
};
extern FakeWire Wire;
