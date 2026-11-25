#pragma once
#include <cstdint>
#include <cstring>
#include <string>
constexpr int ESP_PARTITION_TYPE_APP=0;
struct esp_partition_t{int type;uint32_t address,size;};
inline esp_partition_t runningPart={0,0x10000,0x180000},peerPart={0,0x190000,0x180000};
inline std::string peerImage="\xBE\xEF\xCA\xFE" "UNIFIED_ESP32_2400_RX full-app 4.1.0-er6g-esc-mode-r11";
inline bool badHash=false,tooSmall=false,failSelect=false,differentHash=false,returnRunning=false;inline unsigned selects=0;
inline int esp_partition_read(const esp_partition_t*p,size_t offset,void*out,size_t n){assert(p==&peerPart);memset(out,0xff,n);if(offset<peerImage.size())memcpy(out,peerImage.data()+offset,std::min(n,peerImage.size()-offset));return 0;}
inline int esp_partition_get_sha256(const esp_partition_t*p,uint8_t*out){assert(p==&peerPart);memset(out,differentHash?1:0,32);return badHash?-1:0;}
