#include "firmware_package.h"
#include <cassert>
#include <cstdio>
int main(){
    uint8_t bytes[FirmwarePackage::HeaderSize]={};
    memcpy(bytes,"R1OTA001",8);memcpy(bytes+8,FirmwarePackage::Target,sizeof(FirmwarePackage::Target));
    bytes[41]=4;memset(bytes+44,'a',32);
    FirmwarePackage::Header out;
    assert(FirmwarePackage::decode(bytes,sizeof(bytes),4096,out)&&out.size==1024);
    assert(strlen(out.md5)==32);
    for(size_t n=0;n<sizeof(bytes);++n)assert(!FirmwarePackage::decode(bytes,n,4096,out));
    assert(!FirmwarePackage::decode(bytes,sizeof(bytes),1000,out));
    bytes[8]='z';assert(!FirmwarePackage::decode(bytes,sizeof(bytes),4096,out));bytes[8]='x';
    bytes[0]='z';assert(!FirmwarePackage::decode(bytes,sizeof(bytes),4096,out));bytes[0]='R';
    bytes[44]='Z';assert(!FirmwarePackage::decode(bytes,sizeof(bytes),4096,out));bytes[44]='a';
    bytes[41]=0;assert(!FirmwarePackage::decode(bytes,sizeof(bytes),4096,out));
    puts("firmware package tests passed");
}
