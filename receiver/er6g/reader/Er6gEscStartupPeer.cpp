#include "Er6gEscStartupPeer.h"
#include "Er6gEscStartupImage.h"
#include <Preferences.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <cstring>
#include <cstdio>
namespace {
bool checked=false,armed=false;
const esp_partition_t *verifiedPeer=nullptr;
esp_err_t lastError=ESP_OK;
uint32_t stageResult=0,stageReadyUs=0,stageElapsedUs=0,stageRxCount=0,stageRxTail=0,stageReadProgress=0;
const esp_partition_t *peer(){
 if(checked)return verifiedPeer;checked=true;
 const auto *running=esp_ota_get_running_partition();
 const auto *candidate=esp_ota_get_next_update_partition(running);
 if(!running||!candidate||candidate==running||candidate->type!=ESP_PARTITION_TYPE_APP)return nullptr;
 uint8_t digest[32];lastError=esp_partition_get_sha256(candidate,digest);
 if(lastError==ESP_OK&&!std::memcmp(digest,Er6gStartupImageSha,sizeof(digest)))verifiedPeer=candidate;
 return verifiedPeer;
}
}
bool er6gStartupResume(Er6gStartup::Record &record){
 // NVS survives loss of ESC power; a previous cycle is never proof of this one.
 if(esp_reset_reason()!=ESP_RST_SW)return false;
 Preferences store;
 if(!store.begin(Er6gStartup::Namespace,true))return false;
 const bool found=store.getBytesLength(Er6gStartup::Key)==sizeof(record)&&
                  store.getBytes(Er6gStartup::Key,&record,sizeof(record))==sizeof(record);
 store.end();
 const auto *running=esp_ota_get_running_partition();
 if(!found||!record.valid()||!running||record.returnAddress!=running->address||!peer())return false;
 stageResult=record.result;stageReadyUs=record.readyUs;stageElapsedUs=record.elapsedUs;stageRxCount=record.rxCount;stageRxTail=record.rxTail;stageReadProgress=record.readProgress;
 // Consume only an intact record tied to the running full app and exact small image.
 if(store.begin(Er6gStartup::Namespace,false)){store.remove(Er6gStartup::Key);store.end();}
 return true;
}
void er6gStartupArmNextBoot(){
 if(const auto *candidate=peer()){
  lastError=esp_ota_set_boot_partition(candidate);armed=lastError==ESP_OK;
 }
}
const char *er6gStartupDiagnostics(){
 static char text[500];
 const auto *running=esp_ota_get_running_partition();
 const auto *other=running?esp_ota_get_next_update_partition(running):nullptr;
 std::snprintf(text,sizeof(text),"starterPeerReady=%u starterArmed=%u starterSlot=%lu starterError=%d starterResult=%lu starterReadyUs=%lu starterElapsedUs=%lu starterRunningSlot=%lu starterOtherSlot=%lu starterOtherSize=%lu starterRxCount=%lu starterRxTail=%08lx starterReadProgress=%lu",
  verifiedPeer!=nullptr,armed,verifiedPeer?(unsigned long)verifiedPeer->address:0UL,int(lastError),
  (unsigned long)stageResult,(unsigned long)stageReadyUs,(unsigned long)stageElapsedUs,
  running?(unsigned long)running->address:0UL,other?(unsigned long)other->address:0UL,other?(unsigned long)other->size:0UL,(unsigned long)stageRxCount,(unsigned long)stageRxTail,(unsigned long)stageReadProgress);
 return text;
}

namespace {bool programmingMode=false,nextProgramming=false;}
void er6gStartupModeBegin(){
 peer(); // Report the exact hashed startup peer even on an ordinary Drive boot.
 // The small app has already selected this full app for subsequent boots.
 // Lock survives a lost handoff record, but only applies to the software handoff.
 Preferences store;
 if(!store.begin(Er6gStartup::Namespace,false)){programmingMode=true;return;}
 programmingMode=esp_reset_reason()!=ESP_RST_POWERON&&store.getUInt(Er6gStartup::LockKey,0)==Er6gStartup::ProgramTicket;
 if(!programmingMode)store.remove(Er6gStartup::LockKey);
 store.end();
}
bool er6gStartupProgramming(){return programmingMode;}
bool er6gStartupPending(){return nextProgramming;}
bool er6gStartupPrepare(){
 if(programmingMode)return false;
 const auto *candidate=peer();if(!candidate)return false;
 Preferences store;if(!store.begin(Er6gStartup::Namespace,false))return false;
 const bool saved=store.putUInt(Er6gStartup::TicketKey,Er6gStartup::ProgramTicket)==sizeof(uint32_t);
 store.end();if(!saved)return false;
 lastError=esp_ota_set_boot_partition(candidate);armed=nextProgramming=lastError==ESP_OK;
 if(!armed){if(store.begin(Er6gStartup::Namespace,false)){store.remove(Er6gStartup::TicketKey);store.end();}}
 return armed;
}
bool er6gStartupCancel(){
 const auto *running=esp_ota_get_running_partition();if(!running)return false;
 Preferences store;if(!store.begin(Er6gStartup::Namespace,false))return false;
 store.remove(Er6gStartup::TicketKey);
 const bool cleared=store.getUInt(Er6gStartup::TicketKey,0)==0;store.end();
 if(!cleared)return false;
 lastError=esp_ota_set_boot_partition(running);
 if(lastError!=ESP_OK)return false;
 armed=nextProgramming=false;
 // Do not release the programming lock or restore PWM in a live ESC session.
 return true;
}
