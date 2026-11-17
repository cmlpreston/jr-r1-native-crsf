#include "crsf_telemetry.h"
#include "crsf_channel_output.h"
#include "ers_sensor_config.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

static uint32_t nowMs=0;
uint32_t millis(){return nowMs;}
static void feed(CrsfTelemetryParser &parser,uint8_t type,std::vector<uint8_t> payload,bool bad=false){
    std::vector<uint8_t> frame={0xc8,uint8_t(payload.size()+2),type};
    frame.insert(frame.end(),payload.begin(),payload.end());
    frame.push_back(crc8DvbS2(frame.data()+2,frame.size()-2)^uint8_t(bad));
    for(auto byte:frame)parser.feed(byte);
}
static void burst(){
    // All possible arrival orders, with no consumer until all four CRC-valid
    // replies have passed through the actual CRSF parser and a snapshot copy.
    std::vector<uint8_t> types={2,3,4,8};
    do{
        CrsfTelemetryParser parser;ErsSensorConfig config;ErsRequest scan;
        scan.type=255;scan.id=1;scan.command=0;scan.value=255;
        assert(config.request(scan,1000));nowMs=1001;
        for(auto type:types)feed(parser,0x7b,{0xea,0xc8,type,uint8_t(type+1),0,0,'E','R','S',0});
        const auto snapshot=parser.state();assert(snapshot.ersSequence==4&&snapshot.ersReplyDrops==0);
        ErsReply reply;unsigned received=0;
        while(parser.popErsReply(reply)){
            assert(reply.payload[2]==types[received++]&&reply.receivedMs==1001);
            config.receive(reply.frameType,reply.payload,reply.length,reply.receivedMs);
        }
        assert(received==4&&config.count()==4);
        // A repeated heartbeat must update the same devices, without duplicates.
        nowMs=3000;for(auto type:types)feed(parser,0x7b,{0xea,0xc8,type,uint8_t(type+1),9,0,'E','R','S',0});
        while(parser.popErsReply(reply))config.receive(reply.frameType,reply.payload,reply.length,reply.receivedMs);
        assert(config.count()==4);
        for(unsigned n=0;n<4;++n)assert(config.device(n).updatedMs==3000);
        // A settings acknowledgement must survive another device replying last.
        config.cancel();ErsRequest write;write.type=8;write.id=9;write.command=10;write.value=4;
        assert(config.request(write,3100));nowMs=3101;
        feed(parser,0x7b,{0xea,0xc8,8,9,10,4,0});feed(parser,0x7b,{0xea,0xc8,3,4,9,0});
        while(parser.popErsReply(reply))config.receive(reply.frameType,reply.payload,reply.length,reply.receivedMs);
        assert(!config.busy());
        bool found=false;for(unsigned n=0;n<4;++n)if(config.device(n).type==8){assert(config.device(n).motorPoles==4);found=true;}
        assert(found);
    }while(std::next_permutation(types.begin(),types.end()));
}
static void invalidAndOverflow(){
    CrsfTelemetryParser parser;ErsReply reply;nowMs=100;
    feed(parser,0x7b,{0xea,0xc8,3,1,0,0},true);
    feed(parser,0x7b,{0xec,0xc8,3,1,0,0});feed(parser,0x7b,{0xea,0xc8,3,1,0});
    assert(!parser.popErsReply(reply)&&!parser.state().ersSequence);
    for(unsigned n=0;n<ErsReplyQueue::Capacity+3;++n)
        feed(parser,0x7b,{0xea,0xc8,3,uint8_t(n+1),0,0});
    assert(parser.state().ersReplyDrops==3&&parser.ersReplyDrops()==3);
    for(unsigned n=0;n<ErsReplyQueue::Capacity;++n){assert(parser.popErsReply(reply));assert(reply.payload[3]==n+1);}
    assert(!parser.popErsReply(reply));
    feed(parser,0x7b,{0xea,0xc8,3,99,9,0});assert(parser.popErsReply(reply)&&reply.payload[3]==99);
    ErsSensorConfig config;ErsRequest scan;scan.type=255;scan.id=1;scan.value=255;
    assert(config.request(scan,1000));const uint8_t gps[]={0xea,0xc8,3,1,0,0};
    config.receive(0x7b,gps,sizeof(gps),999);assert(!config.count());
    config.receive(0x7b,gps,sizeof(gps),9000);assert(!config.count());
    config.receive(0x7b,gps,sizeof(gps),1001);assert(config.count()==1);
    config.cancel();ErsRequest write;write.type=3;write.id=1;write.command=15;write.value=2;
    assert(config.request(write,2000));const uint8_t ack[]={0xea,0xc8,3,1,15,2};
    config.receive(0x7b,ack,sizeof(ack),1999);assert(config.busy()&&!(config.device(0).known&1));
    config.receive(0x7b,ack,sizeof(ack),6000);assert(config.busy()&&!(config.device(0).known&1));
    config.receive(0x7b,ack,sizeof(ack),2001);assert(!config.busy()&&config.device(0).gpsMode==2);
    assert(config.request(scan,UINT32_MAX-500));
    config.receive(0x7b,gps,sizeof(gps),UINT32_MAX-501);assert(!config.count());
    config.receive(0x7b,gps,sizeof(gps),200);assert(config.count()==1);
    config.receive(0x7b,gps,sizeof(gps),1001);assert(config.count()==1);
    config.cancel();assert(config.request(scan,UINT32_MAX-20));
    config.receive(0x7b,gps,sizeof(gps),5);assert(config.count()==1);
    ErsReplyQueue wrap(UINT32_MAX-4);
    for(unsigned n=0;n<40;++n){assert(wrap.push(0x7b,gps,sizeof(gps),n));assert(wrap.pop(reply)&&reply.receivedMs==n);}
}
static void concurrent(){
    ErsReplyQueue queue;
    std::thread producer([&](){
        const uint8_t payload[]={0xea,0xc8,3,1,9,0};
        for(uint32_t n=0;n<20000;++n)while(!queue.push(0x7b,payload,sizeof(payload),n))std::this_thread::yield();
    });
    ErsReply reply;
    for(uint32_t n=0;n<20000;++n){while(!queue.pop(reply))std::this_thread::yield();assert(reply.receivedMs==n&&reply.length==6&&reply.payload[4]==9);}
    producer.join();assert(!queue.pop(reply));
}
int main(){burst();invalidAndOverflow();concurrent();puts("ERS transport: all four sensor types in 24 burst orders, heartbeat deduplication, settings ACK, CRC/envelope filtering, overflow/recovery, stale scans, wrap and concurrent FIFO PASS");}
