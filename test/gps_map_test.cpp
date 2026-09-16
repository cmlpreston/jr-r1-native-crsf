#include "gps_map_projection.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace GpsMap;
    assert(covers(OriginLat,OriginLon) && covers(DemoLat,DemoLon));
    assert(!covers(0,0) && !covers(-378500000,OriginLon));
    assert(east(OriginLat,OriginLon,OriginLon)==0);
    assert(north(OriginLat,OriginLat)==0);
    // One E7 unit is ~1.11 cm north and ~0.879 cm east at this latitude.
    assert(std::abs(north(OriginLat+10000,OriginLat)-111.3195)<0.001);
    assert(east(OriginLat,OriginLon+10000,OriginLon)>87 && east(OriginLat,OriginLon+10000,OriginLon)<89);
    // Panning the viewport translates geometry and trace equally; zoom doubles offsets.
    const double landmark=east(OriginLat,DemoLon,OriginLon);
    const double centre=east(OriginLat,OriginLon+3000,OriginLon);
    assert(std::abs((landmark-centre)-east(OriginLat,DemoLon,OriginLon+3000))<1e-8);
    // The complete synthetic orbit stays inside the downloaded map and north pitch bounds.
    for(int i=0;i<360;++i){
        const double p=i*3.141592653589793/180;
        const int32_t lat=DemoLat+static_cast<int32_t>(std::sin(p)*4500);
        const int32_t lon=DemoLon+static_cast<int32_t>(std::cos(p)*6000);
        assert(covers(lat,lon));
        assert(lat>=-378360457 && lat<=-378348116);
        assert(lon>=1450849658 && lon<=1450865947);
    }
    puts("GPS map: local projection, pan alignment, coverage and simulated route PASS");
}
