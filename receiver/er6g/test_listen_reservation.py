#!/usr/bin/env python3
"""Exercise actual prepared servo functions with GPIO/PWM/RF boundary mocks."""
import argparse,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('source',type=pathlib.Path);a=p.parse_args()
s=a.source.read_text();deferred='static bool er6gDeferredEscPwm(' in s
def function(signature):
 start=s.index(signature);brace=s.index('{',start);depth=1;end=brace+1
 while depth:
  if s[end]=='{':depth+=1
  elif s[end]=='}':depth-=1
  end+=1
 return s[start:end]
pre=r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <vector>
#define PLATFORM_ESP32
#define OPT_HAS_SERVO_OUTPUT true
#define GPIO_PIN_PWM_OUTPUTS_COUNT 6
#define UNDEF_PIN -1
#define DBGLN(...) ((void)0)
#define ICACHE_RAM_ATTR
constexpr int OUTPUT=1,INPUT_PULLUP=2,LOW=0,DURATION_NEVER=-1,DURATION_IMMEDIATELY=0,DISCONNECTED_UPDATE_MS=10;
constexpr uint32_t FAILSAFE_ABS_TIMEOUT_MS=1000;
const int8_t GPIO_PIN_PWM_OUTPUTS[]={14,1,3,15,2,4};
enum eServoOutputMode{som50Hz,som60Hz,som100Hz,som160Hz,som333Hz,som400Hz,som10KHzDuty,somOnOff,somDShot,somDShot3D,somSerial};
struct rx_config_pwm_t {struct{int mode=som50Hz;}val;};
struct Config{rx_config_pwm_t channels[6];const rx_config_pwm_t *GetPwmChannel(int ch){return channels+ch;}}config;
using pwm_channel_t=int;using gpio_num_t=int;using rmt_channel_t=int;
constexpr int DSHOT300=300,RMT_MAX_CHANNELS=8;
bool reserved=true,failEscAllocate=false;
struct DShotRMT {DShotRMT(int pin,int){assert(!reserved||pin!=2);}void begin(int,bool){}};
DShotRMT *dshotInstances[6]={};
int8_t servoPins[6];int pwmChannels[6];uint16_t pwmChannelValues[6];bool initialized=false;
bool er6gEscPwmFrameReady=false,connectionHasModelMatch=true,teamraceHasModelMatch=false;
volatile bool newChannelsAvailable=false;uint32_t lastUpdate=0;
uint8_t getLq(){return 100;}
struct Io{char kind;int pin,value;};std::vector<Io> io;
int pinModes[64]={};bool pwmAlive[64]={};
unsigned allocated=0,released=0,writes=0;
struct Pwm{
 int allocate(int pin,int hz){assert(!reserved||pin!=2);io.push_back({'A',pin,hz});
  if(pin==2&&failEscAllocate)return -1;
  ++allocated;pwmAlive[pin]=true;pinModes[pin]=3;return pin;}
 int allocateLedc(int pin,int hz,int initialUs){assert(pin==2&&initialUs>0);return allocate(pin,hz);}
 void release(int ch){assert(ch>=0&&(!reserved||ch!=2)&&pwmAlive[ch]);pwmAlive[ch]=false;++released;io.push_back({'R',ch,0});}
 void setDuty(int ch,int value){assert(ch>=0&&(!reserved||ch!=2)&&pwmAlive[ch]);++writes;io.push_back({'W',ch,value});}
 void setMicroseconds(int ch,int value){assert(ch>=0&&(!reserved||ch!=2)&&pwmAlive[ch]);++writes;io.push_back({'W',ch,value});}
 void feedWatchdog(){}
}PWM;
void pinMode(int pin,int mode){assert(!reserved||pin!=2);pinModes[pin]=mode;io.push_back({'M',pin,mode});}
void digitalWrite(int pin,int v){assert(!reserved||pin!=2);io.push_back({'D',pin,v});}
int constrain(int value,int low,int high){return value<low?low:value>high?high:value;}
bool er6gEscListenOwnsPin(int pin){return reserved&&pin==2;}
void servoWriteDshot(eServoOutputMode,int ch,uint16_t){assert(!reserved||ch!=4);}
void servosEnterFailsafe();void servosFailsafe();
enum State{connected,disconnected,wifiUpdate};State connectionState=disconnected;
using servoWrite_fn=void(*)(uint8_t,uint16_t);
uint16_t commandUs[6]={1500,1500,1500,1500,1500,1500},failsafeUs[6]={1500,1500,1500,1500,1500,1500};
bool commandValid[6]={true,true,true,true,true,true};
void servoCalcAllChannels(servoWrite_fn write){for(int ch=0;ch<6;++ch)if(commandValid[ch])write(ch,commandUs[ch]);}
'''
sigs=['uint16_t servoOutputModeToFrequency(']
if deferred:sigs+=['static bool er6gDeferredEscPwm(']
sigs+=['static void servoWrite(']
if deferred:sigs+=['void ICACHE_RAM_ATTR servoNewChannelsAvailable()', 'static void servosUpdate(']
sigs+=['static bool initialize()', 'static int event()']
functions='\n'.join(function(x) for x in sigs)
post=r'''
void servosFailsafe(){for(int ch=0;ch<6;++ch)servoWrite(ch,failsafeUs[ch]);}
void servosEnterFailsafe(){newChannelsAvailable=false;lastUpdate=0;if(initialized)servosFailsafe();}
void reset(bool reserve){
 reserved=reserve;allocated=released=writes=0;initialized=false;er6gEscPwmFrameReady=false;
 connectionState=disconnected;connectionHasModelMatch=true;newChannelsAvailable=false;lastUpdate=0;failEscAllocate=false;io.clear();
 for(int p=0;p<64;++p){pwmAlive[p]=false;pinModes[p]=0;}
 for(int ch=0;ch<6;++ch){config.channels[ch].val.mode=som50Hz;dshotInstances[ch]=nullptr;commandUs[ch]=failsafeUs[ch]=1500;commandValid[ch]=true;}
}
int main(){
 for(bool reserve:{true,false})for(auto mode:{som50Hz,som10KHzDuty,somOnOff,somDShot,somDShot3D,somSerial}){
  if(!reserve&&mode!=som50Hz)continue;
  reset(reserve);config.channels[4].val.mode=mode;
  assert(initialize());assert(reserve?(servoPins[4]==UNDEF_PIN&&pwmChannels[4]==-1):(servoPins[4]==2&&pwmChannels[4]==-1));
  connectionState=connected;event();assert(allocated==(reserve||deferred?5u:6u));
  er6gEscPwmFrameReady=true;for(int ch=0;ch<6;++ch)servoWrite(ch,1700);er6gEscPwmFrameReady=false;
  assert(writes==(reserve?5u:6u));
  connectionState=disconnected;event();
  connectionState=wifiUpdate;event();assert(released==(reserve?5u:6u));
 }
'''
if deferred:
 post+=r'''
 // Real servoUpdate/event paths: a connection event is NOT a valid command.
 for(auto mode:{som50Hz,som60Hz,som100Hz,som160Hz,som333Hz,som400Hz}){
  reset(false);config.channels[4].val.mode=mode;assert(initialize());
  assert(pinModes[2]==INPUT_PULLUP&&pwmChannels[4]==-1);
  for(const auto &e:io)if(e.pin==2)assert(e.kind=='M'&&e.value==INPUT_PULLUP);
  servosUpdate(10);assert(!pwmAlive[2]); // disconnected
  connectionState=connected;event();assert(allocated==5&&!pwmAlive[2]);
  connectionHasModelMatch=false;servoNewChannelsAvailable();servosUpdate(20);
  assert(!pwmAlive[2]&&pinModes[2]==INPUT_PULLUP); // failsafe before valid input
  connectionHasModelMatch=true;servosUpdate(30);assert(!pwmAlive[2]);
  commandValid[4]=false;servoNewChannelsAvailable();servosUpdate(40);assert(!pwmAlive[2]); // unset channel
  commandValid[4]=true;failEscAllocate=true;servoNewChannelsAvailable();servosUpdate(50);
  assert(!pwmAlive[2]&&pwmChannels[4]==-1&&pinModes[2]==INPUT_PULLUP);
  failEscAllocate=false;servoNewChannelsAvailable();servosUpdate(60);
  assert(pwmAlive[2]&&pwmChannels[4]==2&&!er6gEscPwmFrameReady);
  assert(io.size()>=2);auto end=io.size();
  assert(io[end-2].kind=='A'&&io[end-2].pin==2&&io[end-1].kind=='W'&&io[end-1].pin==2&&io[end-1].value==1500);
  // Shared RF/model gates remain; obsolete Team Race selection does not block single-RX commands.
  assert(!teamraceHasModelMatch);commandUs[4]=1700;servoNewChannelsAvailable();servosUpdate(70);
  assert(pwmChannelValues[4]==1700);
  auto unchanged=writes;servoNewChannelsAvailable();servosUpdate(80);assert(writes==unchanged);
  failsafeUs[4]=1300;servosUpdate(1200);assert(pwmChannelValues[4]==1300&&lastUpdate==0);
  // Keep configured no-pulse and positional failsafe after PWM is active.
  failsafeUs[4]=0;servosFailsafe();assert(pwmChannelValues[4]==0&&pwmAlive[2]);
  assert(pwmAlive[14]&&pwmAlive[1]); // no timer releases on one channel's no-pulse
  servoNewChannelsAvailable();servosUpdate(1300);assert(pwmChannelValues[4]==1700);
  connectionHasModelMatch=false;failsafeUs[4]=1400;servosUpdate(1400);assert(pwmChannelValues[4]==1400);
  connectionState=wifiUpdate;event();assert(released==6&&pinModes[2]==INPUT_PULLUP&&servoPins[4]==UNDEF_PIN);
 }
 // A no-pulse failsafe before the first usable command leaves the data wire released.
 reset(false);failsafeUs[4]=0;initialize();connectionState=connected;event();
 connectionHasModelMatch=false;servosUpdate(10);assert(pwmChannels[4]==-1&&pinModes[2]==INPUT_PULLUP);
 puts("Actual single-RX ESC PWM: startup released, no allocation until usable model-matched frame, immediate first pulse, allocation failure, every50..400Hz mode, no-pulse/position/link/model/time failsafes, Wi-Fi release and other outputs PASS");
'''
post+=' puts("Actual reserved GPIO2: no PWM/duty/digital/DShot/serial writes; other five outputs preserved PASS");\n}\n'
with tempfile.TemporaryDirectory(prefix='er6g-pwm-test-') as d:
 f=pathlib.Path(d)/'test.cpp';f.write_text(pre+'\nconstexpr bool deferred='+str(deferred).lower()+';\n'+functions+post);binary=pathlib.Path(d)/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(f),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
