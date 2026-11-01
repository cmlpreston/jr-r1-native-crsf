#include "ers_sensor_config.h"
#include <cassert>
#include <cstdio>
int main(){
 ErsSensorConfig c;ErsRequest scan{255,1,0,255},q;uint8_t out[16]={};
 assert(buildErsFrame(scan,out,sizeof(out))==10 && out[2]==0x7c && out[3]==0xc8 && out[4]==0xea);
 assert(!buildErsFrame(scan,out,9));
 assert(!validErsRequest({8,1,3,0})); // no reboot/calibration/reset commands
 assert(!validErsRequest({3,1,15,3}));assert(!validErsRequest({8,1,10,0}));
 assert(!c.request({8,1,10,14},0)); // discovered sensors only
 assert(c.request(scan,0));assert(!c.request(scan,1));assert(c.poll(q,0));assert(!c.poll(q,1));
 // IDs are scoped by sensor type, not just numeric ID.
 const uint8_t gps[]={0xea,0xc8,3,1,0,0,'G','P','S',0};
 const uint8_t rpm[]={0xea,0xc8,8,1,0,0,'R','P','M',0};
 c.receive(0x7b,gps,sizeof(gps),10);c.receive(0x7b,rpm,sizeof(rpm),11);assert(c.count()==2);
 assert(!c.poll(q,1999));assert(c.poll(q,2000) && q.id==255 && q.command==9);
 assert(!c.poll(q,2001));assert(c.poll(q,4000) && q.command==9);
 const uint8_t heartbeat[]={0xea,0xc8,4,17,9,0,'C','U',0};
 c.receive(0x7b,heartbeat,sizeof(heartbeat),4001);assert(c.count()==3 && c.device(2).id==17);
 assert(c.poll(q,6000) && q.command==9);c.update(8000);assert(!c.busy());
 assert(c.request({8,1,10,14},4001));assert(c.poll(q,4002));
 assert(buildErsFrame(q,out,sizeof(out))==11 && out[8]==14 && out[9]==0);
 const uint8_t wrong[]={0xea,0xc8,8,1,10,12,0};c.receive(0x7b,wrong,sizeof(wrong),4003);assert(c.busy());
 const uint8_t ack[]={0xea,0xc8,8,1,10,14,0};c.receive(0x7b,ack,sizeof(ack),4004);assert(!c.busy());
 assert(c.device(1).motorPoles==14 && (c.device(1).known&2));
 assert(c.request({8,1,12,255},4100));assert(c.poll(q,4100));
 const uint8_t settings[]={0xea,0xc8,8,1,12,0,10,14,0,14,0,18,0x13,0};
 c.receive(0x7b,settings,sizeof(settings),4101);assert(!c.busy() && (c.device(1).known&8));
 assert(c.request({8,1,12,255},4200));const uint8_t truncated[]={0xea,0xc8,8,1,12,0,10,22};
 c.receive(0x7b,truncated,sizeof(truncated),4201);assert(c.busy() && c.device(1).motorPoles==14);
 c.update(8200);assert(!c.busy());assert(!c.poll(q,8200)); // writes never auto-retried
 assert(c.request(scan,UINT32_MAX-100));c.update(7899);assert(!c.busy());
 assert(c.request(scan,5000));c.cancel();assert(!c.poll(q,5001));
 puts("ERS discovery, command validation, frame encoding, acknowledgements and timeouts passed");
}
