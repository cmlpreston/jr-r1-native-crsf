#include "crsf_telemetry.h"
#include "crsf_channel_output.h"
#include <cassert>
#include <cstdio>
#include <vector>
static uint32_t testNow = 1234;
uint32_t millis() { return testNow; }
static void feed(CrsfTelemetryParser &p,uint8_t type,std::vector<uint8_t> payload,bool corrupt=false) {
    std::vector<uint8_t> b={0xc8,uint8_t(payload.size()+2),type};
    b.insert(b.end(),payload.begin(),payload.end());
    b.push_back(crc8DvbS2(b.data()+2,b.size()-2)^uint8_t(corrupt));
    for(auto value:b)p.feed(value);
}
int main() {
    CrsfTelemetryParser parser;
    feed(parser,0x2b,{0xea,0xee,1,0,0,9,'P',0},true);
    assert(parser.state().crcErrors==1 && parser.state().txParameterSequence==0);
    feed(parser,0x2b,{0xea,0xec,1,0,0,9,'P',0}); // receiver not TX
    feed(parser,0x29,{0xec,0xee,'T',0}); // wrong destination
    assert(parser.state().txParameterSequence==0);
    std::vector<uint8_t> maximum(60,0x42); maximum[0]=0xea;maximum[1]=0xee;
    feed(parser,0x2b,maximum);
    assert(parser.state().txParameterSequence==1 && parser.state().txParameterLength==60);
    assert(parser.state().txParameterPayload[59]==0x42);
    feed(parser,0x14,{31,32,99,5,0,1,2,0,0,0});
    assert(parser.state().linkValid && parser.state().uplinkLinkQuality==99);
    assert(parser.state().txParameterSequence==1); // telemetry doesn't erase response
    feed(parser,0x29,{0xea,0xee,'T',0});
    assert(parser.state().txParameterType==0x29 && parser.state().txParameterSequence==2);
    // Recorded wire shape: signed big-endian coordinates, deci-km/h speed,
    // centidegree heading, altitude offset and satellite count.
    std::vector<uint8_t> gps;
    auto be32 = [&](int32_t v) { for(int shift=24;shift>=0;shift-=8) gps.push_back(uint32_t(v)>>shift); };
    be32(-378134000); be32(1449631000);
    gps.insert(gps.end(),{0x01,0xc2,0x23,0x28,0x04,0x10,12}); // 45.0 km/h, 90 deg, 40 m
    feed(parser,0x02,gps);
    assert(parser.state().latitudeE7==-378134000 && parser.state().longitudeE7==1449631000);
    assert(parser.state().groundSpeedDeciKmh==450 && parser.state().headingCentiDegrees==9000);
    assert(parser.state().altitudeMetres==40 && parser.state().satellites==12);
    testNow+=1;
    auto malformed=gps; malformed.resize(14); feed(parser,0x02,malformed);
    assert(parser.state().gpsUpdatedMs==1234);
    malformed=gps;malformed[0]=0x7f;feed(parser,0x02,malformed);
    assert(parser.state().gpsUpdatedMs==1234); // out-of-range coordinates ignored
    feed(parser,0x0c,{0,0,0x75,0x30}); // motor 0: 30000 RPM
    assert(parser.state().rpm==30000 && parser.state().rpmValid);
    feed(parser,0x0c,{1,0,0,100}); // unrelated motor does not overwrite source 0
    assert(parser.state().rpm==30000);
    feed(parser,0x0c,{0,0xff,0xff,0x9c});
    assert(parser.state().rpm==-100); // signed 24-bit reverse rotation
    feed(parser,0x0c,{0,0,1}); assert(parser.state().rpm==-100);
    feed(parser,0x0d,{0,0xff,0xce});
    assert(parser.state().temperatureDeciC==-50 && parser.state().temperatureValid);
    feed(parser,0x0d,{0xee,0x01,0x90}); // TX temperature is not vehicle temperature
    assert(parser.state().temperatureDeciC==-50);
    feed(parser,0x0d,{0,0}); assert(parser.state().temperatureDeciC==-50);
    testNow=5000;parser.updateFreshness();
    assert(!parser.state().gpsValid && !parser.state().rpmValid && parser.state().temperatureValid);
    testNow=12000;parser.updateFreshness(); assert(!parser.state().temperatureValid);
    testNow=UINT32_MAX-100;feed(parser,0x02,gps);
    testNow=200;parser.updateFreshness();assert(parser.state().gpsValid);
    testNow=4000;parser.updateFreshness();assert(!parser.state().gpsValid);
    puts("CRSF telemetry transport, GPS, RPM, temperature and freshness tests passed");
}
