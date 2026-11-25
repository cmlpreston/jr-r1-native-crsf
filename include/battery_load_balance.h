#pragma once
#include <stdint.h>

// UI-owned observations. CU01 load and CV01 cells have independent clocks;
// neither fills missing data for the other, and no resistance/health estimate
// is inferred by pairing asynchronous readings.
class BatteryLoadBalance {
public:
    static constexpr uint32_t PackFreshnessMs=3000,CellsFreshnessMs=20000;
    static constexpr uint8_t MaxCells=12;
    struct Result {
        bool packFresh=false,cellsFresh=false;
        bool packExtremaValid=false,cellExtremaValid=false;
        uint16_t voltageDeciV=0,currentDeciA=0;
        uint16_t minimumVoltageDeciV=0,peakCurrentDeciA=0;
        uint8_t cellCount=0,lowestCell=0,highestCell=0; // Physical cell indices, 1-based.
        uint16_t lowestCellMv=0,highestCellMv=0,spreadMv=0;
        uint16_t minimumCellMv=0,worstSpreadMv=0;
    };

    void reset(){*this=BatteryLoadBalance{};}
    void resumeAfterPowerCycle(uint32_t powerStamp=0,uint32_t cellsStamp=0){powerSeen_=cellsSeen_=true;powerActive_=cellsActive_=false;powerStamp_=powerStamp;cellsStamp_=cellsStamp;}
    void reset(uint32_t powerStamp,uint32_t cellsStamp){
        reset();powerSeen_=cellsSeen_=true;powerStamp_=powerStamp;cellsStamp_=cellsStamp;
    }
    void update(uint16_t voltageDeciV,uint16_t currentDeciA,uint32_t powerStamp,bool powerValid,
                const uint16_t *cells,uint8_t count,uint32_t cellsStamp,bool cellsValid,uint32_t now){
        updatePack(voltageDeciV,currentDeciA,powerStamp,powerValid,now);
        updateCells(cells,count,cellsStamp,cellsValid,now);
    }
    template<class Telemetry>void update(const Telemetry &t,uint32_t now){
        update(t.ersVoltageDeciV,t.ersCurrentDeciA,t.ersBatteryUpdatedMs,t.ersBatteryValid,
               t.vehicleCellsMv,t.vehicleCellCount,t.vehicleCellsUpdatedMs,t.vehicleCellsValid,now);
    }
    Result result(uint32_t now)const{
        Result r=reading_;
        r.packFresh=powerActive_&&uint32_t(now-powerStamp_)<=PackFreshnessMs;
        r.cellsFresh=cellsActive_&&uint32_t(now-cellsStamp_)<=CellsFreshnessMs;
        return r;
    }
private:
    // Unsigned subtraction permits normal millis rollover while rejecting
    // duplicate/reordered packets. A rejected packet cannot move a baseline.
    static bool newer(uint32_t stamp,uint32_t previous,bool seen){
        const uint32_t elapsed=stamp-previous;
        return !seen||(elapsed&&elapsed<0x80000000u);
    }
    void updatePack(uint16_t voltage,uint16_t current,uint32_t stamp,bool valid,uint32_t now){
        if(!valid||uint32_t(now-stamp)>PackFreshnessMs){powerActive_=false;return;}
        if(!newer(stamp,powerStamp_,powerSeen_)){
            if(stamp!=powerStamp_)powerActive_=false;
            return;
        }
        reading_.voltageDeciV=voltage;reading_.currentDeciA=current;
        if(!reading_.packExtremaValid||voltage<reading_.minimumVoltageDeciV)
            reading_.minimumVoltageDeciV=voltage;
        if(!reading_.packExtremaValid||current>reading_.peakCurrentDeciA)
            reading_.peakCurrentDeciA=current;
        reading_.packExtremaValid=true;powerSeen_=powerActive_=true;powerStamp_=stamp;
    }
    void updateCells(const uint16_t *cells,uint8_t count,uint32_t stamp,bool valid,uint32_t now){
        if(!valid||!cells||!count||count>MaxCells||uint32_t(now-stamp)>CellsFreshnessMs){
            cellsActive_=false;return;
        }
        if(!newer(stamp,cellsStamp_,cellsSeen_)){
            if(stamp!=cellsStamp_)cellsActive_=false;
            return;
        }
        uint16_t minimum=65535,maximum=0;
        uint8_t minimumIndex=0,maximumIndex=0;
        for(uint8_t i=0;i<count;++i){
            // Match the CV01 parser's admitted cell range, including LFP.
            if(cells[i]<500||cells[i]>5000){cellsActive_=false;return;}
            if(cells[i]<minimum){minimum=cells[i];minimumIndex=i+1;}
            if(cells[i]>maximum){maximum=cells[i];maximumIndex=i+1;}
        }
        const uint16_t spread=maximum-minimum;
        reading_.cellCount=count;reading_.lowestCell=minimumIndex;reading_.highestCell=maximumIndex;
        reading_.lowestCellMv=minimum;reading_.highestCellMv=maximum;reading_.spreadMv=spread;
        if(!reading_.cellExtremaValid||minimum<reading_.minimumCellMv)reading_.minimumCellMv=minimum;
        if(!reading_.cellExtremaValid||spread>reading_.worstSpreadMv)reading_.worstSpreadMv=spread;
        reading_.cellExtremaValid=true;cellsSeen_=cellsActive_=true;cellsStamp_=stamp;
    }
    Result reading_;
    uint32_t powerStamp_=0,cellsStamp_=0;
    bool powerSeen_=false,cellsSeen_=false,powerActive_=false,cellsActive_=false;
};
