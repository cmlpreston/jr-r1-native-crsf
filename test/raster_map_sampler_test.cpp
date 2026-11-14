#include "raster_map_sampler.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
uint32_t randomState=0x71946b2du;
uint32_t randomValue(){randomState=randomState*1664525u+1013904223u;return randomState;}
int randomBetween(int low,int high){return int(int64_t(low)+int64_t(randomValue())% (int64_t(high)-low+1));}
int originalX(const GpsMap::RasterSource &s,const GpsMap::RasterView &v,int x,int origin){
    const double metresLon=GpsMap::MetresPerE7*GpsMap::longitudeScale(v.latitude);
    const double lon=v.longitude+(origin+x-(origin+v.width/2.0))/v.pixelsPerMetre/metresLon;
    const double value=std::floor((lon-s.west)/(double(s.east)-s.west)*s.width);
    return value>=0&&value<s.width?int(value):-1;
}
int originalY(const GpsMap::RasterSource &s,const GpsMap::RasterView &v,int y,int origin){
    const double lat=v.latitude-(origin+y-(origin+v.height/2.0))/v.pixelsPerMetre/GpsMap::MetresPerE7;
    const double value=std::floor((s.north-lat)/(double(s.north)-s.south)*s.height);
    return value>=0&&value<s.height?int(value):-1;
}
void equivalence(){
    GpsMap::RasterMapSampler sampler;
    for(unsigned trial=0;trial<2500;++trial){
        const int32_t lat=randomBetween(-840000000,840000000);
        const int32_t lon=randomBetween(-1700000000,1700000000);
        const int latSpan=randomBetween(2,2500000),lonSpan=randomBetween(2,2500000);
        GpsMap::RasterSource s(lat-latSpan/2,lat+(latSpan+1)/2,lon-lonSpan/2,lon+(lonSpan+1)/2,
            randomBetween(1,512),randomBetween(1,512));
        const double scales[]={0.001,0.03,0.125,0.5,1,3,8,64,200};
        const int width=trial%2?320:480,height=trial%2?480:320;
        GpsMap::RasterView v(lat+randomBetween(-latSpan,latSpan),lon+randomBetween(-lonSpan,lonSpan),
            scales[trial%9],width,height);
        assert(sampler.prepare(s,v,trial));
        const auto builds=sampler.builds();
        std::vector<uint16_t> pixels(unsigned(s.width)*s.height);
        for(unsigned n=0;n<pixels.size();++n)pixels[n]=uint16_t((n*31u)^(n>>4)^0x71abu);
        uint16_t row[480];const uint16_t background=0x33abu;
        const int ox=randomBetween(-200,200),oy=randomBetween(-200,200);
        for(int x=0;x<width;++x)assert(sampler.sourceX(x)==originalX(s,v,x,ox));
        // Random subrectangles, repeated/reversed strip order and off-map rows.
        for(unsigned strip=0;strip<8;++strip){
            const int y=randomBetween(-100,height+100),x=randomBetween(0,width-1),n=randomBetween(1,width-x);
            assert(sampler.prepare(s,v,trial)&&sampler.builds()==builds);
            assert(sampler.sourceY(y)==originalY(s,v,y,oy));
            assert(sampler.sampleRow(y,x,n,pixels.data(),row,background));
            const int sy=originalY(s,v,y,oy);
            for(int i=0;i<n;++i){const int sx=originalX(s,v,x+i,ox);
                const uint16_t expected=sx<0||sy<0?background:pixels[unsigned(sy)*s.width+unsigned(sx)];
                assert(row[i]==expected);}
        }
        // Replacing pixels at identical geometry cannot return a cached image.
        std::fill(pixels.begin(),pixels.end(),0xc71e);
        assert(sampler.sampleRow(height/2,0,width,pixels.data(),row,background));
        const int sy=originalY(s,v,height/2,oy);
        for(int x=0;x<width;++x)assert(row[x]==(sy<0||sampler.sourceX(x)<0?background:0xc71e));
    }
    GpsMap::RasterSource s(-1000,1000,-1000,1000,512,512);
    GpsMap::RasterView v(0,0,1,480,320);
    assert(sampler.prepare(s,v,1));uint32_t builds=sampler.builds();
    assert(sampler.prepare(s,v,2)&&sampler.builds()==++builds); // replacement revision
    ++v.latitude;assert(sampler.prepare(s,v,2)&&sampler.builds()==++builds);
    ++v.longitude;assert(sampler.prepare(s,v,2)&&sampler.builds()==++builds);
    v.pixelsPerMetre=2;assert(sampler.prepare(s,v,2)&&sampler.builds()==++builds);
    v.width=320;v.height=480;assert(sampler.prepare(s,v,2)&&sampler.builds()==++builds);
    --s.west;assert(sampler.prepare(s,v,2)&&sampler.builds()==++builds);
    --s.height;assert(sampler.prepare(s,v,2)&&sampler.builds()==++builds);
    uint16_t row[480]={};
    assert(!sampler.sampleRow(0,319,2,nullptr,row,0));
    v.pixelsPerMetre=0;assert(!sampler.prepare(s,v,2));
    assert(!sampler.sampleRow(0,0,1,nullptr,row,0));
    v.pixelsPerMetre=std::numeric_limits<double>::quiet_NaN();assert(!sampler.prepare(s,v,2));
    v.pixelsPerMetre=1;v.width=481;assert(!sampler.prepare(s,v,2));
}
void trailKeys(){
    GpsMap::TrailProjectionCache cache;
    assert(!cache.unchanged(1,48,-378361917,1450852289,0.5,1,480,320));
    cache.remember(1,48,-378361917,1450852289,0.5,1,480,320);
    assert(cache.unchanged(1,48,-378361917,1450852289,0.5,1,480,320));
    assert(!cache.unchanged(2,48,-378361917,1450852289,0.5,1,480,320)); // full history rollover
    assert(!cache.unchanged(1,47,-378361917,1450852289,0.5,1,480,320));
    assert(!cache.unchanged(1,48,-378361916,1450852289,0.5,1,480,320));
    assert(!cache.unchanged(1,48,-378361917,1450852288,0.5,1,480,320));
    assert(!cache.unchanged(1,48,-378361917,1450852289,1,1,480,320));
    assert(!cache.unchanged(1,48,-378361917,1450852289,0.5,2,480,320));
    assert(!cache.unchanged(1,48,-378361917,1450852289,0.5,1,320,480));
}
void widgetCoverage(){
    GpsMap::RasterMapSampler sampler;
    for(unsigned trial=0;trial<1200;++trial){
        const int32_t lat=randomBetween(-840000000,840000000);
        const int32_t lon=randomBetween(-1700000000,1700000000);
        const int latSpan=randomBetween(3,2500000),lonSpan=randomBetween(3,2500000);
        const GpsMap::RasterSource source(lat-latSpan/2,lat+(latSpan+1)/2,
            lon-lonSpan/2,lon+(lonSpan+1)/2,randomBetween(1,512),randomBetween(1,512));
        const int widths[]={112,232,464,304,320,480},heights[]={88,96,188,276,480,320};
        const double scales[]={0.001,0.15,0.5,1,2,8};
        GpsMap::RasterView requested(source.south+randomBetween(0,latSpan),source.west+randomBetween(0,lonSpan),
            scales[trial%6],widths[trial%6],heights[trial%6]);
        if(trial%4==0){requested.latitude=source.south;requested.longitude=source.west;}
        if(trial%4==1){requested.latitude=source.north;requested.longitude=source.east;}
        const auto fitted=GpsMap::coverRasterViewport(source,requested);
        assert(fitted.pixelsPerMetre>=requested.pixelsPerMetre);
        assert(sampler.prepare(source,fitted,trial));
        for(unsigned x=0;x<fitted.width;++x)assert(sampler.sourceX(x)>=0);
        for(unsigned y=0;y<fitted.height;++y)assert(sampler.sourceY(y)>=0);
        // The minimum cover zoom is stable across the saved area and unchanged
        // fits are idempotent, avoiding a redraw for every moving GPS packet.
        auto opposite=requested;opposite.latitude=source.north;opposite.longitude=source.west;
        const auto other=GpsMap::coverRasterViewport(source,opposite);
        assert(other.pixelsPerMetre==fitted.pixelsPerMetre);
        const auto again=GpsMap::coverRasterViewport(source,fitted);
        assert(again.latitude==fitted.latitude&&again.longitude==fitted.longitude&&
               again.pixelsPerMetre==fitted.pixelsPerMetre);
        // An out-of-coverage fix must not be moved onto saved geography.
        auto outside=requested;outside.latitude=source.north+1;
        const auto unchanged=GpsMap::coverRasterViewport(source,outside);
        assert(unchanged.latitude==outside.latitude&&unchanged.longitude==outside.longitude&&
               unchanged.pixelsPerMetre==outside.pixelsPerMetre);
    }
}
// Optional host CPU benchmark of the actual sampler, separate from tests and
// never presented as an ESP32/SPI timing measurement. Render checksum prevents
// dead-code elimination; both paths must produce the identical full image.
void benchmark(){
    const GpsMap::RasterSource s(GpsMap::OriginLat-6000,GpsMap::OriginLat+6000,
        GpsMap::OriginLon-8000,GpsMap::OriginLon+8000,512,512);
    const GpsMap::RasterView v(GpsMap::OriginLat,GpsMap::OriginLon,0.5,480,320);
    std::vector<uint16_t> pixels(unsigned(s.width)*s.height),oldFrame(unsigned(v.width)*v.height),newFrame(oldFrame.size());
    for(unsigned i=0;i<pixels.size();++i)pixels[i]=uint16_t(i^(i>>4));
    GpsMap::RasterMapSampler sampler;
    constexpr unsigned rounds=100;volatile uint64_t oldChecksum=0,newChecksum=0;
    const auto oldStart=std::chrono::steady_clock::now();
    for(unsigned round=0;round<rounds;++round){
        const double metresLon=GpsMap::MetresPerE7*GpsMap::longitudeScale(v.latitude);
        for(int y=0;y<v.height;++y){
            const double lat=v.latitude-(y-v.height/2.0)/v.pixelsPerMetre/GpsMap::MetresPerE7;
            const int sy=int(std::floor((s.north-lat)/(double(s.north)-s.south)*s.height));
            for(int x=0;x<v.width;++x){
                const double lon=v.longitude+(x-v.width/2.0)/v.pixelsPerMetre/metresLon;
                const int sx=int(std::floor((lon-s.west)/(double(s.east)-s.west)*s.width));
                oldFrame[unsigned(y)*v.width+x]=sx>=0&&sx<s.width&&sy>=0&&sy<s.height?pixels[unsigned(sy)*s.width+sx]:0x33ab;
            }
        }
        oldChecksum+=oldFrame[round*719%oldFrame.size()];
    }
    const auto oldEnd=std::chrono::steady_clock::now();
    for(unsigned round=0;round<rounds;++round){
        // Normal LVGL strip rendering re-enters this cache for every16 rows.
        for(int first=0;first<v.height;first+=16){
            assert(sampler.prepare(s,v,1));
            for(int y=first;y<std::min(first+16,int(v.height));++y)
                assert(sampler.sampleRow(y,0,v.width,pixels.data(),newFrame.data()+unsigned(y)*v.width,0x33ab));
        }
        newChecksum+=newFrame[round*719%newFrame.size()];
    }
    const auto newEnd=std::chrono::steady_clock::now();
    assert(oldFrame==newFrame&&oldChecksum==newChecksum&&sampler.builds()==1);
    const auto oldUs=std::chrono::duration_cast<std::chrono::microseconds>(oldEnd-oldStart).count();
    const auto newUs=std::chrono::duration_cast<std::chrono::microseconds>(newEnd-oldEnd).count();
    printf("Map sampling host CPU: %u x 480x320 old=%lldus cached=%lldus checksum=%llu cache_builds=%lu; excludes LVGL/SPI\n",
        rounds,static_cast<long long>(oldUs),static_cast<long long>(newUs),
        static_cast<unsigned long long>(newChecksum),static_cast<unsigned long>(sampler.builds()));
}
}
int main(){
    equivalence();trailKeys();widgetCoverage();
    if(std::getenv("JR_R1_MAP_BENCHMARK"))benchmark();
    puts("Raster maps: original-formula pixel equivalence, clipping/pan/zoom, cache/trail keys and edge-to-edge widget imagery PASS");
}
