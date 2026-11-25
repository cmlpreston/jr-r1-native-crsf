#include "../../receiver/er6g/reader/Er6gEscStartupPeer.h"
bool er6gStartupResume(Er6gStartup::Record &){return false;}
void er6gStartupArmNextBoot(){}
const char *er6gStartupDiagnostics(){return "starterPeerReady=0 starterArmed=0";}
