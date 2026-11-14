#pragma once
#include "er6g_esc_startup.h"
bool er6gStartupResume(Er6gStartup::Record &record);
void er6gStartupArmNextBoot();
const char *er6gStartupDiagnostics();

// Selectable single-receiver mode: ownership is fixed for the whole boot.
void er6gStartupModeBegin();
bool er6gStartupProgramming();
bool er6gStartupPrepare();
bool er6gStartupCancel();
bool er6gStartupPending();
#ifdef ER6G_ESC_SINGLE_STAGE
bool er6gStartupEntryRequested();
#endif
