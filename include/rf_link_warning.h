#pragma once
#include <stdint.h>

// Installed Ranger Nano is 2.4 GHz. RFMD numbering differs by ELRS major version.
// Official tables: https://www.expresslrs.org/info/signal-health/#rf-mode-indexes-rfmd
inline int elrs24Sensitivity(uint8_t mode,uint8_t major){
    if(major==4)switch(mode){
        case 21:return -115;
        case 23:case 24:return -112;
        case 27:return -108;
        case 28:case 29:return -105;
        case 30:case 31:case 32:case 33:return -104;
        case 34:case 35:case 36:return -103;
        default:return 0;
    }
    if(major==3)switch(mode){
        case 2:return -115;
        case 4:case 5:return -112;
        case 7:return -108;
        case 8:case 9:return -105;
        case 10:case 11:case 12:case 13:return -104;
        case 16:case 19:return -101;
        default:return 0;
    }
    return 0;
}
struct RfLinkWarningSettings {
    bool enabled=true;
    uint8_t elrsMajor=4,marginDb=8,minimumLq=90;
};
class RfWarningLatch {
    bool pending_=false,active_=false;
    uint32_t since_=0,next_=0;
public:
    bool active()const{return active_;}
    bool update(bool available,bool bad,bool recovered,uint32_t now){
        if(!available || recovered){pending_=active_=false;return false;}
        if(!bad && !active_){pending_=false;return false;}
        if(!pending_){pending_=true;since_=now;}
        if(!active_ && uint32_t(now-since_)>=1000){active_=true;next_=now;}
        if(active_ && int32_t(now-next_)>=0){next_=now+5000;return true;}
        return false;
    }
};
