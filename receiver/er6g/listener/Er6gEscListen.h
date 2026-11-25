#pragma once
// Bench-only: physical output 5 is reserved for receive-only ESC data.
bool er6gEscListenOwnsPin(int pin);
void er6gEscListenLoop();
const char *er6gEscListenDiagnostics();
