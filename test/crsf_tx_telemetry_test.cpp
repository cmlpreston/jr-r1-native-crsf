#include "crsf_telemetry.h"
#include "crsf_channel_output.h"
#include "esc_programmer_wire.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
static uint32_t testNow = 1234;
uint32_t millis() { return testNow; }
static void feed(CrsfTelemetryParser &p,uint8_t type,std::vector<uint8_t> payload,bool corrupt=false) {
    std::vector<uint8_t> b={0xc8,uint8_t(payload.size()+2),type};
    b.insert(b.end(),payload.begin(),payload.end());
    b.push_back(crc8DvbS2(b.data()+2,b.size()-2)^uint8_t(corrupt));
    for(auto value:b)p.feed(value);
}
static void testGpsUtc()
{
    auto time=[](unsigned year,unsigned month,unsigned day,unsigned hour,unsigned minute,
                 unsigned second,unsigned millisecond){
        return std::vector<uint8_t>{uint8_t(year>>8),uint8_t(year),uint8_t(month),uint8_t(day),
            uint8_t(hour),uint8_t(minute),uint8_t(second),uint8_t(millisecond>>8),uint8_t(millisecond)};
    };
    CrsfTelemetryParser p;
    assert(!gpsTimeAvailable(p.state(),0));
    // Timestamp zero, midnight and zero milliseconds are real readings, with
    // no dependency on position, satellite count or the RF statistics frame.
    testNow=0;feed(p,0x03,time(2026,11,15,0,0,0,0));
    assert(gpsTimeAvailable(p.state(),0) && !p.state().gpsValid && !p.state().linkValid);
    assert(p.state().gpsUtcYear==2026 && p.state().gpsUtcMonth==11 && p.state().gpsUtcDay==15);
    assert(p.state().gpsUtcHour==0 && p.state().gpsUtcMinute==0 && p.state().gpsUtcSecond==0);
    assert(p.state().gpsUtcMillisecond==0 && p.state().gpsTimeUpdatedMs==0);
    // Distinct high/low bytes verify network byte order for both 16-bit fields.
    testNow=1000;const auto good=time(2024,2,29,23,58,57,789);feed(p,0x03,good);
    assert(p.state().gpsUtcYear==2024 && p.state().gpsUtcMonth==2 && p.state().gpsUtcDay==29);
    assert(p.state().gpsUtcHour==23 && p.state().gpsUtcMinute==58 && p.state().gpsUtcSecond==57);
    assert(p.state().gpsUtcMillisecond==789 && p.state().gpsTimeFrames==2 && !p.state().gpsTimeRejectedFrames);
    const auto lastGood=p.state();testNow=2000;
    // Truncated and invalid calendar/range frames are counted but cannot
    // replace or refresh the last good UTC value.
    for(size_t n=0;n<9;++n){auto shortFrame=good;shortFrame.resize(n);feed(p,0x03,shortFrame);}
    const std::vector<std::vector<uint8_t>> invalid={
        time(1999,12,31,23,59,59,999),time(10000,1,1,0,0,0,0),
        time(2026,0,1,0,0,0,0),time(2026,13,1,0,0,0,0),time(2026,1,0,0,0,0,0),
        time(2026,4,31,0,0,0,0),time(2026,2,29,0,0,0,0),time(2100,2,29,0,0,0,0),
        time(2026,1,1,24,0,0,0),time(2026,1,1,0,60,0,0),time(2026,1,1,0,0,60,0),
        time(2026,1,1,0,0,0,1000),std::vector<uint8_t>(9,0xff),std::vector<uint8_t>(9,0)
    };
    for(const auto &packet:invalid)feed(p,0x03,packet);
    assert(p.state().gpsTimeFrames==2+9+invalid.size());
    assert(p.state().gpsTimeRejectedFrames==9+invalid.size());
    assert(p.state().gpsTimeUpdatedMs==lastGood.gpsTimeUpdatedMs && gpsTimeAvailable(p.state(),testNow));
    assert(p.state().gpsUtcYear==lastGood.gpsUtcYear && p.state().gpsUtcDay==lastGood.gpsUtcDay);
    assert(p.state().gpsUtcSecond==lastGood.gpsUtcSecond && p.state().gpsUtcMillisecond==lastGood.gpsUtcMillisecond);
    const auto frames=p.state().gpsTimeFrames,rejects=p.state().gpsTimeRejectedFrames;
    feed(p,0x03,good,true);
    assert(p.state().crcErrors==1 && p.state().gpsTimeFrames==frames && p.state().gpsTimeRejectedFrames==rejects);
    auto oversized=good;oversized.resize(61,0xff);feed(p,0x03,oversized);
    assert(p.state().gpsTimeFrames==frames && p.state().gpsTimeRejectedFrames==rejects);
    // Future trailing fields, up to the supported frame size, are ignored.
    auto extended=time(2000,2,29,1,2,3,999);extended.resize(60,0xff);feed(p,0x03,extended);
    assert(p.state().gpsUtcYear==2000 && p.state().gpsUtcDay==29 && p.state().gpsUtcMillisecond==999);
    feed(p,0x03,time(2400,2,29,1,2,3,0));assert(p.state().gpsUtcYear==2400);
    feed(p,0x03,time(9999,12,31,23,59,59,999));assert(p.state().gpsUtcYear==9999);
    // Ordinary telemetry neither supplies nor refreshes UTC; repeated invalid
    // time packets also cannot postpone expiry.
    testNow=5000;feed(p,0x14,{31,32,99,5,0,1,2,0,0,0});
    feed(p,0x03,std::vector<uint8_t>(9,0));
    assert(gpsTimeAvailable(p.state(),testNow));p.updateFreshness();assert(p.state().gpsTimeValid);
    testNow=5001;assert(!gpsTimeAvailable(p.state(),testNow));p.updateFreshness();
    assert(!p.state().gpsTimeValid && p.state().gpsUtcYear==9999 && p.state().linkValid);
    // Recovery and 32-bit millis wrap retain the same three-second boundary.
    testNow=UINT32_MAX-100;feed(p,0x03,good);assert(p.state().gpsTimeValid);
    testNow=200;p.updateFreshness();assert(gpsTimeAvailable(p.state(),testNow));
    testNow=2899;assert(gpsTimeAvailable(p.state(),testNow));
    testNow=2900;assert(!gpsTimeAvailable(p.state(),testNow));p.updateFreshness();assert(!p.state().gpsTimeValid);
    assert(p.state().gpsTimeFrames>p.state().gpsTimeRejectedFrames);
    assert(!p.state().gpsWireFrames && !p.state().gpsDecodedFrames && !p.state().gpsSpeedFrames);
    testNow=1234;
}
int main() {
    testGpsUtc();
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
    assert(parser.state().gpsWireFrames==1 && parser.state().gpsDecodedFrames==1 && parser.state().gpsRejectedFrames==0);
    assert(parser.state().gpsWireUpdatedMs==testNow && parser.state().gpsLastPayloadLength==15);
    assert(parser.state().latitudeE7==-378134000 && parser.state().longitudeE7==1449631000);
    assert(parser.state().groundSpeedDeciKmh==450 && parser.state().headingCentiDegrees==9000);
    assert(parser.state().altitudeMetres==40 && parser.state().satellites==12);
    assert(gpsPositionAvailable(parser.state(),testNow) && gpsFixConfirmed(parser.state(),testNow));
    assert(parser.state().gpsCourseValid && gpsCourseAvailable(parser.state(),testNow));
    auto noCourse=gps;noCourse[10]=noCourse[11]=0;
    noCourse.insert(noCourse.end(),{'R','1',0});feed(parser,0x02,noCourse);
    assert(parser.state().gpsValid && !parser.state().gpsCourseValid);
    assert(gpsPositionAvailable(parser.state(),testNow) && !gpsCourseAvailable(parser.state(),testNow));
    auto north=gps;north[10]=north[11]=0;feed(parser,0x02,north);
    assert(parser.state().gpsCourseValid && gpsCourseAvailable(parser.state(),testNow));
    auto stopped=parser.state();stopped.groundSpeedDeciKmh=9;assert(!gpsCourseAvailable(stopped,testNow));
    assert(std::string(gpsCourseCardinal(0))=="N" && std::string(gpsCourseCardinal(35999))=="N");
    assert(std::string(gpsCourseCardinal(9000))=="E" && std::string(gpsCourseCardinal(18000))=="S");
    feed(parser,0x02,gps);
    auto unknown=parser.state();unknown.satellites=0;unknown.gpsSatellitesValid=false;
    assert(gpsPositionAvailable(unknown,testNow) && !gpsFixConfirmed(unknown,testNow));
    assert(!gpsPositionAvailable(unknown,testNow+3001));
    unknown.gpsSatellitesValid=true;
    assert(gpsPositionAvailable(unknown,testNow)&&!gpsFixConfirmed(unknown,testNow));
    // Legacy zero-as-unknown and a native zero with retained coordinates have
    // the same untagged wire representation: both remain unconfirmed.
    unknown.gpsSatellitesValid=false;
    unknown.latitudeE7=unknown.longitudeE7=0;assert(!gpsPositionAvailable(unknown,testNow));
    unknown=parser.state();unknown.satellites=3;assert(!gpsPositionAvailable(unknown,testNow));
    testNow+=1;
    const auto gpsBefore=parser.state();
    auto malformed=gps; malformed.resize(14); feed(parser,0x02,malformed);
    assert(parser.state().gpsRejectedFrames==gpsBefore.gpsRejectedFrames+1);
    assert(parser.state().gpsUpdatedMs==1234);
    malformed=gps;malformed[0]=0x7f;feed(parser,0x02,malformed);
    assert(parser.state().gpsUpdatedMs==1234); // out-of-range coordinates ignored
    assert(parser.state().gpsWireFrames==gpsBefore.gpsWireFrames+2);
    assert(parser.state().gpsDecodedFrames==gpsBefore.gpsDecodedFrames+1);
    assert(parser.state().gpsRejectedFrames==gpsBefore.gpsRejectedFrames+1);
    assert(parser.state().gpsWireUpdatedMs==testNow);
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
    // Captured 4S values, separate from the receiver's own supply voltage.
    testNow=10000;
    feed(parser,0x08,{0,66,0,0,0,0,0,0});
    const std::vector<uint8_t> cells={0,0x0f,0x94,0x0f,0xae,0x0f,0xa6,0x0f,0xc0};
    feed(parser,0x0e,cells);
    assert(parser.state().vehicleCellsValid && parser.state().vehiclePackMv==16040);
    assert(parser.state().vehicleCellsMv[0]==3988 && parser.state().vehicleCellsMv[1]==4014);
    assert(parser.state().vehicleCellsMv[2]==4006 && parser.state().vehicleCellsMv[3]==4032);
    assert(parser.state().vehicleMinimumCellMv==3988 && parser.state().receiverVoltageDecivolts==66);
    testNow=11000;
    auto invalid=cells;invalid[0]=1;feed(parser,0x0e,invalid);
    invalid=cells;invalid.pop_back();feed(parser,0x0e,invalid);
    invalid=cells;invalid.push_back(0);feed(parser,0x0e,invalid);
    invalid=cells;invalid[4]=0;invalid[3]=0;feed(parser,0x0e,invalid);
    invalid=cells;invalid[7]=0xff;feed(parser,0x0e,invalid);
    feed(parser,0x0e,cells,true);
    assert(parser.state().vehicleCellsUpdatedMs==10000 && parser.state().vehiclePackMv==16040);
    testNow=13000;parser.updateFreshness();assert(parser.state().vehicleCellsValid);
    testNow=13001;parser.updateFreshness();assert(parser.state().vehicleCellsValid);
    assert(parser.state().vehiclePackMv==16040 && parser.state().vehicleCellsMv[0]==3988);
    testNow=30000;parser.updateFreshness();assert(parser.state().vehicleCellsValid);
    testNow=30001;parser.updateFreshness();assert(!parser.state().vehicleCellsValid);
    feed(parser,0x0e,cells);assert(parser.state().vehicleCellsValid); // recovery on the next good packet
    assert(parser.state().receiverVoltageValid);
    testNow=UINT32_MAX-100;feed(parser,0x0e,cells);
    testNow=200;parser.updateFreshness();assert(parser.state().vehicleCellsValid);
    testNow=19899;parser.updateFreshness();assert(parser.state().vehicleCellsValid);
    testNow=19900;parser.updateFreshness();assert(!parser.state().vehicleCellsValid);
    // Variable packs are counted from cell data; shorter packs clear old slots.
    for(unsigned count : {1U, 3U, 4U, 6U, 12U}) {
        std::vector<uint8_t> packet={0};
        for(unsigned i=0;i<count;++i){packet.push_back(0x0f);packet.push_back(0xa0+i);}
        feed(parser,0x0e,packet);
        assert(parser.state().vehicleCellCount==count && parser.state().vehicleCellsValid);
        assert(parser.state().vehiclePackMv==count*4000+count*(count-1)/2);
        assert(parser.state().vehicleMinimumCellMv==4000);
        if(count<12)assert(parser.state().vehicleCellsMv[count]==0);
    }
    auto last=parser.state();
    std::vector<uint8_t> tooMany(27,0x0f);tooMany[0]=0;
    feed(parser,0x0e,tooMany);
    assert(parser.state().vehicleCellCount==last.vehicleCellCount && parser.state().vehiclePackMv==last.vehiclePackMv);
    feed(parser,0x0e,{0,0x0f,0xa0});
    assert(parser.state().vehicleCellCount==1 && parser.state().vehicleCellsMv[11]==0);
    testNow=20000;
    feed(parser,0x08,{0,66,0,0,0,0,0,0});
    const std::vector<uint8_t> fasCurrent={0,0,0x2d,0x27,0,0,0,0,1};
    feed(parser,0x08,fasCurrent);feed(parser,0x0e,{128,0,0});
    assert(parser.state().fasCurrentValid && parser.state().fasCurrentDeciA==11559);
    assert(parser.state().fasVoltageValid && parser.state().fasVoltageMv==0);
    assert(parser.state().receiverVoltageDecivolts==66 && parser.state().receiverVoltageValid);
    auto saved=parser.state();testNow=21000;
    auto badFas=fasCurrent;badFas[8]=2;feed(parser,0x08,badFas);
    badFas=fasCurrent;badFas.push_back(0);feed(parser,0x08,badFas);
    feed(parser,0x08,fasCurrent,true);feed(parser,0x0e,{128,0xff,0xff});
    feed(parser,0x0e,{128,0,0,0,0});feed(parser,0x0e,{129,0,0});
    assert(parser.state().fasCurrentUpdatedMs==saved.fasCurrentUpdatedMs);
    assert(parser.state().fasVoltageUpdatedMs==saved.fasVoltageUpdatedMs);
    feed(parser,0x0e,{128,0x3e,0xa8});assert(parser.state().fasVoltageMv==16040);
    testNow=24001;parser.updateFreshness();
    assert(!parser.state().fasCurrentValid && !parser.state().fasVoltageValid && parser.state().receiverVoltageValid);
    testNow=UINT32_MAX-100;feed(parser,0x08,fasCurrent);feed(parser,0x0e,{128,0,0});
    testNow=200;parser.updateFreshness();assert(parser.state().fasCurrentValid && parser.state().fasVoltageValid);
    feed(parser,0x08,{0,67,0,0,0,0,0,0,0});assert(parser.state().receiverVoltageDecivolts==67);
    testNow=50000;
    // ERS CU01 is tagged ID 2 at receiver ingress, never RX supply or FAS.
    feed(parser,0x08,{0,160,0,123,0,1,244,75,2});
    assert(parser.state().ersBatteryValid && parser.state().ersVoltageDeciV==160);
    assert(parser.state().ersCurrentDeciA==123 && parser.state().ersCapacityMah==500);
    assert(parser.state().ersRemainingPercent==75 && parser.state().receiverVoltageDecivolts==67);
    assert(parser.state().fasCurrentDeciA==11559);
    auto ers=parser.state();testNow=50100;
    for(auto invalidBattery : std::vector<std::vector<uint8_t>>{
        {0,160,0,123,0,1,244,101,2}, {2,189,0,123,0,1,244,75,2},
        {0,160,5,221,0,1,244,75,2}, {0,160,0,123,0,1,244,75,3}})feed(parser,0x08,invalidBattery);
    feed(parser,0x08,{0,160,0,123,0,1,244,75,2},true);
    assert(parser.state().ersBatteryUpdatedMs==ers.ersBatteryUpdatedMs);
    feed(parser,0x08,{0,0,0,0,0xff,0xff,0xff,0,2});
    assert(parser.state().ersBatteryValid && parser.state().ersCurrentDeciA==0 && parser.state().ersCapacityMah==0xffffff);
    testNow=53101;parser.updateFreshness();assert(!parser.state().ersBatteryValid && !parser.state().ersRemainingValid);
    testNow=UINT32_MAX-100;feed(parser,0x08,{0,160,0,123,0,1,244,75,2});
    testNow=200;parser.updateFreshness();assert(parser.state().ersBatteryValid);
    // CV01 fixed-width zero padding is discarded; internal zero cells rejected.
    auto padded=cells;padded.insert(padded.end(),8,0);feed(parser,0x0e,padded);
    assert(parser.state().vehicleCellCount==4 && parser.state().vehiclePackMv==16040 && parser.state().vehicleCellsMv[4]==0);
    testNow=300;auto broken=padded;broken[3]=broken[4]=0;feed(parser,0x0e,broken);
    assert(parser.state().vehicleCellsUpdatedMs==200);
    feed(parser,0x0e,std::vector<uint8_t>(17,0));assert(parser.state().vehicleCellsUpdatedMs==200);
    parser.setStandardBatteryVehicle(true);parser.updateFreshness();
    assert(!parser.state().receiverVoltageValid && !parser.state().ersBatteryValid);
    feed(parser,0x08,{0,160,0,123,0,1,244,75});
    assert(parser.state().ersBatteryValid && parser.state().ersCurrentDeciA==123);
    assert(!parser.state().receiverVoltageValid);
    feed(parser,0x08,{0,160,0,0,0,0,0,100,0});
    assert(parser.state().ersCurrentDeciA==0 && !parser.state().receiverVoltageValid);
    // Stock ER6G local ADC group remains independent of a CU01 battery.
    testNow=1000;
    feed(parser,0x0e,{128,0x1a,0x25}); // 6693 mV RX supply
    assert(parser.state().receiverVoltageValid && parser.state().receiverVoltageDecivolts==67);
    assert(parser.state().ersVoltageDeciV==160 && parser.state().ersCurrentDeciA==0);
    feed(parser,0x08,{0,0,0,0,0,0,0,0}); // CU01 powered, measurement input disconnected
    assert(parser.state().ersVoltageDeciV==0 && parser.state().receiverVoltageDecivolts==67);
    testNow=1100;feed(parser,0x0e,{128,0xff,0xff});
    feed(parser,0x0e,{129,0x1a,0x25});
    feed(parser,0x0e,{128,0x1a,0x25},true);
    assert(parser.state().batteryUpdatedMs==1000 && parser.state().receiverVoltageDecivolts==67);
    feed(parser,0x0e,{128,0x1a,0x25,0x30,0x39}); // additional ADC is not RX voltage
    assert(parser.state().receiverVoltageDecivolts==67 && parser.state().batteryUpdatedMs==1100);
    testNow=31100;parser.updateFreshness();assert(parser.state().receiverVoltageValid);
    testNow=31101;parser.updateFreshness();assert(!parser.state().receiverVoltageValid);
    testNow=UINT32_MAX-100;feed(parser,0x0e,{128,0x1a,0x25});
    testNow=200;parser.updateFreshness();assert(parser.state().receiverVoltageValid);
    feed(parser,0x0e,{128,0,0});assert(!parser.state().receiverVoltageValid);
    parser.setStandardBatteryVehicle(false);parser.updateFreshness();
    assert(!parser.state().ersBatteryValid);
    feed(parser,0x08,{0,67,0,0,0,0,0,0});assert(parser.state().receiverVoltageValid);
    // Unavailable FF fields must not be interpreted as -0.0000001 coordinates,
    // 6553.5 km/h speed, 255 satellites, or a reason to discard valid speed.
    CrsfTelemetryParser speedOnly;
    testNow=1000;feed(speedOnly,0x02,gps);
    const auto lastPosition=speedOnly.state();
    auto speedPacket=std::vector<uint8_t>(15,0xff);speedPacket[8]=1;speedPacket[9]=194;
    for(unsigned i=1;i<=20;++i){
        testNow=1000+i*100;feed(speedOnly,0x02,speedPacket);
        assert(gpsSpeedAvailable(speedOnly.state(),testNow));
        assert(!gpsPositionAvailable(speedOnly.state(),testNow) && !speedOnly.state().gpsCourseValid);
        assert(!speedOnly.state().gpsSatellitesValid && speedOnly.state().satellites==0);
        assert(speedOnly.state().latitudeE7==lastPosition.latitudeE7 && speedOnly.state().longitudeE7==lastPosition.longitudeE7);
        assert(speedOnly.state().gpsUpdatedMs==lastPosition.gpsUpdatedMs);
        assert(speedOnly.state().groundSpeedDeciKmh==450);
    }
    assert(speedOnly.state().gpsDecodedFrames==21 && speedOnly.state().gpsRejectedFrames==0);
    assert(speedOnly.state().gpsSpeedFrames==21);
    feed(speedOnly,0x02,speedPacket,true);assert(speedOnly.state().gpsSpeedFrames==21);
    auto unavailable=std::vector<uint8_t>(15,0xff);feed(speedOnly,0x02,unavailable);
    assert(!gpsSpeedAvailable(speedOnly.state(),testNow) && speedOnly.state().gpsRejectedFrames==1);
    testNow=4000;feed(speedOnly,0x02,gps);
    assert(gpsPositionAvailable(speedOnly.state(),testNow) && gpsSpeedAvailable(speedOnly.state(),testNow));
    testNow=7001;speedOnly.updateFreshness();assert(!gpsSpeedAvailable(speedOnly.state(),testNow));
    testNow=UINT32_MAX-100;feed(speedOnly,0x02,speedPacket);
    testNow=200;speedOnly.updateFreshness();assert(gpsSpeedAvailable(speedOnly.state(),testNow));
    // A sensor can retain nonzero coordinates while reporting acquisition.
    // Nonzero low counts cannot qualify a position; zero retains legacy
    // compatibility but never confirms a fix. FF remains unavailable.
    CrsfTelemetryParser acquiring;
    for(uint8_t satellites:{0,1,3}){
        auto waiting=gps;waiting[14]=satellites;
        testNow+=100;feed(acquiring,0x02,waiting);
        assert(acquiring.state().gpsSatellitesValid&&acquiring.state().satellites==satellites);
        assert(gpsSpeedAvailable(acquiring.state(),testNow));
        assert(gpsPositionAvailable(acquiring.state(),testNow)==(satellites==0));
        assert(!gpsFixConfirmed(acquiring.state(),testNow));
    }
    auto noFix=gps;for(unsigned n=0;n<8;++n)noFix[n]=0;noFix[14]=0;
    feed(acquiring,0x02,noFix);
    assert(acquiring.state().gpsSatellitesValid&&!gpsPositionAvailable(acquiring.state(),testNow));
    auto countUnknown=gps;countUnknown[14]=0xff;
    feed(acquiring,0x02,countUnknown);
    assert(!acquiring.state().gpsSatellitesValid&&acquiring.state().satellites==0);
    assert(gpsPositionAvailable(acquiring.state(),testNow));
    auto noPosition=std::vector<uint8_t>(15,0xff);noPosition[8]=noPosition[9]=0;noPosition[14]=8;
    feed(acquiring,0x02,noPosition);
    assert(acquiring.state().gpsSatellitesValid&&acquiring.state().satellites==8);
    assert(!gpsPositionAvailable(acquiring.state(),testNow)); // Count alone is not a usable position.
    assert(gpsSpeedAvailable(acquiring.state(),testNow)&&acquiring.state().groundSpeedDeciKmh==0);
    // Custom sensor replies do not pollute TX configuration or binding replies.
    auto txseq=parser.state().txParameterSequence;
    feed(parser,0x7b,{0xea,0xc8,3,1,0,0,'G','P','S',0});
    assert(parser.state().ersSequence==1);
    ErsReply ersReply;assert(parser.popErsReply(ersReply)&&ersReply.length==10);
    assert(ersReply.receivedMs==testNow&&ersReply.frameType==0x7b);
    assert(parser.state().txParameterSequence==txseq);
    feed(parser,0x7b,{0xec,0xc8,3,1,0,0});assert(parser.state().ersSequence==1);
    EscWire::Message esc;esc.session=0x12345678;esc.sequence=7;esc.flags=EscWire::SelectorAvailable;
    uint8_t escFrame[21];assert(EscWire::build(esc,true,escFrame,sizeof(escFrame))==21);
    const auto rxseq=parser.state().rxParameterSequence;
    for(auto b:escFrame)parser.feed(b);
    assert(parser.state().rxParameterSequence==rxseq+1&&parser.state().rxParameterLength==17);
    EscWire::Message decoded;assert(EscWire::parse(parser.state().rxParameterType,parser.state().rxParameterPayload,parser.state().rxParameterLength,true,decoded));
    assert(decoded.session==esc.session&&decoded.sequence==7&&parser.state().txParameterSequence==txseq);
    escFrame[20]^=1;for(auto b:escFrame)parser.feed(b);assert(parser.state().rxParameterSequence==rxseq+1);
    puts("CRSF telemetry transport, GPS, RPM, temperature, 1–12S cells, FAS voltage/current and freshness tests passed");
}
