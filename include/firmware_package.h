#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// An OTA package wraps an application image, never a merged flash image.
// MD5 detects transfer damage; authentication is supplied by the web session.
namespace FirmwarePackage {
constexpr size_t HeaderSize = 76;
constexpr char Target[] = "xiao-s3-waveshare-f-landscape";
struct Header { uint32_t size=0; char md5[33]={}; };
inline bool decode(const uint8_t *p,size_t length,size_t capacity,Header &out) {
    out=Header{};
    if(length!=HeaderSize||std::memcmp(p,"R1OTA001",8)!=0)return false;
    char target[32]={};std::memcpy(target,Target,sizeof(Target));
    if(std::memcmp(p+8,target,sizeof(target))!=0)return false;
    const uint32_t size=uint32_t(p[40])|(uint32_t(p[41])<<8)|
        (uint32_t(p[42])<<16)|(uint32_t(p[43])<<24);
    if(size<256||size>capacity)return false;
    for(size_t n=0;n<32;++n){const char c=char(p[44+n]);
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}
    out.size=size;std::memcpy(out.md5,p+44,32);return true;
}
}
