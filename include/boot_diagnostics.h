#pragma once
#include <stdint.h>

enum class BootCheck : uint8_t { Keypad, Controls, TouchReset, Touch, LcdReset, Display, Ui, Count };

#if defined(JR_R1_XIAO_PCB) && !defined(JR_R1_SIMULATOR)
void bootDiagnosticsBegin(uint32_t resetReason);
void bootDiagnosticsStage(const char *stage);
void bootDiagnosticsResult(BootCheck check, bool ok);
void bootDiagnosticsDump();
#else
inline void bootDiagnosticsBegin(uint32_t) {}
inline void bootDiagnosticsStage(const char *) {}
inline void bootDiagnosticsResult(BootCheck, bool) {}
inline void bootDiagnosticsDump() {}
#endif
