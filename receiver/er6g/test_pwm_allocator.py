#!/usr/bin/env python3
"""Exercise the actual prepared ESP32 allocator: ESC never resets a shared MCPWM timer."""
import pathlib,argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('source',type=pathlib.Path);a=p.parse_args();s=a.source.read_text()
def function(signature):
 start=s.index(signature);brace=s.index('{',start);end=brace+1;depth=1
 while depth:
  if s[end]=='{':depth+=1
  elif s[end]=='}':depth-=1
  end+=1
 return s[start:end]
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#define SOC_MCPWM_SUPPORTED 1
#define DBGLN(...) ((void)0)
constexpr int MCPWM_CHANNELS=12,LEDC_CHANNELS=8,LEDC_TIMER_MAX=4,LEDC_TIMER_BIT_MAX=20;
constexpr int MCPWM_CHANNEL_FLAG=0x100,LEDC_CHANNEL_FLAG=0x200;
constexpr int ESP_OK=0,MCPWM_DUTY_MODE_0=0,MCPWM_UP_COUNTER=0;
#define IS_LEDC_CHANNEL(ch) ((ch)&LEDC_CHANNEL_FLAG)
#define IS_MCPWM_CHANNEL(ch) ((ch)&MCPWM_CHANNEL_FLAG)
#define LEDC_CHANNEL(ch) ((ch)&0xff)
#define MCPWM_CHANNEL(ch) ((ch)&0xff)
using pwm_channel_t=int;using ledc_timer_t=int;
struct MConfig{int unit,signal,timer,generator;};
MConfig mcpwm_config[MCPWM_CHANNELS]={};uint32_t mcpwm_frequencies[MCPWM_CHANNELS]={};
struct LConfig{int8_t pin;uint8_t resolution_bits;uint32_t interval;bool attached;uint8_t tmr_idx;};
LConfig ledc_config[LEDC_CHANNELS]={};uint32_t ledcTimerConfigs[LEDC_TIMER_MAX]={};
struct mcpwm_config_t {uint32_t frequency;float cmpr_a;int duty_mode,counter_mode;};
unsigned mcpwmRoutes=0,mcpwmInits=0,ledcRoutes=0,ledcInits=0,ledcWrites=0;
uint32_t lastRaw=0,lastInitialRaw=0;
int mcpwm_gpio_init(int,int,int){++mcpwmRoutes;return ESP_OK;}
void mcpwm_init(int,int,mcpwm_config_t*){++mcpwmInits;}
void mcpwm_set_duty_in_us(int,int,int,uint16_t){assert(false);}
void ledcSetupEx(int,int,uint32_t,int){++ledcInits;}
void ledcAttachPinEx(int,int,int,uint32_t initial=0){++ledcRoutes;lastInitialRaw=initial;}
void ledcDetachPin(int){}
void ledcWrite(int,uint32_t v){++ledcWrites;lastRaw=v;}
long map(long v,long a,long b,long c,long d){return (v-a)*(d-c)/(b-a)+c;}
class PWMController{public:pwm_channel_t allocate(uint8_t,uint32_t);pwm_channel_t allocateLedc(uint8_t,uint32_t,uint16_t);void setMicroseconds(pwm_channel_t,uint16_t);};
'''
body='\n'.join(function(n) for n in ('static pwm_channel_t allocatePwm(', 'pwm_channel_t PWMController::allocate(', 'pwm_channel_t PWMController::allocateLedc(', 'static void setLedcMapped(', 'void PWMController::setMicroseconds('))
post=r'''
void reset(){memset(mcpwm_frequencies,0,sizeof(mcpwm_frequencies));memset(ledc_config,0,sizeof(ledc_config));memset(ledcTimerConfigs,0,sizeof(ledcTimerConfigs));mcpwmRoutes=mcpwmInits=ledcRoutes=ledcInits=ledcWrites=0;}
int main(){PWMController pwm;
 reset();auto legacy=pwm.allocate(14,50);assert(legacy==MCPWM_CHANNEL_FLAG&&mcpwmInits==1&&mcpwmRoutes==1&&ledcRoutes==0);
 auto esc=pwm.allocateLedc(2,50,1500);assert(esc==LEDC_CHANNEL_FLAG&&mcpwmInits==1&&mcpwmRoutes==1&&ledcRoutes==1&&ledcInits==1);
 pwm.setMicroseconds(esc,1500);assert(ledcWrites==1&&mcpwmInits==1&&lastInitialRaw==lastRaw&&lastInitialRaw>0);
 double actual=double(lastRaw)*ledc_config[0].interval/((1u<<ledc_config[0].resolution_bits)-1);
 assert(fabs(actual-1500)<0.1);
 // Existing LEDC timer and channel remain untouched when their frequency is reused.
 auto saved=ledc_config[0];auto inits=ledcInits;auto other=pwm.allocateLedc(4,50,1400);
 assert(other==(LEDC_CHANNEL_FLAG|1)&&ledcInits==inits&&memcmp(&saved,&ledc_config[0],sizeof(saved))==0&&mcpwmInits==1);
 reset();for(auto &f:mcpwm_frequencies)f=50; // completely occupied MCPWM resources must be ignored
 assert(pwm.allocateLedc(2,400,1500)==LEDC_CHANNEL_FLAG&&mcpwmInits==0&&mcpwmRoutes==0);
 reset();for(auto &c:ledc_config)c.resolution_bits=1;
 assert(pwm.allocateLedc(2,50,1500)==-1&&mcpwmInits==0&&mcpwmRoutes==0&&ledcRoutes==0); // no fallback that could reset another timer
 puts("Actual ESP32 PWM allocator: legacy MCPWM retained, ESC LEDC-only, shared timers/other channels unchanged, accurate1500us, occupied resources/failure paths PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='er6g-allocator-test-') as d:
 src=pathlib.Path(d)/'test.cpp';src.write_text(pre+body+post);out=pathlib.Path(d)/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(src),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True)
