#include "crsf_tx_config.h"
#include "crsf_channel_output.h"
#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <cstdio>
using Bytes=std::vector<uint8_t>;
static void str(Bytes &v,const char *s) { v.insert(v.end(),s,s+strlen(s)+1); }
static Bytes device(uint8_t count=3) {
    Bytes b={0xea,0xee}; str(b,"Ranger Nano");
    b.insert(b.end(),{'E','L','R','S',0,0,0,0,0,0,0,1,count,0}); return b;
}
static Bytes field(uint8_t id,uint8_t type=9,uint8_t value=0,const char *options="10;25;;100;250") {
    Bytes b={0xea,0xee,id,0,0,type}; str(b,type==13 ? "Enable WiFi" : "Max Power");
    if(type==9) {str(b,options);b.insert(b.end(),{value,0,4,0});str(b,"mW");}
    else if(type==13) {b.insert(b.end(),{value,10});str(b,value==3 ? "Enter WiFi?" : "");}
    return b;
}
static void rx(CrsfTxConfig &c,uint8_t type,const Bytes &b,uint32_t now) {c.receive(type,b.data(),b.size(),now);}
static void ready(CrsfTxConfig &c,uint32_t now=0) {
    c.start(now); TxParameterRequest r; assert(c.poll(now,r)&&r.type==0x28);
    rx(c,0x29,device(),now+1);
    for(uint8_t id=1;id<=3;++id) {assert(c.poll(now+id*30,r)&&r.id==id);rx(c,0x2b,field(id,id==3 ? 13:9),now+id*30+1);}
    assert(c.ready());
}
int main() {
    uint8_t wire[64];
    assert(buildTxParameterFrame({0x28},wire,64)==6);
    assert(wire[1]==4 && wire[3]==0xee && wire[4]==0xea);
    assert(wire[5]==crc8DvbS2(wire+2,3));
    assert(buildTxParameterFrame({0x2d,8,4},wire,64)==8 && wire[5]==8 && wire[6]==4);
    assert(wire[7]==crc8DvbS2(wire+2,5));
    assert(!buildTxParameterFrame({0x2d,0,4},wire,64));
    assert(!buildTxParameterFrame({0x2d,1,4},wire,7));
    assert(!buildTxParameterFrame({0x7c,1,4},wire,64));
    CrsfTxConfig c; ready(c); TxParameterRequest r;
    assert(!c.apply(1,2,100)); // blank option is not selectable
    assert(!c.apply(1,5,100)); // range enforced
    assert(c.apply(1,4,100)); assert(!c.apply(2,1,100));
    assert(c.poll(100,r)&&r.type==0x2d&&r.value==4);
    assert(!c.poll(200,r));
    assert(c.poll(350,r)&&r.type==0x2c);
    auto other=field(1,9,4);other[1]=0xec;rx(c,0x2b,other,351);
    assert(!c.ready()); rx(c,0x2b,field(2,9,4),352); // wrong id ignored
    rx(c,0x2b,field(1,9,4),353);
    assert(strstr(c.status(),"verified"));
    assert(c.poll(453,r)&&r.id==1&&r.type==0x2c); // reload dependent fields
    rx(c,0x2b,field(1,9,4),454);
    assert(c.poll(474,r));rx(c,0x2b,field(2),475);
    assert(c.poll(495,r));rx(c,0x2b,field(3,13),496);
    assert(c.ready());
    assert(c.command(3,1,500));assert(c.poll(500,r)&&r.value==1);
    rx(c,0x2b,field(3,13,3),501); assert(c.confirming());
    assert(!c.poll(900,r)); // confirmation requires user action
    assert(!c.command(3,1,901));assert(c.command(3,4,902));
    assert(c.poll(902,r)&&r.value==4);
    assert(c.poll(1102,r)&&r.value==6); // status query, not repeated confirmation
    rx(c,0x2b,field(3,13,2),1103);
    assert(c.command(3,5,1104)); assert(c.poll(1104,r)&&r.value==5);
    rx(c,0x2b,field(3,13,0),1105); assert(!c.busyCommand());
    // Chunked options, duplicates, out-of-order chunks and wrong origins.
    c.start(0); assert(c.poll(0,r));rx(c,0x29,device(1),1);assert(c.poll(2,r));
    Bytes full=field(1); Bytes a(full.begin(),full.begin()+18);a[3]=1;
    Bytes b={0xea,0xee,1,0};b.insert(b.end(),full.begin()+18,full.end());
    rx(c,0x2b,a,3); assert(c.poll(23,r)&&r.value==1);
    rx(c,0x2b,a,24); // duplicate ignored
    rx(c,0x2b,b,25);assert(c.ready());
    assert(c.parameter(1)->value==0);
    char text[20];assert(c.parameter(1)->option(4,text,sizeof(text))&&strcmp(text,"250")==0);
    // Malformed/truncated and hidden entries cannot be written.
    c.start(0);assert(c.poll(0,r));rx(c,0x29,device(1),1);assert(c.poll(2,r));
    auto bad=field(1);bad.pop_back();rx(c,0x2b,bad,3);assert(!c.ready());assert(!c.apply(1,1,4));
    c.start(0);assert(c.poll(0,r));rx(c,0x29,device(1),1);assert(c.poll(2,r));
    auto hidden=field(1);hidden[5]|=0x80;rx(c,0x2b,hidden,3);assert(c.ready());assert(!c.apply(1,1,4));
    // Timeout ends discovery; stale settings cannot be used for writes.
    c.start(0);assert(c.poll(0,r));assert(c.poll(600,r));assert(c.poll(1200,r));assert(!c.poll(1800,r));
    assert(strstr(c.status(),"timed out"));
    ready(c);assert(!c.apply(1,1,5000));
    assert(c.poll(2100,r)&&r.type==0x28);assert(c.poll(2700,r));assert(c.poll(3300,r));assert(!c.poll(3900,r));
    assert(strstr(c.status(),"disconnected"));
    // Device capacity overflow rejected, never silently incomplete.
    c.start(0);assert(c.poll(0,r));rx(c,0x29,device(65),1);assert(strstr(c.status(),"count"));
    // Readback mismatch remains explicit and never retries the write.
    ready(c);assert(c.apply(1,4,100));assert(c.poll(100,r));assert(c.poll(350,r));
    rx(c,0x2b,field(1,9,0),351);assert(strstr(c.status(),"did not accept"));assert(!c.poll(1000,r));
    // Commands may also need chunk assembly before confirmation is visible.
    ready(c);assert(c.command(3,1,100));assert(c.poll(100,r));
    auto cmd=field(3,13,3);Bytes first(cmd.begin(),cmd.begin()+12);first[3]=1;
    Bytes last={0xea,0xee,3,0};last.insert(last.end(),cmd.begin()+12,cmd.end());
    rx(c,0x2b,first,101);assert(!c.confirming());
    assert(c.poll(121,r)&&r.type==0x2d&&r.value==6);
    rx(c,0x2b,last,122);assert(c.confirming());
    // Overlong assembly never overflows its fixed buffer.
    c.start(0);assert(c.poll(0,r));rx(c,0x29,device(1),1);assert(c.poll(2,r));
    Bytes huge(645,1);huge[0]=0xea;huge[1]=0xee;huge[2]=1;huge[3]=0;
    rx(c,0x2b,huge,3);assert(strstr(c.status(),"too large"));
    // Timer arithmetic works over millis wrap.
    ready(c,0xfffffff0U);assert(c.ready());c.stop();assert(!c.poll(500,r));
    puts("CRSF TX configuration tests passed");
}
