#include "offline_map_json.h"
#include <cassert>
#include <cstdio>
using namespace OfflineMap;
int main(){
    DynamicJsonDocument d(48000);Package p;
    assert(!deserializeJson(d,R"({"version":1,"name":"Track","attribution":"Survey","paths":[[[145.08,-37.83],[145.081,-37.831]]]})"));
    assert(decode(d.as<JsonVariantConst>(),p));assert(p.pointCount==2&&p.pathCount==1);
    const auto before=revision();
    install(p);assert(revision()==before+1);
    install(p);assert(revision()==before+2); // Same bounds still represent replacement content.
    assert(covers(-378300000,1450800000));assert(!covers(0,0));
    d["paths"][0][0][0]=181;assert(!decode(d.as<JsonVariantConst>(),p));
    d["paths"][0][0][0]=144;assert(!decode(d.as<JsonVariantConst>(),p));
    d["paths"][0][0][0]="145";assert(!decode(d.as<JsonVariantConst>(),p));
    d["paths"][0][0][0]=145.08;d["attribution"]="";assert(!decode(d.as<JsonVariantConst>(),p));
    d["attribution"]="Survey";d["version"]=2;assert(!decode(d.as<JsonVariantConst>(),p));
    d["version"]=1;auto paths=d["paths"].as<JsonArray>();paths.clear();
    for(unsigned i=0;i<33;++i){auto line=paths.createNestedArray();for(int j=0;j<2;++j){auto point=line.createNestedArray();point.add(145.08);point.add(-37.83);}}
    assert(!decode(d.as<JsonVariantConst>(),p));
    paths.clear();auto line=paths.createNestedArray();for(unsigned i=0;i<513;++i){auto point=line.createNestedArray();point.add(145.08);point.add(-37.83);}
    assert(!decode(d.as<JsonVariantConst>(),p));assert(!coordinateValid(NAN,0));assert(!coordinateValid(0,INFINITY));
    d.clear();
    assert(!deserializeJson(d,R"({"version":2,"format":"rgb565le","projection":"equirectangular","name":"Colour track","attribution":"OSM","width":512,"height":256,"bounds":{"south":-37.84,"north":-37.83,"west":145.08,"east":145.09}})"));
    assert(decodeImage(d.as<JsonVariantConst>(),p));
    auto *image=allocatePixels(p.width*p.height);assert(image);image[0]=0xf800;
    install(p,image);assert(pixels()[0]==0xf800&&current().width==512);
    assert(covers(-378350000,1450850000));
    d["width"]=513;assert(!decodeImage(d.as<JsonVariantConst>(),p));
    d["width"]=512;d["bounds"]["north"]=-37.0;assert(!decodeImage(d.as<JsonVariantConst>(),p));
    d["bounds"]["north"]=-37.84;assert(!decodeImage(d.as<JsonVariantConst>(),p));
    d["bounds"]["north"]=-37.83;d["format"]="rgb888";assert(!decodeImage(d.as<JsonVariantConst>(),p));
    assert(!allocatePixels(0)&&!allocatePixels(512*512+1));
    install(Package{});assert(!pixels()&&!current().valid);
    puts("Offline map vector/image validation PASS");
}
