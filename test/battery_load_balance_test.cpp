#include "battery_load_balance.h"
#include <cassert>
#include <cstdio>

static void sample(BatteryLoadBalance &s,uint32_t now,uint16_t voltage=160,uint16_t current=250){
    const uint16_t cells[]={4000,4015,3980,4005};
    s.update(voltage,current,now,true,cells,4,now,true,now);
}
int main(){
    BatteryLoadBalance s;
    auto r=s.result(0);
    assert(!r.packFresh&&!r.cellsFresh&&!r.packExtremaValid&&!r.cellExtremaValid);
    sample(s,1000);r=s.result(1000);
    assert(r.packFresh&&r.cellsFresh&&r.packExtremaValid&&r.cellExtremaValid);
    assert(r.voltageDeciV==160&&r.currentDeciA==250&&r.minimumVoltageDeciV==160&&r.peakCurrentDeciA==250);
    assert(r.cellCount==4&&r.lowestCell==3&&r.highestCell==2);
    assert(r.lowestCellMv==3980&&r.highestCellMv==4015&&r.spreadMv==35);
    assert(r.minimumCellMv==3980&&r.worstSpreadMv==35);
    sample(s,2000,150,400);sample(s,3000,165,50);r=s.result(3000);
    assert(r.voltageDeciV==165&&r.currentDeciA==50&&r.minimumVoltageDeciV==150&&r.peakCurrentDeciA==400);

    // Preserve extrema while live sources expire independently.
    r=s.result(6000);assert(r.packFresh&&r.cellsFresh);
    r=s.result(6001);assert(!r.packFresh&&r.cellsFresh&&r.minimumVoltageDeciV==150);
    r=s.result(23000);assert(!r.packFresh&&r.cellsFresh);
    r=s.result(23001);assert(!r.packFresh&&!r.cellsFresh&&r.cellExtremaValid);
    s.update(0,0,3000,false,nullptr,0,3000,false,4000);
    sample(s,3000);r=s.result(4000);assert(!r.packFresh&&!r.cellsFresh); // Cached packet cannot reactivate.
    sample(s,4000);r=s.result(4000);assert(r.packFresh&&r.cellsFresh);

    // Repeated UI reads, altered duplicates and reordered samples cannot alter extrema.
    const uint16_t damaged[]={500,5000};
    s.update(0,65535,4000,true,damaged,2,4000,true,4100);
    r=s.result(4100);assert(r.minimumVoltageDeciV==150&&r.peakCurrentDeciA==400&&r.worstSpreadMv==35);
    s.update(0,65535,3500,true,damaged,2,3500,true,4100);
    r=s.result(4100);assert(!r.packFresh&&!r.cellsFresh&&r.peakCurrentDeciA==400&&r.worstSpreadMv==35);
    sample(s,4500);r=s.result(4500);assert(r.packFresh&&r.cellsFresh);

    // Reset rejects already displayed packets even if still marked fresh.
    s.reset(4500,4500);sample(s,4500);r=s.result(4500);
    assert(!r.packFresh&&!r.cellsFresh&&!r.packExtremaValid&&!r.cellExtremaValid);
    sample(s,4600);r=s.result(4600);assert(r.packFresh&&r.cellsFresh&&r.peakCurrentDeciA==250);
    s.reset();sample(s,0);r=s.result(0);assert(r.packFresh&&r.cellsFresh); // Timestamp zero is valid at boot.

    BatteryLoadBalance independent;
    const uint16_t lfp[]={3200,3200,3200,3200};
    independent.update(0,0,1000,true,lfp,4,1000,false,1000);
    r=independent.result(1000);assert(r.packFresh&&!r.cellsFresh&&r.packExtremaValid&&!r.cellExtremaValid);
    assert(r.voltageDeciV==0&&r.currentDeciA==0&&r.minimumVoltageDeciV==0&&r.peakCurrentDeciA==0);
    independent.update(123,100,1000,false,lfp,4,2000,true,2000);
    r=independent.result(2000);assert(!r.packFresh&&r.cellsFresh&&r.cellCount==4);
    assert(r.lowestCell==1&&r.highestCell==1&&r.spreadMv==0); // Deterministic first-cell ties.
    independent.update(123,100,3000,true,damaged,2,3000,true,3000);
    r=independent.result(3000);assert(r.cellsFresh&&r.lowestCell==1&&r.highestCell==2&&r.spreadMv==4500);
    assert(r.minimumCellMv==500&&r.worstSpreadMv==4500);
    const uint16_t single[]={3300};
    independent.update(125,200,4000,true,single,1,4000,true,4000);
    r=independent.result(4000);assert(r.cellCount==1&&r.lowestCell==1&&r.highestCell==1&&r.spreadMv==0);
    assert(r.minimumCellMv==500&&r.worstSpreadMv==4500); // Count changes do not erase observed session extrema.
    independent.update(125,200,4000,true,single,0,5000,true,5000);
    r=independent.result(5000);assert(r.packFresh&&!r.cellsFresh&&r.worstSpreadMv==4500);

    // Validate all admitted counts and all reported cells, not cached parser summaries.
    uint16_t cells[13];for(unsigned i=0;i<13;++i)cells[i]=3500+i;
    for(uint8_t count=1;count<=12;++count){
        BatteryLoadBalance size;size.update(160,0,1000,false,cells,count,1000,true,1000);
        r=size.result(1000);assert(r.cellsFresh&&r.cellCount==count&&r.lowestCell==1&&r.highestCell==count);
        assert(r.spreadMv==count-1);
    }
    BatteryLoadBalance invalid;
    invalid.update(160,0,1000,false,cells,13,1000,true,1000);assert(!invalid.result(1000).cellsFresh);
    invalid.update(160,0,1000,false,cells,0,1000,true,1000);assert(!invalid.result(1000).cellsFresh);
    invalid.update(160,0,1000,false,nullptr,4,1000,true,1000);assert(!invalid.result(1000).cellsFresh);
    cells[3]=0;invalid.update(160,0,1000,false,cells,4,1000,true,1000);
    assert(!invalid.result(1000).cellExtremaValid);
    cells[3]=5001;invalid.update(160,0,1000,false,cells,4,1000,true,1000);
    assert(!invalid.result(1000).cellExtremaValid);
    cells[3]=499;invalid.update(160,0,1000,false,cells,4,1000,true,1000);
    assert(!invalid.result(1000).cellExtremaValid);

    // Neither stale nor future packet time is accepted as a new observation.
    BatteryLoadBalance stale;
    stale.update(160,100,1000,true,lfp,4,1000,true,21001);r=stale.result(21001);
    assert(!r.packExtremaValid&&!r.cellExtremaValid);
    stale.update(160,100,22000,true,lfp,4,22000,true,21001);r=stale.result(21001);
    assert(!r.packExtremaValid&&!r.cellExtremaValid);
    sample(stale,22000);assert(stale.result(22000).packFresh&&stale.result(22000).cellsFresh);
    BatteryLoadBalance wrap;
    sample(wrap,UINT32_MAX-999,165,30);sample(wrap,500,155,200);r=wrap.result(500);
    assert(r.packFresh&&r.cellsFresh&&r.minimumVoltageDeciV==155&&r.peakCurrentDeciA==200);
    wrap.reset(UINT32_MAX-99,UINT32_MAX-99);sample(wrap,50);assert(wrap.result(50).packFresh);

    struct Telemetry {
        bool ersBatteryValid=true,vehicleCellsValid=true;
        uint16_t ersVoltageDeciV=160,ersCurrentDeciA=100;
        uint32_t ersBatteryUpdatedMs=1000,vehicleCellsUpdatedMs=1000;
        uint8_t vehicleCellCount=2;
        uint16_t vehicleCellsMv[12]={4000,3990};
    } telemetry;
    BatteryLoadBalance adapter;adapter.update(telemetry,1000);r=adapter.result(1000);
    assert(r.packFresh&&r.cellsFresh&&r.voltageDeciV==160&&r.lowestCell==2&&r.spreadMv==10);

    puts("Battery load/balance: independent CU01/CV01 freshness, observed extrema, reset/cache guards, validation and wrap PASS");
}
