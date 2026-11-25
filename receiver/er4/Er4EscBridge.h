#pragma once
#include "crsf_protocol.h"
#include <Arduino.h>
void er4EscLoop();
bool er4EscHandle(const crsf_header_t *message);
bool ICACHE_RAM_ATTR er4EscSelected();

const char *er4EscDiagnostics();
const char *er4EscElectricalDiagnostics();

void ICACHE_RAM_ATTR er4EscRecordAck(bool value);
