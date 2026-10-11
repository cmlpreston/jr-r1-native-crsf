#pragma once
#include <stdint.h>

constexpr uint8_t kModelImageCount = 9;
constexpr uint8_t kModelImageNone = 8;
constexpr uint32_t kModelOriginalColour = 0x1000000;
inline bool validModelColour(uint32_t value) { return value <= kModelOriginalColour; }
// Hand-defined body silhouettes in 160 x 108 artwork coordinates. Combine
// these spatial masks with chroma so coloured wheel rims and wings stay intact.
inline bool modelPaintRegion(uint8_t image, unsigned x, unsigned y, unsigned width, unsigned height)
{
    struct Point { int x,y; };
    static constexpr Point masks[8][12]={
        {{34,44},{51,31},{74,29},{100,40},{116,58},{129,71},{124,77},{101,70},{82,61},{63,73},{37,55},{34,44}},
        {{31,55},{43,44},{59,32},{95,21},{115,30},{121,40},{99,47},{85,57},{74,70},{47,68},{36,61},{31,55}},
        {{38,52},{43,38},{64,27},{98,26},{117,34},{120,44},{111,53},{98,49},{81,59},{66,61},{47,62},{38,52}},
        {{9,48},{14,36},{43,31},{56,18},{110,17},{121,31},{149,29},{149,48},{131,46},{108,72},{89,46},{14,56}},
        {{24,23},{45,20},{53,10},{79,11},{106,28},{125,31},{134,45},{96,46},{84,38},{48,43},{32,37},{24,23}},
        {{27,43},{37,34},{53,31},{63,16},{115,15},{135,31},{137,49},{119,50},{100,68},{85,54},{54,51},{27,43}},
        {{17,58},{28,46},{48,39},{72,28},{109,27},{136,36},{148,45},{125,53},{101,71},{87,57},{61,69},{22,66}},
        {{44,74},{62,54},{79,41},{91,30},{94,21},{103,24},{109,37},{133,44},{132,55},{111,51},{83,56},{54,80}}
    };
    if(image>=8||!width||!height)return false;
    const int px=x*160/width,py=y*108/height;
    bool inside=false;
    for(unsigned i=0,j=11;i<12;j=i++){
        const auto a=masks[image][i],b=masks[image][j];
        if((a.y>py)!=(b.y>py) && px<(b.x-a.x)*float(py-a.y)/(b.y-a.y)+a.x)inside=!inside;
    }
    return inside;
}
// A chroma mask leaves neutral rubber, glass and white highlights unchanged.
// It intentionally recolours all chromatic paint/livery, not individual panels.
inline void recolourModelPixel(uint8_t &r, uint8_t &g, uint8_t &b, uint32_t colour)
{
    if (colour == kModelOriginalColour) return;
    const uint8_t lo = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const uint8_t hi = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const unsigned chroma = hi - lo;
    if (chroma < 20) return;
    r = lo + chroma * ((colour >> 16) & 255) / 255;
    g = lo + chroma * ((colour >> 8) & 255) / 255;
    b = lo + chroma * (colour & 255) / 255;
}

// Paint layers: 0 protected, 1 primary hue, 2 accent/white livery, 3 dark base.
// Spatial masks are authored for these exact assets, including painted wings.
inline uint8_t modelPaintRole(uint8_t image,unsigned x,unsigned y,unsigned w,unsigned h,uint8_t r,uint8_t g,uint8_t b) {
    if(image>=8||!w||!h)return 0;
    const unsigned px=x*160/w,py=y*108/h;
    static const uint8_t wings[8][4]={{9,8,73,29},{93,8,147,30},{87,8,145,25},{0,0,0,0},{0,0,0,0},{0,0,0,0},{95,13,151,29},{108,18,148,39}};
    const auto &wing=wings[image];
    bool painted=modelPaintRegion(image,x,y,w,h)||(px>=wing[0]&&px<wing[2]&&py>=wing[1]&&py<wing[3]);
    if(image==7&&py>=70&&py<=88&&px>=8&&px<=78)painted=true; // formula front wing
    if(!painted)return 0;
    // Protect cockpit glazing; polygons constrain the rest to body panels.
    static const uint8_t glass[8][4]={{57,34,87,49},{71,27,95,41},{63,30,93,44},{51,23,103,37},{51,16,77,28},{63,21,108,38},{60,35,106,50},{85,35,102,45}};
    const auto &window=glass[image];
    if(px>=window[0]&&px<window[2]&&py>=window[1]&&py<window[3])return 0;
    const unsigned hi=r>g?(r>b?r:b):(g>b?g:b),lo=r<g?(r<b?r:b):(g<b?g:b);
    if(hi-lo>=24) {
        // Blue/cyan is primary on the off-road/touring assets; red on formula.
        const bool primary=image==7?r>g*1.2&&r>b*1.2:b>r*1.1;
        return primary?1:2;
    }
    return hi>145?2:3;
}
inline void recolourModelMaterial(uint8_t &r,uint8_t &g,uint8_t &b,uint32_t colour,uint8_t role) {
    if(!role||colour==kModelOriginalColour)return;
    const unsigned hi=r>g?(r>b?r:b):(g>b?g:b),lo=r<g?(r<b?r:b):(g<b?g:b);
    if(hi-lo>=20){recolourModelPixel(r,g,b,colour);return;}
    // Neutral livery needs a luminance-based tint. Lift dark painted base enough
    // to make a chosen light colour visible while preserving its shading.
    const unsigned light=role==3?(hi*2>255?255:hi*2):hi;
    r=light*((colour>>16)&255)/255;g=light*((colour>>8)&255)/255;b=light*(colour&255)/255;
}
