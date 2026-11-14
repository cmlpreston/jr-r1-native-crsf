// Single-application trial: no second-app hash, handoff, restart or OTA selection.
#include "Er6gEscStartupPeer.h"
#include <Preferences.h>
#include <esp_system.h>
#include <cstdio>
namespace {
bool programming=false,singlePending=false,entryRequested=false;
}
void er6gStartupModeBegin(){
 Preferences store;
 programming=true;entryRequested=false; // NVS failure must not permit PWM.
 if(!store.begin(Er6gStartup::Namespace,false))return;
 if(store.getUInt(Er6gStartup::TicketKey,0)==Er6gStartup::ProgramTicket){
  // Consume once BEFORE GPIO/UART activity. A software reset cannot retry entry.
  store.remove(Er6gStartup::TicketKey);
  entryRequested=store.getUInt(Er6gStartup::TicketKey,0)==0&&
      store.putUInt(Er6gStartup::LockKey,Er6gStartup::ProgramTicket)==sizeof(uint32_t);
 }else{
  programming=esp_reset_reason()!=ESP_RST_POWERON&&
      store.getUInt(Er6gStartup::LockKey,0)==Er6gStartup::ProgramTicket;
  if(!programming)store.remove(Er6gStartup::LockKey);
 }
 store.end();
}
bool er6gStartupEntryRequested(){return entryRequested;}
bool er6gStartupProgramming(){return programming;}
bool er6gStartupPending(){return singlePending;}
bool er6gStartupPrepare(){
 if(programming)return false;
 Preferences store;if(!store.begin(Er6gStartup::Namespace,false))return false;
 singlePending=store.putUInt(Er6gStartup::TicketKey,Er6gStartup::ProgramTicket)==sizeof(uint32_t)&&
     store.getUInt(Er6gStartup::TicketKey,0)==Er6gStartup::ProgramTicket;
 store.end();return singlePending;
}
bool er6gStartupCancel(){
 Preferences store;if(!store.begin(Er6gStartup::Namespace,false))return false;
 store.remove(Er6gStartup::TicketKey);
 const bool cleared=store.getUInt(Er6gStartup::TicketKey,0)==0;store.end();
 if(cleared)singlePending=false;
 return cleared; // Never release this boot's pin reservation.
}
bool er6gStartupResume(Er6gStartup::Record &){return false;}
void er6gStartupArmNextBoot(){} // This trial never selects another application.
const char *er6gStartupDiagnostics(){
 static char text[180];
 std::snprintf(text,sizeof(text),"startupStrategy=single-app starterPeerReady=0 starterArmed=0 singlePending=%u singleEntryRequested=%u",singlePending,entryRequested);
 return text;
}
