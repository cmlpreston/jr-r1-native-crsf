#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// RadioMaster SENSOR_LUA V1.3 command envelope (not encapsulated MSP).
// Protocol reference: RadioMaster's SENSOR_LUA_2026_7_31.zip. No Lua code copied.
struct ErsRequest { uint8_t type=0, id=0, command=0; uint16_t value=0; };
inline bool validErsRequest(const ErsRequest &r) {
    if(r.type==255)return r.id==1 && r.command==0 && r.value==255;
    if((r.type!=2 && r.type!=3 && r.type!=4 && r.type!=8) || r.id==0 || r.id==255)return false;
    if(r.command==12)return r.value==255;
    if(r.type==3 && r.command==15)return r.value==1 || r.value==2;
    if(r.type==8 && r.command==10)return r.value>=1 && r.value<=100;
    if(r.type==4 && r.command==17)return r.value>=1;
    return false; // No calibration, power, reset or firmware-update commands.
}
inline size_t buildErsFrame(const ErsRequest &r,uint8_t *out,size_t capacity) {
    if(!out || capacity<11 || !validErsRequest(r))return 0;
    const bool word=r.command==10 || r.command==17;
    const size_t n=word?11:10;
    out[0]=0xc8;out[1]=n-2;out[2]=0x7c;out[3]=0xc8;out[4]=0xea;
    out[5]=r.type;out[6]=r.id;out[7]=r.command;out[8]=uint8_t(r.value);
    if(word)out[9]=r.value>>8;
    uint8_t crc=0;for(size_t i=2;i<n-1;++i){crc^=out[i];for(int b=0;b<8;++b)crc=(crc&128)?uint8_t((crc<<1)^0xd5):uint8_t(crc<<1);}
    out[n-1]=crc;return n;
}
struct ErsDevice {
    uint8_t type=0,id=0,gpsMode=0,telemetryId=0;
    uint16_t motorPoles=0,capacityMah=0;
    uint8_t known=0; // bits: mode, poles, capacity, telemetry ID
    uint32_t updatedMs=0;
    char name[32]={};
};
class ErsSensorConfig {
public:
    static constexpr uint8_t MaxDevices=8;
    bool request(const ErsRequest &r,uint32_t now) {
        update(now);
        if(busy_ || !validErsRequest(r))return false;
        if(r.type!=255 && !find(r.type,r.id))return false;
        if(r.type==255){count_=0;memset(devices_,0,sizeof(devices_));}
        pending_=r;busy_=true;queued_=true;started_=now;
        status_=r.type==255?"Discovering sensors":"Queued; awaiting sensor acknowledgement";
        return true;
    }
    bool poll(ErsRequest &r,uint32_t now) { update(now);if(!queued_)return false;r=pending_;queued_=false;return true; }
    void update(uint32_t now) {
        if(busy_ && uint32_t(now-started_)>=4000){busy_=queued_=false;status_=pending_.type==255?(count_?"Discovery complete":"No ERS sensors replied"):"No acknowledgement; refresh settings before retrying";}
    }
    void cancel() {if(busy_){busy_=queued_=false;status_="Cancelled; refresh settings before retrying";}}
    void receive(uint8_t frameType,const uint8_t *p,size_t n,uint32_t now) {
        if((frameType!=0x7a && frameType!=0x7b && frameType!=0x7c) || n<6 || p[0]!=0xea || p[1]!=0xc8)return;
        if(p[2]!=2 && p[2]!=3 && p[2]!=4 && p[2]!=8)return;
        if(!p[3] || p[3]==255)return;
        auto *d=find(p[2],p[3]);
        if(p[4]==0 && busy_ && pending_.type==255) {
            if(!d){if(count_==MaxDevices)return;d=&devices_[count_++];d->type=p[2];d->id=p[3];}
            d->updatedMs=now;
            size_t i=0;while(6+i<n && i<sizeof(d->name)-1 && p[6+i]>=32 && p[6+i]<=126){d->name[i]=char(p[6+i]);++i;}d->name[i]=0;
            return;
        }
        if(!d || !busy_ || pending_.type!=p[2] || pending_.id!=p[3] || pending_.command!=p[4])return;
        if(p[4]==12){
            // Validate the entire command/value list before publishing any settings.
            ErsDevice draft=*d;
            for(size_t i=6;i<n;) {
                uint8_t c=p[i++];const bool word=c==6 || c==10 || c==17 || c==18;
                if(i+(word?2:1)>n)return;
                const uint16_t v=p[i] | (word?uint16_t(p[i+1])<<8:0);i+=word?2:1;
                set(draft,c,v);
            }
            *d=draft;
        } else {
            const bool word=p[4]==10 || p[4]==17;if(word && n<7)return;
            const uint16_t v=p[5] | (word?uint16_t(p[6])<<8:0);
            if(v!=pending_.value)return;
            set(*d,p[4],v);
        }
        d->updatedMs=now;busy_=queued_=false;status_="Sensor confirmed";
    }
    const ErsDevice &device(uint8_t i)const{return devices_[i<MaxDevices?i:0];}
    uint8_t count()const{return count_;}
    bool busy()const{return busy_;}
    const char *status()const{return status_;}
private:
    ErsDevice *find(uint8_t type,uint8_t id){for(uint8_t i=0;i<count_;++i)if(devices_[i].type==type && devices_[i].id==id)return &devices_[i];return nullptr;}
    static void set(ErsDevice &d,uint8_t c,uint16_t v){
        if(d.type==3 && c==15 && (v==1 || v==2)){d.gpsMode=v;d.known|=1;}
        if(d.type==8 && c==10 && v>=1 && v<=100){d.motorPoles=v;d.known|=2;}
        if(d.type==4 && c==17 && v){d.capacityMah=v;d.known|=4;}
        if((d.type==2 || d.type==8) && c==14 && v<=127){d.telemetryId=v;d.known|=8;}
    }
    ErsDevice devices_[MaxDevices]={};uint8_t count_=0;
    ErsRequest pending_;bool busy_=false,queued_=false;uint32_t started_=0;
    const char *status_="Discover sensors to begin";
};
