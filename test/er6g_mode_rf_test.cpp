#define ER6G_ESC_DRIVE_MODE 1
#define ER6G_ESC_WRITES 1
#include "er4_stubs/er4_mocks.h"
#include "../receiver/er6g/reader/Er6gEscMode.cpp"
#include <cstdio>
#include "surpass_protocol.h"
bool program=false,pendingMode=false,validSession=false,busyRead=false,readOk=true;
unsigned writeCount=0;uint8_t writtenValue=0;
unsigned prepareCount=0,closeCount=0,readCount=0,abortCount=0;
bool er6gStartupProgramming(){return program;}
bool er6gStartupPending(){return pendingMode;}
bool er6gStartupPrepare(){++prepareCount;pendingMode=true;return true;}
bool er6gStartupCancel(){++closeCount;pendingMode=false;return true;}
bool er6gEscReadSessionReady(){return program&&validSession;}
bool er6gEscReadStart(uint8_t op,uint8_t p,bool confirm){assert(op==1&&p<15&&confirm);++readCount;return busyRead=true;}
bool er6gEscWriteStart(uint8_t p,uint8_t v,bool confirmed){if(!confirmed||!Surpass::valid(p,v))return false;++writeCount;writtenValue=v;return busyRead=true;}
bool er6gEscWriteValue(uint8_t &v){v=writtenValue;return readOk;}
bool er6gEscReadBusy(){return busyRead;}
bool er6gEscReadValue(uint8_t &v){v=1;return readOk;}
void er6gEscReadAbort(){++abortCount;busyRead=false;}
int main(){
 EscWire::Message m;m.session=44;m.sequence=1;
 auto send=[&](){uint8_t f[21];const size_t n=EscWire::build(m,false,f,sizeof(f));assert(n);assert(er6gEscModeHandle(reinterpret_cast<crsf_header_t*>(f)));er6gEscModeLoop();};
 auto result=[](){EscWire::Message r;auto &f=crsfRouter.replies.back();assert(EscWire::parse(f[2],f.data()+3,17,true,r));return r;};
 connectionState=connected;connectionHasModelMatch=true;isArmed=true;send();assert(crsfRouter.replies.empty()&&prepareCount==0);
 isArmed=false;send();assert(result().flags&EscWire::Drive);
 m.op=EscWire::Op::Prepare;++m.sequence;send();assert(prepareCount==1&&result().flags&EscWire::Pending);
 send();assert(prepareCount==1); // exact duplicate never repeats persistent mode command
 m.op=EscWire::Op::Write;++m.sequence;send();assert(result().status==EscWire::Status::Rejected&&readCount==0);
 m.op=EscWire::Op::Read;++m.sequence;send();assert(result().status==EscWire::Status::Rejected&&readCount==0);
 program=validSession=true;pendingMode=false;++m.sequence;send();assert(readCount==1&&result().status==EscWire::Status::Busy);
 busyRead=false;er6gEscModeLoop();assert(result().status==EscWire::Status::Ok&&result().value==1);
 m.op=EscWire::Op::Write;m.parameter=8;m.value=1;++m.sequence;send();assert(writeCount==1&&result().status==EscWire::Status::Busy);send();assert(writeCount==1);
 busyRead=false;er6gEscModeLoop();assert(result().status==EscWire::Status::Ok&&result().value==1);send();assert(writeCount==1);
 m.op=EscWire::Op::Close;++m.sequence;send();assert(closeCount==1&&program&&(result().flags&EscWire::Programming));
 m.op=EscWire::Op::Read;++m.sequence;send();connectionState=disconnected;er6gEscModeLoop();assert(!busyRead&&!cached&&!replyReady&&program);
 // Entering Wi-Fi must not cancel an independently gated Wi-Fi bench read.
 connectionState=wifiUpdate;busyRead=true;const auto before=abortCount;er6gEscModeLoop();
 assert(busyRead&&abortCount==before);busyRead=false;
 puts("Actual ER6G RF mode adapter: CUT/link/model gates, command deduplication, writes/Drive reads rejected, async read, close preserves pin lock, link loss abort PASS");
}
