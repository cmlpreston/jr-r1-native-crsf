#include "run_record_store.h"
#include <cerrno>
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
using RunRecordStore::ReadResult;

ReadResult checkHeader(const Header &header, size_t bytes)
{
    if (header.magic != Magic) return ReadResult::Corrupt;
    if (header.version != Version) return ReadResult::Incompatible;
    if (header.bytes == 0) {
        return header.checksum == 0 ? ReadResult::Missing : ReadResult::Corrupt;
    }
    return header.bytes == bytes ? ReadResult::Loaded : ReadResult::Incompatible;
}

#ifndef __EMSCRIPTEN__
ReadResult readFile(const char *path, void *data, size_t bytes)
{
    // VFS reports missing files and I/O errors separately through errno.
    auto *file = std::fopen(path, "rb");
    if (!file) return errno == ENOENT ? ReadResult::Missing : ReadResult::IoError;
    Header header{};
    auto result = std::fread(&header, 1, sizeof(header), file) == sizeof(header)
        ? checkHeader(header, bytes) : ReadResult::Corrupt;
    if (result == ReadResult::Loaded &&
        (std::fread(data, 1, bytes, file) != bytes || checksum(data, bytes) != header.checksum)) {
        result = ReadResult::Corrupt;
    }
    if ((result == ReadResult::Loaded || result == ReadResult::Missing) && std::fgetc(file) != EOF) {
        result = ReadResult::Corrupt;
    }
    const bool readError = std::ferror(file);
    const bool closeError = std::fclose(file) != 0;
    return readError || closeError ? ReadResult::IoError : result;
}
#endif
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
ReadResult read(uint8_t model, void *data, size_t bytes)
{
    if (model >= 10 || !data || !bytes) return ReadResult::IoError;
#ifdef __EMSCRIPTEN__
    Preferences preferences;
    char key[16];
    std::snprintf(key, sizeof(key), "model%u", unsigned(model));
    if (!preferences.begin("r1-runlogs", true)) return ReadResult::IoError;
    if (!preferences.isKey(key)) {
        preferences.end();
        return ReadResult::Missing;
    }
    const size_t size = preferences.getBytesLength(key);
    Header header{};
    const bool readHeader = preferences.getBytes(key, &header, sizeof(header)) == sizeof(header);
    auto result = readHeader ? checkHeader(header, bytes) : ReadResult::Corrupt;
    if ((result == ReadResult::Loaded || result == ReadResult::Missing) &&
        size != sizeof(header) + header.bytes) {
        result = ReadResult::Corrupt;
    }
    if (result == ReadResult::Loaded) {
        auto *buffer = new(std::nothrow) uint8_t[size];
        if (buffer && preferences.getBytes(key, buffer, size) == size) {
            std::memcpy(data, buffer + sizeof(header), bytes);
            if (checksum(data, bytes) != header.checksum) result = ReadResult::Corrupt;
        } else {
            result = ReadResult::IoError;
        }
        delete[] buffer;
    }
    preferences.end();
    return result;
#elif defined(JR_R1_SIMULATOR)
    return readFile(path(model).string().c_str(), data, bytes);
#else
    if (!LittleFS.begin(false)) return ReadResult::IoError;
    char name[32];
    path(model, name);
    char mountedPath[48];
    std::snprintf(mountedPath, sizeof(mountedPath), "/littlefs%s", name);
    return readFile(mountedPath, data, bytes);
#endif
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
