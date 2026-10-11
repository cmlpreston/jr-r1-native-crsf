#pragma once
#include "offline_map.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cstring>
namespace OfflineMap {
// Image file: ASCII R1IM, uint32 LE JSON length, bounded JSON metadata,
// followed by exactly width*height little-endian RGB565 pixels.
inline bool decodeImage(JsonVariantConst doc, Package &result) {
    if(doc["version"]!=2||doc["format"]!="rgb565le"||doc["projection"]!="equirectangular"||
       !doc["name"].is<const char*>()||!doc["attribution"].is<const char*>())return false;
    const char *name=doc["name"],*attr=doc["attribution"];
    if(!*name||strlen(name)>40||!*attr||strlen(attr)>80)return false;
    if(!doc["width"].is<unsigned>()||!doc["height"].is<unsigned>())return false;
    unsigned w=doc["width"],h=doc["height"];if(w<2||h<2||w>512||h>512)return false;
    auto b=doc["bounds"];for(const char *k:{"south","north","west","east"})if(!b[k].is<double>())return false;
    double south=b["south"],north=b["north"],west=b["west"],east=b["east"];
    if(!coordinateValid(south,west)||!coordinateValid(north,east)||north<=south||east<=west||north-south>.25||east-west>.25)return false;
    memset(static_cast<void*>(&result),0,sizeof(result));strcpy(result.name,name);strcpy(result.attribution,attr);result.width=w;result.height=h;
    result.south=lround(south*1e7);result.north=lround(north*1e7);result.west=lround(west*1e7);result.east=lround(east*1e7);
    result.valid=result.north>result.south&&result.east>result.west;return result.valid;
}
inline bool decode(JsonVariantConst doc, Package &result) {
    if(doc["version"]!=1 || !doc["name"].is<const char*>() || !doc["attribution"].is<const char*>())return false;
    const char *name=doc["name"], *attr=doc["attribution"];
    if(!*name||strlen(name)>40||!*attr||strlen(attr)>80)return false;
    memset(static_cast<void*>(&result),0,sizeof(result));strcpy(result.name,name);strcpy(result.attribution,attr);
    auto paths=doc["paths"].as<JsonArrayConst>();if(paths.isNull()||paths.size()>OfflineMap::MaxPaths)return false;
    for(JsonVariantConst line:paths) {
        auto points=line.as<JsonArrayConst>();if(points.isNull()||points.size()<2)return false;
        if(result.pointCount+points.size()>OfflineMap::MaxPoints)return false;
        auto &p=result.paths[result.pathCount++];p.offset=result.pointCount;p.count=points.size();
        for(JsonVariantConst pair:points) {
            if(!pair.is<JsonArrayConst>()||pair.size()!=2||!pair[0].is<double>()||!pair[1].is<double>())return false;
            const double lon=pair[0].as<double>(),lat=pair[1].as<double>();
            if(!OfflineMap::coordinateValid(lat,lon))return false;
            auto &point=result.points[result.pointCount++];point.lat=lround(lat*1e7);point.lon=lround(lon*1e7);
            if(result.pointCount==1){result.south=result.north=point.lat;result.west=result.east=point.lon;}
            result.south=std::min(result.south,point.lat);result.north=std::max(result.north,point.lat);
            result.west=std::min(result.west,point.lon);result.east=std::max(result.east,point.lon);
        }
    }
    result.valid=OfflineMap::boundsValid(result);return result.valid;
}
}
