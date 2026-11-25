#pragma once
#include <vector>
#include <cstring>
#include <cstdint>
inline std::vector<uint8_t> savedRecord;
inline bool failStore=false;
inline uint32_t savedTicket=0,savedLock=0;
class Preferences {public:
 bool begin(const char*n,bool ro){(void)ro;assert(!strcmp(n,"r1esc-start"));return true;}
 size_t putBytes(const char*k,const void*p,size_t n){assert(!strcmp(k,"handoff"));if(failStore)return 0;const auto*b=static_cast<const uint8_t*>(p);savedRecord.assign(b,b+n);return n;}
 size_t getBytesLength(const char*k){assert(!strcmp(k,"handoff"));return savedRecord.size();}
 size_t getBytes(const char*k,void*p,size_t n){assert(!strcmp(k,"handoff"));if(n<savedRecord.size())return 0;memcpy(p,savedRecord.data(),savedRecord.size());return savedRecord.size();}
 uint32_t getUInt(const char*k,uint32_t fallback=0){(void)fallback;assert(!strcmp(k,"request")||!strcmp(k,"locked"));return !strcmp(k,"request")?savedTicket:savedLock;}
 size_t putUInt(const char*k,uint32_t v){if(failStore)return 0;assert(!strcmp(k,"request")||!strcmp(k,"locked"));(!strcmp(k,"request")?savedTicket:savedLock)=v;return 4;}
 bool remove(const char*k){if(!strcmp(k,"handoff"))savedRecord.clear();else if(!strcmp(k,"request"))savedTicket=0;else if(!strcmp(k,"locked"))savedLock=0;else assert(false);return true;}
 void end(){}
};
