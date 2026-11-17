#include "run_records.h"
#include "run_record_store.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstdio>

int main(){
    char directory[]="/tmp/r1-record-test.XXXXXX";assert(mkdtemp(directory));
    assert(setenv("JR_R1_SIM_STATE",directory,1)==0);
    auto source=std::make_unique<RunRecords>();auto restored=std::make_unique<RunRecords>();
    auto &r=*source;
    r.sensors[2].update(12345,1000,1000,true);
    r.sensors[2].update(15000,2000,2000,true);
    r.speed.update(600,1000,1000,true);r.speed.update(683,2000,2000,true);
    r.power.update(200,100,1000,true,600,1000,true,1000);
    r.power.update(200,100,2000,true,683,2000,true,2000);
    r.trip.update(200,100,20,1000,true,600,1000,true,1000);
    r.trip.update(200,100,30,2000,true,683,2000,true,2000);
    const uint16_t cells[]={3800,3900};r.battery.update(200,100,2000,true,cells,2,2000,true,2000);
    r.timer.settings.mode=TimerMode::Lap;r.timer.settings.minimumLapSeconds=1;
    r.timer.resume(0);assert(r.timer.recordLap(1500));
    r.addGps(-370000000,1450000000,1000);r.addGps(-370000010,1450000010,2000);
    assert(r.valid());assert(RunRecordStore::write(0,&r,sizeof(r)));
    assert(!RunRecordStore::read(1,restored.get(),sizeof(*restored)));
    // A truncated pending write never replaces the last complete checkpoint.
    {std::ofstream pending(std::string(directory)+"/run-records/model-0.dat.pending");pending<<"partial";}
    assert(RunRecordStore::read(0,restored.get(),sizeof(*restored)));assert(restored->valid());
    assert(!RunRecordStore::read(0,restored.get(),sizeof(*restored)-1));
    const auto pendingPath=std::string(directory)+"/run-records/model-0.dat.pending";
    std::filesystem::remove(pendingPath);std::filesystem::create_directory(pendingPath);
    assert(!RunRecordStore::write(0,&r,sizeof(r))&&!RunRecordStore::erase(0));
    assert(RunRecordStore::read(0,restored.get(),sizeof(*restored))); // Failed write/delete preserve old file.
    std::filesystem::remove(pendingPath);
    const auto energy=restored->power.result(2000).wattHours;
    restored->resumeAfterPowerCycle(50);
    assert(!restored->timer.running()&&!restored->timer.ready()&&restored->timer.lap(0)==1500);
    assert(restored->sensors[2].result(true,50).maximum==15000&&!restored->sensors[2].result(false,50).valid);
    assert(restored->sensors[2].result(true,50).average==13672);
    assert(restored->speed.maximum()==683);
    auto cached=restored->sensors[2];cached.resumeAfterPowerCycle(2000);
    cached.update(99999,2000,2000,true);assert(cached.result(true,2000).maximum==15000);
    cached.update(16000,2100,2100,true);assert(cached.result(true,2100).maximum==16000);
    assert(!restored->power.result(50).powerValid&&!restored->power.result(50).energyComplete);
    assert(!restored->battery.result(50).packFresh&&!restored->battery.result(50).cellsFresh);
    assert(restored->battery.result(50).minimumCellMv==3800);
    assert(restored->trip.result(50).usedMah==10);
    restored->power.update(200,100,100,true,600,100,true,100);
    restored->trip.update(200,100,10000,100,true,600,100,true,100);
    assert(restored->power.result(100).wattHours==energy); // No integration across reboot.
    assert(restored->trip.result(100).usedMah==10); // Counter baseline, no phantom charge.
    restored->trip.update(200,100,10001,1100,true,600,1100,true,1100);
    assert(restored->trip.result(1100).usedMah==11);
    restored->sensors[2].update(10000,100,100,true);
    assert(restored->sensors[2].result(true,100).maximum==15000);
    restored->addGps(-370000020,1450000020,100);assert(restored->gpsCount==3);
    assert(restored->gps[2].segment!=restored->gps[1].segment);
    assert(RunRecordStore::write(1,restored.get(),sizeof(*restored)));
    assert(RunRecordStore::erase(0));assert(!RunRecordStore::read(0,source.get(),sizeof(*source)));
    assert(RunRecordStore::read(1,source.get(),sizeof(*source))); // Model isolation on delete.
    // Corrupt committed payload is rejected, not restored as fresh telemetry.
    {std::fstream file(std::string(directory)+"/run-records/model-1.dat",std::ios::in|std::ios::out|std::ios::binary);
     file.seekp(-1,std::ios::end);file.put(char(0xa5));}
    assert(!RunRecordStore::read(1,source.get(),sizeof(*source)));
    r.clear();
    for(uint32_t i=0;i<20000;++i)r.addGps(-370000000-int32_t(i),1450000000+int32_t(i),i*1000);
    assert(r.valid()&&r.gpsCount<=RunRecords::MaxGps&&r.gpsPeriodMs>1000);
    assert(r.gps[0].latitude==-370000000);
    r.clearGps();assert(r.gpsCount==0&&r.gpsPeriodMs==1000&&r.gps[100].latitude==0);
    std::filesystem::remove_all(directory);
    puts("Run records: reboot/freshness, paused laps, no phantom integrals, atomic checkpoint, corruption, model deletion and bounded GPS PASS");
}
