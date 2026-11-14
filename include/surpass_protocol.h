#pragma once
#include <stdint.h>
#include <stddef.h>

// Measured Supersonic V2 card protocol, not generic Hobbywing programming.
// Reference: cmlpreston/surpass-esc-programming-protocol/docs/protocol.md.
namespace Surpass {
constexpr uint8_t ParameterCount=15;
inline uint8_t choices(uint8_t id) {
    const uint8_t counts[]={9,6,5,4,3,7,7,3,5,9,9,6,6,2,3};
    return id<ParameterCount?counts[id]:0;
}
inline bool valid(uint8_t id,uint8_t value){return value<choices(id);}
inline const char *name(uint8_t id) {
    const char *const names[]={"Low voltage cutoff","Start / punch","Maximum brake","Maximum reverse","Neutral range","Motor thermal limit","ESC thermal limit","Run mode","Drag brake","Boost timing","Turbo timing","Motor PWM","Brake PWM","Motor rotation","BEC voltage"};
    return id<ParameterCount?names[id]:"Unknown";
}
inline const char *choice(uint8_t id,uint8_t v) {
    if(!valid(id,v))return "--";
    const char *const cutoff[]={"3.3 V/cell","3.4 V/cell","3.5 V/cell","3.6 V/cell","3.7 V/cell","3.2 V/cell","3.1 V/cell","3.0 V/cell","2.9 V/cell"};
    const char *const punch[]={"Level 3","Level 1","Level 6","Level 2","Level 4","Level 5"};
    const char *const brake[]={"50%","25%","75%","100%","Disabled"};
    const char *const reverse[]={"25%","50%","75%","100%"};
    const char *const neutral[]={"9% Normal","6% Narrow","12% Wide"};
    const char *const motorHeat[]={"120 C Stop","135 C Stop","150 C Stop","120 C Slow","135 C Slow","150 C Slow","Disabled"};
    const char *const escHeat[]={"105 C Stop","115 C Stop","125 C Stop","105 C Slow","115 C Slow","125 C Slow","Disabled"};
    const char *const mode[]={"Fwd/rev/brake","Fwd/brake","Fwd/reverse"};
    const char *const drag[]={"0%","5%","10%","20%","30%"};
    const char *const boost[]={"Off","4 deg","8 deg","12 deg","16 deg","20 deg","24 deg","32 deg","36 deg"};
    const char *const turbo[]={"Off","16 deg","20 deg","24 deg","28 deg","34 deg","42 deg","52 deg","60 deg"};
    const char *const pwm[]={"8 kHz","2 kHz","4 kHz","12 kHz","16 kHz","24 kHz"};
    const char *const brakePwm[]={"1 kHz","400 Hz","600 Hz","800 Hz","2 kHz","4 kHz"};
    const char *const rotation[]={"CCW","CW"};
    const char *const bec[]={"6.0 V","7.4 V","8.4 V"};
    const char *const *const tables[]={cutoff,punch,brake,reverse,neutral,motorHeat,escHeat,mode,drag,boost,turbo,pwm,brakePwm,rotation,bec};
    return tables[id][v];
}
inline size_t request(bool write,uint8_t id,uint8_t value,uint8_t out[3]) {
    if(!out||id>=ParameterCount||(write&&!valid(id,value)))return 0;
    out[0]=write?0x1b:0x1c;out[1]=id;out[2]=write?value:0;return 3;
}
}
