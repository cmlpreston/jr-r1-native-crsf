#pragma once
#include <cstddef>
#include <cstdint>
namespace Er6gStartup {
constexpr uint32_t Magic=0x52314553,Schema=2;
enum Result:uint32_t {ReadVerified=1,NoStartup=2,HandshakeFailed=3,ReadFailed=4,UartFailed=5,DriveBypass=6};
struct Record {
 uint32_t magic=Magic,schema=Schema,result=NoStartup,value=255;
 uint32_t readyUs=0,elapsedUs=0,returnAddress=0,reserved=0;
 uint32_t rxCount=0,rxTail=0,readProgress=0,checksum=0;
 uint32_t digest() const {
  const auto *p=reinterpret_cast<const uint8_t*>(this);uint32_t h=2166136261u;
  for(size_t i=0;i<offsetof(Record,checksum);++i){h^=p[i];h*=16777619u;}return h;
 }
 void seal(){checksum=digest();}
 bool valid()const{return magic==Magic&&schema==Schema&&reserved==0&&checksum==digest()&&result>=ReadVerified&&result<=DriveBypass&&(result!=ReadVerified||value<9);}
};
static_assert(sizeof(Record)==48,"Stable NVS handoff layout");
constexpr uint32_t ProgramTicket=0x50524731;
constexpr const char *TicketKey="request",*LockKey="locked";
constexpr const char *Namespace="r1esc-start",*Key="handoff";
}
