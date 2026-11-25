#pragma once
#include "gps_map_projection.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace GpsMap {
struct RasterSource {
    RasterSource(int32_t s=0,int32_t n=0,int32_t w=0,int32_t e=0,uint16_t columns=0,uint16_t rows=0)
        :south(s),north(n),west(w),east(e),width(columns),height(rows){}
    int32_t south=0,north=0,west=0,east=0;
    uint16_t width=0,height=0;
};
struct RasterView {
    RasterView(int32_t lat=0,int32_t lon=0,double scale=0,uint16_t columns=0,uint16_t rows=0)
        :latitude(lat),longitude(lon),pixelsPerMetre(scale),width(columns),height(rows){}
    int32_t latitude=0,longitude=0;
    double pixelsPerMetre=0;
    uint16_t width=0,height=0;
};

// Cover a widget with real saved imagery, keeping aspect ratio and geography.
// Keep the minimum zoom stable over the whole package; a latitude-dependent
// minimum would pan/redraw on every small GPS movement. One E7 unit of inset
// allows an integer-coordinate centre without sampling the exclusive far edge.
inline RasterView coverRasterViewport(const RasterSource &source,RasterView view) {
    const double latSpan=double(source.north)-source.south;
    const double lonSpan=double(source.east)-source.west;
    if(!source.width||!source.height||latSpan<=2||lonSpan<=2||!view.width||!view.height||
       !std::isfinite(view.pixelsPerMetre)||view.pixelsPerMetre<=0||
       view.latitude<source.south||view.latitude>source.north||
       view.longitude<source.west||view.longitude>source.east)return view;
    const double metresLon=MetresPerE7*std::min(longitudeScale(source.south),longitudeScale(source.north));
    if(!std::isfinite(metresLon)||metresLon<=0)return view;
    view.pixelsPerMetre=std::max(view.pixelsPerMetre,std::max(
        view.width/((lonSpan-2)*metresLon),view.height/((latSpan-2)*MetresPerE7)));
    const double halfLat=view.height/2.0/view.pixelsPerMetre/MetresPerE7;
    const double minLat=std::ceil(source.south+halfLat),maxLat=std::floor(source.north-halfLat);
    view.latitude=static_cast<int32_t>(std::max(minLat,std::min(maxLat,double(view.latitude))));
    const double halfLon=view.width/2.0/view.pixelsPerMetre/(MetresPerE7*longitudeScale(view.latitude));
    const double minLon=std::ceil(source.west+halfLon),maxLon=std::floor(source.east-halfLon);
    view.longitude=static_cast<int32_t>(std::max(minLon,std::min(maxLon,double(view.longitude))));
    return view;
}

// One bounded cache owned by the UI renderer, not a second image buffer. Source
// X depends only on viewport geometry, so all scanlines and LVGL dirty strips
// reuse these indices. Keep the original arithmetic/floor order to retain exact
// nearest-neighbour pixels at source-cell boundaries.
class RasterMapSampler {
public:
    static constexpr unsigned MaxWidth=480;
    bool prepare(const RasterSource &source,const RasterView &view,uint32_t revision) {
        if(ready_&&same(source,view,revision))return true;
        ready_=false;
        if(!source.width||source.width>512||!source.height||source.height>512||
           source.north<=source.south||source.east<=source.west||
           !view.width||view.width>MaxWidth||!view.height||view.height>480||
           !std::isfinite(view.pixelsPerMetre)||view.pixelsPerMetre<=0)return false;
        source_=source;view_=view;revision_=revision;
        const double metresLon=MetresPerE7*longitudeScale(view.latitude);
        if(!std::isfinite(metresLon)||metresLon<=0)return false;
        for(unsigned x=0;x<view.width;++x){
            const double lon=view.longitude+(double(x)-view.width/2.0)/view.pixelsPerMetre/metresLon;
            columns_[x]=index((lon-source.west)/(double(source.east)-source.west)*source.width,source.width);
        }
        ready_=true;++builds_;return true;
    }
    int sourceX(unsigned x)const{return ready_&&x<view_.width?columns_[x]:-1;}
    int sourceY(int y)const {
        if(!ready_)return -1;
        const double lat=view_.latitude-(y-view_.height/2.0)/view_.pixelsPerMetre/MetresPerE7;
        return index((source_.north-lat)/(double(source_.north)-source_.south)*source_.height,source_.height);
    }
    bool sampleRow(int y,unsigned x,unsigned count,const uint16_t *pixels,uint16_t *out,uint16_t background)const {
        if(!ready_||!out||x>view_.width||count>view_.width-x)return false;
        const int sy=sourceY(y);
        if(!pixels||sy<0){std::fill(out,out+count,background);return true;}
        const auto *row=pixels+unsigned(sy)*source_.width;
        for(unsigned n=0;n<count;++n){const int sx=columns_[x+n];out[n]=sx<0?background:row[sx];}
        return true;
    }
    uint32_t builds()const{return builds_;}
private:
    static int16_t index(double value,unsigned limit) {
        // Bounds before conversion also avoid undefined double-to-int overflow
        // for a geographically tiny package at an extreme pan/zoom setting.
        return std::isfinite(value)&&value>=0&&value<limit?int16_t(std::floor(value)):-1;
    }
    bool same(const RasterSource &s,const RasterView &v,uint32_t revision)const {
        return revision_==revision&&source_.south==s.south&&source_.north==s.north&&
            source_.west==s.west&&source_.east==s.east&&source_.width==s.width&&source_.height==s.height&&
            view_.latitude==v.latitude&&view_.longitude==v.longitude&&view_.pixelsPerMetre==v.pixelsPerMetre&&
            view_.width==v.width&&view_.height==v.height;
    }
    RasterSource source_;
    RasterView view_;
    uint32_t revision_=0,builds_=0;
    int16_t columns_[MaxWidth]={};
    bool ready_=false;
};
}
