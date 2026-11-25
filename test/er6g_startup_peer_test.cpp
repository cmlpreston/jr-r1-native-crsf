#include <Arduino.h>
#include <driver/uart.h>
#include "../receiver/er6g/reader/Er6gEscStartupPeer.cpp"
#include <iostream>
void reset(){
 savedTicket=savedLock=0;programmingMode=nextProgramming=false;resetReason=ESP_RST_SW;checked=armed=false;verifiedPeer=nullptr;lastError=0;stageResult=stageReadyUs=stageElapsedUs=stageRxCount=stageRxTail=stageReadProgress=0;
 badHash=failSelect=differentHash=returnRunning=false;peerPart.type=0;selects=0;savedRecord.clear();
}
void save(bool valid=true){Er6gStartup::Record r;r.result=Er6gStartup::ReadVerified;r.value=1;r.returnAddress=runningPart.address;r.readyUs=20000;r.elapsedUs=1500000;r.rxCount=7;r.rxTail=0x00001b01;r.readProgress=5;r.seal();if(!valid)r.value=2;savedRecord.assign(reinterpret_cast<uint8_t*>(&r),reinterpret_cast<uint8_t*>(&r)+sizeof(r));}
int main(){
 reset();save();Er6gStartup::Record r;assert(er6gStartupResume(r)&&savedRecord.empty()&&r.value==1);
 assert(selects==0);er6gStartupArmNextBoot();assert(selects==1&&armed);
 assert(strstr(er6gStartupDiagnostics(),"starterResult=1"));
 assert(strstr(er6gStartupDiagnostics(),"starterRxCount=7")&&strstr(er6gStartupDiagnostics(),"starterRxTail=00001b01")&&strstr(er6gStartupDiagnostics(),"starterReadProgress=5"));
 reset();save();resetReason=ESP_RST_POWERON;assert(!er6gStartupResume(r)&&!savedRecord.empty()&&selects==0);
 reset();save();Er6gStartup::Record corruptTrace;memcpy(&corruptTrace,savedRecord.data(),sizeof(corruptTrace));corruptTrace.rxTail^=1;memcpy(savedRecord.data(),&corruptTrace,sizeof(corruptTrace));assert(!er6gStartupResume(r));
 reset();save(false);assert(!er6gStartupResume(r)&&!savedRecord.empty()&&selects==0);
 reset();save();savedRecord.pop_back();assert(!er6gStartupResume(r)&&!savedRecord.empty());
 for(int scenario=0;scenario<4;++scenario){
  reset();save();if(scenario==0)badHash=true;if(scenario==1)differentHash=true;if(scenario==2)returnRunning=true;if(scenario==3)peerPart.type=1;
  assert(!er6gStartupResume(r)&&!savedRecord.empty());er6gStartupArmNextBoot();assert(selects==0&&!armed);
 }
 reset();save();Er6gStartup::Record wrong;memcpy(&wrong,savedRecord.data(),sizeof(wrong));wrong.returnAddress=peerPart.address;wrong.seal();memcpy(savedRecord.data(),&wrong,sizeof(wrong));
 assert(!er6gStartupResume(r)&&!savedRecord.empty()&&selects==0);
 reset();failSelect=true;er6gStartupArmNextBoot();assert(selects==1&&!armed);
 reset();er6gStartupModeBegin();assert(!er6gStartupProgramming());
 assert(er6gStartupPrepare()&&savedTicket==Er6gStartup::ProgramTicket&&er6gStartupPending());
 assert(er6gStartupCancel()&&savedTicket==0&&!er6gStartupPending()&&!er6gStartupProgramming());
 reset();savedLock=Er6gStartup::ProgramTicket;er6gStartupModeBegin();assert(er6gStartupProgramming());
 assert(!er6gStartupPrepare());assert(er6gStartupCancel()&&er6gStartupProgramming());
 reset();savedLock=Er6gStartup::ProgramTicket;resetReason=ESP_RST_POWERON;er6gStartupModeBegin();assert(!er6gStartupProgramming()&&savedLock==0);
 reset();savedLock=Er6gStartup::ProgramTicket;resetReason=7;er6gStartupModeBegin();assert(er6gStartupProgramming()); // watchdog/external reset cannot restore PWM
 reset();failStore=true;assert(!er6gStartupPrepare());failStore=false;
 std::cout<<"ER6G full-peer: exact stage hash, current-bank/record integrity checks, no premature selection, consume once, safe next-boot arming/failure PASS\n";
}
