#include "boot_diagnostics.h"
#include <Arduino.h>
#include <Preferences.h>
#include <cassert>
#include <iostream>

int main()
{
    bootDiagnosticsBegin(1);
    bootDiagnosticsResult(BootCheck::Keypad, true);
    bootDiagnosticsResult(BootCheck::Controls, true);
    bootDiagnosticsResult(BootCheck::LcdReset, false);
    bootDiagnosticsStage("halted-safe");
    // A subsequent USB boot must retain the external boot's results.
    bootDiagnosticsBegin(21);
    bootDiagnosticsStage("touch"); // Simulate interrupted startup, no completion.
    Serial.output.clear();
    const auto writes = Preferences::writes;
    bootDiagnosticsDump();
    assert(Preferences::writes == writes); // Retrieval never alters history.
    assert(Serial.output.find("#1 reset=1 stage=halted-safe") != std::string::npos);
    assert(Serial.output.find("K1=PASS  K2=PASS") != std::string::npos);
    assert(Serial.output.find("LCD-reset=FAIL  display-driver=NOT-RUN") != std::string::npos);
    assert(Serial.output.find("#2 reset=21 stage=touch") != std::string::npos);
    for (int i = 3; i <= 10; ++i) bootDiagnosticsBegin(i);
    Serial.output.clear();
    bootDiagnosticsDump();
    assert(Serial.output.find("#2 ") == std::string::npos);
    assert(Serial.output.find("#3 ") < Serial.output.find("#10 "));
    assert(Preferences::storage.size() == 8);
    // Invalid or torn records are ignored; valid older records survive.
    Preferences::storage["boot2"][0] ^= 1;
    Serial.output.clear();
    bootDiagnosticsDump();
    assert(Serial.output.find("#10 ") == std::string::npos);
    assert(Serial.output.find("#9 ") != std::string::npos);
    Preferences::failWrites = true;
    Serial.output.clear();
    bootDiagnosticsStage("write-failure");
    assert(Serial.output.find("flash write failed") != std::string::npos);
    Preferences::failWrites = false;
    Serial.output.clear();
    bootDiagnosticsDump();
    assert(Serial.output.find("write-failure") == std::string::npos);
    std::cout << "Boot history: power-cycle retention, interrupted boot, bounded ring, read-only retrieval, corruption and write failure PASS\n";
}
