#include "run_record_store.h"
#include <cstdio>
#include <cstring>
#include <new>
#if defined(JR_R1_SIMULATOR)
#include <cstdlib>
#include <filesystem>
#include <fstream>
#ifdef __EMSCRIPTEN__
#include <Preferences.h>
#endif
#else
#include <LittleFS.h>
#endif

namespace {
constexpr uint32_t Magic=0x31524c52,Version=1;
struct Header { uint32_t magic,version,bytes,checksum; };
uint32_t checksum(const void *data,size_t bytes){
    uint32_t crc=0xffffffffu;const auto *p=static_cast<const uint8_t *>(data);
    for(size_t i=0;i<bytes;++i){crc^=p[i];for(unsigned b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
    return ~crc;
}
bool valid(const Header &h,size_t bytes){return h.magic==Magic&&h.version==Version&&h.bytes==bytes;}
#if defined(JR_R1_SIMULATOR) && !defined(__EMSCRIPTEN__)
std::filesystem::path path(uint8_t model){
    const char *root=std::getenv("JR_R1_SIM_STATE");
#ifdef JR_R1_SIM_STATE_DIR
    if(!root)root=JR_R1_SIM_STATE_DIR;
#else
    if(!root)root="state";
#endif
    return std::filesystem::path(root)/"run-records"/("model-"+std::to_string(model)+".dat");
}
#elif !defined(__EMSCRIPTEN__)
void path(uint8_t model,char (&out)[32]){std::snprintf(out,sizeof(out),"/run-model-%u.dat",unsigned(model));}
#endif
}
namespace RunRecordStore {
bool read(uint8_t model,void *data,size_t bytes){
    if(model>=10||!data||!bytes)return false;
    Header h{};
#ifdef __EMSCRIPTEN__
    Preferences p;char key[16];std::snprintf(key,sizeof(key),"model%u",unsigned(model));
    if(!p.begin("r1-runlogs",true))return false;
    bool ok=p.getBytesLength(key)==bytes+sizeof(h);
    // Browser demo has no handset filesystem; use its localStorage adapter.
    auto *buffer=ok?new(std::nothrow) uint8_t[bytes+sizeof(h)]:nullptr;
    if(buffer){ok=p.getBytes(key,buffer,bytes+sizeof(h))==bytes+sizeof(h);if(ok){std::memcpy(&h,buffer,sizeof(h));std::memcpy(data,buffer+sizeof(h),bytes);}delete[] buffer;}else ok=false;
    p.end();if(!ok)return false;
#elif defined(JR_R1_SIMULATOR)
    std::ifstream file(path(model),std::ios::binary);
    if(!file.read(reinterpret_cast<char *>(&h),sizeof(h))||!valid(h,bytes))return false;
    if(!file.read(static_cast<char *>(data),bytes)||file.peek()!=std::char_traits<char>::eof())return false;
#else
    if(!LittleFS.begin(false))return false;
    char name[32];path(model,name);
    auto file=LittleFS.open(name,"r");
    if(!file||file.size()!=sizeof(h)+bytes||file.read(reinterpret_cast<uint8_t *>(&h),sizeof(h))!=sizeof(h)||!valid(h,bytes))return false;
    if(file.read(static_cast<uint8_t *>(data),bytes)!=bytes)return false;
#endif
    return valid(h,bytes)&&checksum(data,bytes)==h.checksum;
}
bool write(uint8_t model,const void *data,size_t bytes){
    if(model>=10||(!data&&bytes)||bytes>128*1024)return false;
    const Header h{Magic,Version,uint32_t(bytes),checksum(data,bytes)};
#ifdef __EMSCRIPTEN__
    auto *buffer=new(std::nothrow) uint8_t[bytes+sizeof(h)];if(!buffer)return false;
    std::memcpy(buffer,&h,sizeof(h));if(bytes)std::memcpy(buffer+sizeof(h),data,bytes);
    Preferences p;char key[16];std::snprintf(key,sizeof(key),"model%u",unsigned(model));
    const bool ok=p.begin("r1-runlogs",false)&&p.putBytes(key,buffer,bytes+sizeof(h))==bytes+sizeof(h);
    p.end();delete[] buffer;return ok;
#elif defined(JR_R1_SIMULATOR)
    const auto name=path(model);const auto pending=name.string()+".pending";std::error_code error;
    std::filesystem::create_directories(name.parent_path(),error);if(error)return false;
    {std::ofstream file(pending,std::ios::binary|std::ios::trunc);
     file.write(reinterpret_cast<const char *>(&h),sizeof(h));if(bytes)file.write(static_cast<const char *>(data),bytes);
     file.flush();if(!file)return false;file.close();if(file.fail())return false;}
    std::filesystem::rename(pending,name,error);return !error;
#else
    if(!LittleFS.begin(false))return false;
    char name[32];path(model,name);
    char pending[40];std::snprintf(pending,sizeof(pending),"%s.pending",name);
    auto file=LittleFS.open(pending,"w");if(!file)return false;
    bool ok=file.write(reinterpret_cast<const uint8_t *>(&h),sizeof(h))==sizeof(h);
    if(ok&&bytes)ok=file.write(static_cast<const uint8_t *>(data),bytes)==bytes;
    file.flush();file.close();if(!ok){LittleFS.remove(pending);return false;}
    return LittleFS.rename(pending,name);
#endif
}
// An atomic empty record prevents a partially deleted checkpoint resurrecting.
bool erase(uint8_t model){return write(model,nullptr,0);}
}
