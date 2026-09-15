#include "sport_crsf.h"
#include "vehicle_sensors.h"
#include "crsf_telemetry.h"
#include <cassert>
#include <cstdio>
#include <vector>

static uint32_t clockMs;
uint32_t millis() { return clockMs; }

// Independent wire fixture: 12.60 V VFAS, with checksum computed by hand:
// 0x10 + 0x10 + 0x02 + 0xec + 0x04 -> folded sum 0x13, checksum 0xec.
static const std::vector<uint8_t> voltage =
    {0x7e, 0x22, 0x10, 0x10, 0x02, 0xec, 0x04, 0, 0, 0xec};
static const std::vector<uint8_t> current =
    {0x7e, 0x22, 0x10, 0x00, 0x02, 0x7b, 0, 0, 0, 0x72};

static bool feed(onboard::SportParser &p, const std::vector<uint8_t> &bytes,
                 uint32_t time, onboard::SportSample &sample)
{
    bool result = false;
    for (auto byte : bytes) result |= p.feed(byte, time, sample);
    return result;
}

int main()
{
    onboard::SportParser parser;
    onboard::SportSample s{};
    onboard::BatteryBridge bridge({0x22, 0x0210}, {0x22, 0x0200});
    uint8_t out[12]{};
    assert(bridge.nextFrame(0, out, sizeof(out)) == 0);
    assert(feed(parser, voltage, 0, s));
    assert(s.physicalId == 0x22 && s.appId == 0x0210 && s.value == 1260);
    bridge.accept(s, 0);
    assert(bridge.nextFrame(0, out, sizeof(out)) == 0);
    assert(feed(parser, current, 0, s));
    assert(s.value == 123);
    bridge.accept(s, 0);
    assert(bridge.nextFrame(0, nullptr, 12) == 0);
    assert(bridge.nextFrame(0, out, 11) == 0);
    assert(bridge.nextFrame(0, out, sizeof(out)) == 12);
    assert(out[0] == 0xc8 && out[1] == 10 && out[2] == 8);
    assert(out[3] == 0 && out[4] == 126 && out[5] == 0 && out[6] == 123);
    for (unsigned i = 7; i < 11; ++i) assert(out[i] == 0);
    CrsfTelemetryParser transmitter;
    for (auto byte : out) transmitter.feed(byte);
    assert(transmitter.state().validFrames == 1);
    assert(transmitter.state().crcErrors == 0);
    assert(transmitter.state().receiverVoltageDecivolts == 126);
    assert(bridge.nextFrame(199, out, 12) == 0);
    assert(bridge.nextFrame(200, out, 12) == 12);
    assert(bridge.nextFrame(3001, out, 12) == 0);
    bridge.accept({0x22, 0x0210, 1260}, 3001);
    assert(bridge.nextFrame(3001, out, 12) == 0); // stale current
    bridge.accept({0x23, 0x0200, 100}, 3001);
    assert(bridge.nextFrame(3001, out, 12) == 0); // wrong sensor
    bridge.accept({0x22, 0x0200, 123}, 3001);
    assert(bridge.nextFrame(3001, out, 12) == 12);
    bridge.accept({0x22, 0x0210, 0xffffffff}, 3201);
    assert(bridge.nextFrame(3201, out, 12) == 0);

    auto bad = voltage;
    bad.back() ^= 1;
    assert(!feed(parser, bad, 10, s));
    assert(parser.checksumErrors() == 1);
    assert(feed(parser, voltage, 11, s));
    // Stuffed value 0x7d7e, app ID 0x1234; valid folded checksum 0xad.
    assert(feed(parser, {0x7e, 0x22, 0x10, 0x34, 0x12,
        0x7d, 0x5e, 0x7d, 0x5d, 0, 0, 0xad}, 12, s));
    assert(s.value == 0x7d7e && s.appId == 0x1234);
    assert(!feed(parser, {0x7e, 0x22, 0x10, 0x7d, 0x01}, 13, s));
    assert(feed(parser, voltage, 14, s));
    assert(!feed(parser, {0x7e, 0x22, 0x10}, 15, s));
    assert(!feed(parser, {0x10, 0x02, 0xec, 0x04, 0, 0, 0xec}, 26, s));
    assert(feed(parser, voltage, 27, s));
    // New delimiter recovers from an incomplete response.
    assert(!feed(parser, {0x7e, 0x22, 0x10}, 28, s));
    assert(feed(parser, current, 28, s));

    onboard::BatteryBridge wrap({0x22, 0x0210}, {0x22, 0x0200});
    wrap.accept({0x22, 0x0210, 655350}, UINT32_MAX - 100);
    wrap.accept({0x22, 0x0200, 65535}, UINT32_MAX - 100);
    assert(wrap.nextFrame(UINT32_MAX - 100, out, 12) == 12);
    assert(out[3] == 255 && out[4] == 255 && out[5] == 255 && out[6] == 255);
    assert(wrap.nextFrame(50, out, 12) == 0);
    assert(wrap.nextFrame(100, out, 12) == 12);
    assert(wrap.nextFrame(3000, out, 12) == 0);
    // Deterministic noise exercises bounded parser state under sanitizers.
    uint32_t random = 42;
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525u + 1013904223u;
        parser.feed(uint8_t(random >> 24), i / 20, s);
    }
    assert(feed(parser, voltage, 6000, s));
    onboard::VehicleSensors vehicle({{0xa1, 0x0300}, {0x22, 0x0400}, 0x83, 0});
    uint16_t total = 0, minimum = 0;
    const auto pair = [](unsigned index, unsigned aMv, unsigned bMv) {
        return uint32_t(index | 0x40 | ((aMv / 2) << 8) | ((bMv / 2) << 20));
    };
    assert(!vehicle.pack(0, total, minimum));
    vehicle.accept({0xa1, 0x0300, pair(2, 4100, 4080)}, 0);
    assert(!vehicle.pack(0, total, minimum)); // needs first pair
    vehicle.accept({0xa1, 0x0300, pair(0, 4200, 4180)}, 0);
    assert(!vehicle.pack(0, total, minimum));
    vehicle.accept({0xa1, 0x0300, pair(2, 4100, 4080)}, 1);
    assert(vehicle.pack(1, total, minimum));
    assert(total == 16560 && minimum == 4080);
    assert(!vehicle.pack(3001, total, minimum));
    vehicle.accept({0xa1, 0x0300, pair(0, 4160, 4140)}, 3002);
    assert(!vehicle.pack(3002, total, minimum)); // old second pair discarded
    vehicle.accept({0xa1, 0x0300, pair(2, 0, 4100)}, 3003);
    assert(!vehicle.pack(3003, total, minimum));
    vehicle.accept({0xa1, 0x0300, pair(0, 4200, 4180)}, 3004);
    vehicle.accept({0xa1, 0x0300, pair(2, 4100, 4080)}, 3004);
    assert(vehicle.pack(3004, total, minimum));
    vehicle.accept({0xa1, 0x0300, 0x30}, 3005); // 3S is not this pack
    assert(!vehicle.pack(3005, total, minimum));
    vehicle.accept({0x22, 0x0400, uint32_t(-12)}, 10);
    assert(vehicle.motorTemperatureC.fresh(10));
    assert(vehicle.motorTemperatureC.value == -12);
    vehicle.accept({0x23, 0x0400, 90}, 11);
    assert(vehicle.motorTemperatureC.value == -12);
    vehicle.accept({0x22, 0x0400, 251}, 12);
    assert(!vehicle.motorTemperatureC.valid);
    // Melbourne-like southern latitude and east longitude, in minutes*10000.
    vehicle.accept({0x83, 0x0800, 0x40000000u | 22500000u}, 20);
    vehicle.accept({0x83, 0x0800, 0x80000000u | 87000000u}, 20);
    assert(vehicle.latitudeE7.value == -375000000);
    assert(vehicle.longitudeE7.value == 1450000000);
    vehicle.accept({0x83, 0x0830, 10000}, 20);
    assert(vehicle.speedDeciKmh.value == 185); // 10 knots = 18.52 km/h
    vehicle.accept({0x83, 0x0820, uint32_t(-1250)}, 20);
    assert(vehicle.altitudeCm.value == -1250);
    vehicle.accept({0x83, 0x0840, 35999}, 20);
    assert(vehicle.headingCentiDegrees.value == 35999);
    vehicle.accept({0x83, 0x0840, 36000}, 21);
    assert(!vehicle.headingCentiDegrees.valid);
    vehicle.accept({0x83, 0x0800, 54000001}, 21);
    assert(!vehicle.latitudeE7.valid);
    vehicle.accept({0x83, 0x0830, 0xffffffff}, 21);
    assert(!vehicle.speedDeciKmh.valid);
    assert(!vehicle.longitudeE7.fresh(3021));
    puts("S.Port/CRSF protocol, vehicle sensor and transmitter compatibility tests passed");
}
