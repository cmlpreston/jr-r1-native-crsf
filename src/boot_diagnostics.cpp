#include "boot_diagnostics.h"
#if defined(JR_R1_XIAO_PCB) && !defined(JR_R1_SIMULATOR)
#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr unsigned kSlots = 8;
constexpr uint32_t kFormat = 0x52314201;
struct Record {
    uint32_t format, sequence, resetReason, elapsedMs, freeHeap, largestDma;
    char stage[32];
    uint8_t checks[static_cast<unsigned>(BootCheck::Count)];
    uint8_t reserved;
    uint32_t checksum;
};
Record current{};
bool active = false;
const char *const names[] = {"K1", "K2", "touch-reset", "touch-detected", "LCD-reset", "display-driver", "UI"};

uint32_t checksum(const Record &record)
{
    uint32_t hash = 2166136261u;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&record);
    for (size_t i = 0; i < offsetof(Record, checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

void keyFor(unsigned slot, char (&key)[8]) { snprintf(key, sizeof(key), "boot%u", slot); }

bool read(Preferences &prefs, unsigned slot, Record &record)
{
    char key[8];
    keyFor(slot, key);
    // isKey avoids noisy missing-key diagnostics on a fresh device.
    return prefs.isKey(key) && prefs.getBytesLength(key) == sizeof(record) &&
        prefs.getBytes(key, &record, sizeof(record)) == sizeof(record) &&
        record.format == kFormat && record.stage[sizeof(record.stage)-1] == 0 &&
        record.checksum == checksum(record);
}

void save()
{
    if (!active) return;
    current.elapsedMs = millis();
    current.freeHeap = ESP.getFreeHeap();
    current.largestDma = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    current.checksum = checksum(current);
    Preferences prefs;
    bool saved = false;
    if (prefs.begin("r1-bootlog", false)) {
        char key[8];
        keyFor(current.sequence % kSlots, key);
        saved = prefs.putBytes(key, &current, sizeof(current)) == sizeof(current);
        prefs.end();
    }
    if (!saved) Serial.println("BOOTLOG: flash write failed; diagnostic history may be incomplete");
}
}

void bootDiagnosticsDump()
{
    Preferences prefs;
    if (!prefs.begin("r1-bootlog", true)) {
        Serial.println("BOOTLOG: storage unavailable");
        return;
    }
    Record records[kSlots]{};
    bool valid[kSlots]{};
    uint32_t newest = 0;
    bool any = false;
    for (unsigned i = 0; i < kSlots; ++i) {
        valid[i] = read(prefs, i, records[i]);
        if (valid[i] && (!any || static_cast<int32_t>(records[i].sequence - newest) > 0)) {
            newest = records[i].sequence;
            any = true;
        }
    }
    prefs.end();
    Serial.println("BOOTLOG: saved startups, oldest first; power source is not detected");
    if (!any) Serial.println("BOOTLOG: no saved startups");
    for (unsigned age = kSlots; any && age > 0; --age) {
        const uint32_t sequence = newest - (age - 1);
        const unsigned slot = sequence % kSlots;
        if (!valid[slot] || records[slot].sequence != sequence) continue;
        const Record &record = records[slot];
        Serial.printf("BOOTLOG #%lu reset=%lu stage=%s at=%lums heap=%lu largest_DMA=%lu\n",
            static_cast<unsigned long>(record.sequence), static_cast<unsigned long>(record.resetReason),
            record.stage, static_cast<unsigned long>(record.elapsedMs),
            static_cast<unsigned long>(record.freeHeap), static_cast<unsigned long>(record.largestDma));
        for (unsigned i = 0; i < static_cast<unsigned>(BootCheck::Count); ++i)
            Serial.printf("  %s=%s", names[i], record.checks[i] == 1 ? "PASS" : record.checks[i] == 2 ? "FAIL" : "NOT-RUN");
        Serial.println();
    }
    Serial.println("BOOTLOG END");
}

void bootDiagnosticsBegin(uint32_t resetReason)
{
    active = false;
    Preferences prefs;
    if (!prefs.begin("r1-bootlog", false)) {
        Serial.println("BOOTLOG: cannot open flash storage");
        return;
    }
    Record record{};
    uint32_t newest = 0;
    bool any = false;
    for (unsigned i = 0; i < kSlots; ++i)
        if (read(prefs, i, record) && (!any || static_cast<int32_t>(record.sequence - newest) > 0)) {
            newest = record.sequence;
            any = true;
        }
    prefs.end();
    // Print retained history before this boot updates the ring.
    bootDiagnosticsDump();
    current = {};
    current.format = kFormat;
    current.sequence = any ? newest + 1 : 1;
    current.resetReason = resetReason;
    active = true;
    bootDiagnosticsStage("startup");
    Serial.println("BOOTLOG: type BOOTLOG then Enter to retrieve the last eight startups");
}

void bootDiagnosticsStage(const char *stage)
{
    if (!active) return;
    snprintf(current.stage, sizeof(current.stage), "%s", stage);
    save();
    Serial.printf("BOOTLOG: stage=%s\n", current.stage);
}

void bootDiagnosticsResult(BootCheck check, bool ok)
{
    const unsigned index = static_cast<unsigned>(check);
    if (!active || index >= static_cast<unsigned>(BootCheck::Count)) return;
    current.checks[index] = ok ? 1 : 2;
    save();
    Serial.printf("BOOTLOG: %s=%s\n", names[index], ok ? "PASS" : "FAIL");
}
#endif
