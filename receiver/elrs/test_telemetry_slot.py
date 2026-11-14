#!/usr/bin/env python3
"""Compile and exercise the actual prepared RXdoneISR with mocked RF boundaries."""
import argparse,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('source',type=pathlib.Path);a=p.parse_args()
s=a.source.read_text();start=s.index('bool ICACHE_RAM_ATTR RXdoneISR(');end=s.index('void ICACHE_RAM_ATTR TXdoneISR()',start);isr=s[start:end]
pre='''#include <cstdint>
#include <cassert>
#include <cstdio>
#define ICACHE_RAM_ATTR
struct SX12xxDriverCommon{enum rx_status{Ok};};
enum State{connected,tentative,disconnected};State connectionState=connected;
unsigned ExpressLRS_currTlmDenom=1;uint8_t OtaNonce=0;
unsigned processCalls=0,resumeCalls=0;bool processResult=true,doStartTimer=false;
struct LQ{bool busy=false;bool currentIsSet(){return busy;}}LQCalc;
bool ProcessRFPacket(SX12xxDriverCommon::rx_status){++processCalls;return processResult;}
namespace hwTimer{void resume(){++resumeCalls;}}
'''
post='''
int main(){
 const unsigned ratios[]={1,2,4,8,16,32,64,128};
 for(State state:{connected,tentative,disconnected})for(auto ratio:ratios){
  bool telemetrySlots[256]={};if(ratio>1)for(unsigned n=0;n<256;n+=ratio)telemetrySlots[n]=true;
  for(unsigned n=0;n<256;++n){connectionState=state;ExpressLRS_currTlmDenom=ratio;OtaNonce=n;
   LQCalc.busy=false;processCalls=resumeCalls=0;processResult=true;doStartTimer=true;
   const bool skip=state==connected&&telemetrySlots[n];
   assert(RXdoneISR(SX12xxDriverCommon::Ok)==!skip);
   assert(processCalls==unsigned(!skip)&&resumeCalls==unsigned(!skip));
  }
 }
 connectionState=connected;ExpressLRS_currTlmDenom=1;LQCalc.busy=true;processCalls=0;
 assert(!RXdoneISR(SX12xxDriverCommon::Ok)&&processCalls==0);
 LQCalc.busy=false;processResult=false;processCalls=resumeCalls=0;doStartTimer=true;
 assert(!RXdoneISR(SX12xxDriverCommon::Ok)&&processCalls==1&&resumeCalls==0);
 puts("Actual RXdoneISR: connected/disconnected/binding acquisition, all256 nonces, 8 telemetry ratios, telemetry-off, duplicate and timer guards PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='elrs-slot-test-') as d:
 f=pathlib.Path(d)/'test.cpp';f.write_text(pre+isr+post);binary=pathlib.Path(d)/'test'
 subprocess.run(['c++','-std=c++17','-include','initializer_list',str(f),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
