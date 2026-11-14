#include "targets.h"
#include "Er6gEscMode.h"
#include "Er6gEscRead.h"
#include "Er6gEscStartupPeer.h"
#include "esc_programmer_wire.h"
#include "OTA.h"
#include "rxtx_intf.h"
#include <Arduino.h>
extern CRSFRouter crsfRouter;
namespace {
EscWire::Message response,last;
bool cached=false,reading=false,writing=false,replyReady=false;
uint32_t session=0;
uint16_t sequence=0;
bool linkedCut(){return connectionState==connected&&connectionHasModelMatch&&!isArmed;}
uint8_t flags(){return EscWire::SelectorAvailable|EscWire::ModeSelect|
 (er6gStartupProgramming()?EscWire::Programming:EscWire::Drive)|
 (er6gStartupPending()?EscWire::Pending:0)|
 (er6gEscReadSessionReady()?EscWire::Interface:0)
#if ER6G_ESC_WRITES
 |EscWire::Writes
#endif
 ;}
void reply(EscWire::Status status){response.status=status;response.flags=flags();replyReady=true;}
}
bool er6gEscReadRfAllowed(){return linkedCut()&&er6gStartupProgramming();}
bool er6gEscModeHandle(const crsf_header_t *frame){
 EscWire::Message request;
 if(!EscWire::parse(frame->type,reinterpret_cast<const uint8_t*>(frame)+3,frame->frame_size-2,false,request))return false;
 // Reserved messages are always consumed; never fall through to generic ELRS writes.
 if(!linkedCut())return true;
 if(cached&&request.session==session){
  if(request.sequence==sequence){
   if(request.op==last.op&&request.parameter==last.parameter&&request.value==last.value)replyReady=true;
   return true;
  }
  if(int16_t(request.sequence-sequence)<=0)return true;
 }else if(request.op!=EscWire::Op::Hello||reading)return true;
 if(reading)return true;
 session=request.session;sequence=request.sequence;last=response=request;cached=true;
 if(request.op==EscWire::Op::Hello)reply(EscWire::Status::Ok);
 else if(request.op==EscWire::Op::Prepare)
  reply(er6gStartupPrepare()?EscWire::Status::Ok:EscWire::Status::Rejected);
 else if(request.op==EscWire::Op::Close){
  er6gEscReadAbort();reply(er6gStartupCancel()?EscWire::Status::Ok:EscWire::Status::Rejected);
 }else if(request.op==EscWire::Op::Read&&er6gEscReadSessionReady()&&request.parameter<15){
  writing=false;
  reading=er6gEscReadStart(1,request.parameter,true);
  reply(reading?EscWire::Status::Busy:EscWire::Status::Rejected);
 }else if(request.op==EscWire::Op::Write&&er6gEscReadSessionReady()){
  reading=er6gEscWriteStart(request.parameter,request.value,true);writing=reading;
  reply(reading?EscWire::Status::Busy:EscWire::Status::Rejected);
 }else reply(EscWire::Status::Rejected);
 return true;
}
void er6gEscModeLoop(){
 if(!linkedCut()){
  if(reading)er6gEscReadAbort(); // Abort only the RF-owned transaction, not a Wi-Fi diagnostic read.
  reading=writing=cached=replyReady=false;session=0;return;
 }
 if(reading&&!er6gEscReadBusy()){
  reading=false;uint8_t value=255;
  const bool valid=writing?er6gEscWriteValue(value):er6gEscReadValue(value);response.value=value;writing=false;
  reply(valid?EscWire::Status::Ok:EscWire::Status::Unverified);
 }
 if(replyReady){
  replyReady=false;uint8_t bytes[21];response.flags=flags();
  EscWire::build(response,true,bytes,sizeof(bytes));
  crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER,reinterpret_cast<crsf_header_t*>(bytes));
 }
}
