#pragma once
#include <stdint.h>
// Explicit Wi-Fi bench operation only: 0=startup response, 1=read parameter,
// 2=one 69ms low entry probe, conditional startup response (never a setting write).
// 3=bounded startup LOW hold; release explicitly after powering ESC.
bool er6gEscReadStart(uint8_t operation,uint8_t parameter,bool benchConfirmed);

// Implemented by rx_main to avoid importing the complete RF/device graph.
bool er6gEscReadWifiAllowed();

// Releases only a currently armed startup hold; all existing bench gates apply.
bool er6gEscReadReleaseEntry(bool benchConfirmed);

// Read-only 3-second early boot bench probe: one startup response/read0. No pulse/PWM.
void er6gEscReadBootProbe();

#ifdef ER6G_ESC_DRIVE_MODE
// RF-only single write, accepted only in a verified programming session.
// A valid acknowledgement is followed by one readback; never retries the write.
bool er6gEscWriteStart(uint8_t parameter,uint8_t value,bool confirmed);
bool er6gEscWriteValue(uint8_t &value);
bool er6gEscReadRfAllowed();
bool er6gEscReadBusy();
bool er6gEscReadSessionReady();
bool er6gEscReadValue(uint8_t &value);
void er6gEscReadAbort();
#endif
