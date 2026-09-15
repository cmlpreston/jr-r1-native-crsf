#pragma once
#include "crsf_management.h"
#include "crsf_telemetry.h"
#include <vector>
#include <cstring>

// Representative fake module for offline UI use; never talks to hardware.
inline void emulateTxConfig(const TxParameterRequest &r, CrsfTelemetryState &telemetry)
{
    struct Field { uint8_t parent,type,value; const char *name,*options; };
    static Field fields[] = {
        {0,9,1,"Packet Rate","50Hz;150Hz;250Hz;500Hz"},
        {0,9,1,"Telem Ratio","Off;Std;1:2;1:4;1:8;1:16;1:32;1:64"},
        {0,9,0,"Model Match","Off;On"},
        {0,11,0,"TX Power",""},
        {4,9,2,"Max Power","10;25;100;250;500;1000"},
        {4,9,1,"Dynamic","Off;Dyn;AUX9;AUX10;AUX11;AUX12"},
        {0,11,0,"WiFi Connectivity",""},
        {7,13,0,"Enable WiFi",""}
    };
    std::vector<uint8_t> payload={0xea,0xee};
    auto append=[&](const char *s) { payload.insert(payload.end(),s,s+strlen(s)+1); };
    if (r.type==0x28) {
        telemetry.txParameterType=0x29;append("Ranger Nano (SIM)");
        payload.insert(payload.end(),{'E','L','R','S',0,0,0,0,0,0,0,1,8,0});
    } else {
        if (!r.id || r.id>8) return;
        auto &f=fields[r.id-1];
        if (r.type==0x2d) {
            if (f.type==9) f.value=r.value;
            else if (f.type==13) {
                if (r.value==1) f.value=3;
                else if (r.value==4 || r.value==5) f.value=0;
            }
        }
        telemetry.txParameterType=0x2b;
        payload.insert(payload.end(),{r.id,0,f.parent,f.type});append(f.name);
        if (f.type==9) {
            append(f.options);uint8_t max=0;
            for(const char *s=f.options;*s;++s) if(*s==';') ++max;
            payload.insert(payload.end(),{f.value,0,max,0});append("");
        } else if (f.type==13) {
            payload.insert(payload.end(),{f.value,20});append(f.value==3 ? "Enter WiFi mode?" : "");
        }
        // Parameter payloads use at most 56 bytes per chunk after routing/id.
        const size_t dataSize=payload.size()-4, offset=size_t(r.type==0x2c ? r.value:0)*56;
        if(offset>=dataSize) return;
        const size_t length=dataSize-offset>56 ? 56:dataSize-offset;
        std::vector<uint8_t> chunk={0xea,0xee,r.id,uint8_t((dataSize-offset-length+55)/56)};
        chunk.insert(chunk.end(),payload.begin()+4+offset,payload.begin()+4+offset+length);
        payload=chunk;
    }
    telemetry.txParameterLength=payload.size();
    std::memcpy(telemetry.txParameterPayload,payload.data(),payload.size());
    ++telemetry.txParameterSequence;
}
