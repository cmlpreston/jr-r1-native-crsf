#include "dashboard_widgets.h"
#include "drive_pack_widget.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main()
{
    static_assert(int(DashboardWidgetKind::Course)==9,"Retired layout ID must stay reserved");
    DashboardWidget course;course.kind=DashboardWidgetKind::Course;course.width=2;
    assert(sanitizedDashboardWidget(0,course).kind==DashboardWidgetKind::Empty);
    const uint8_t retired[]={9,2,2,1,0,1};
    assert(dashboardWidgetFromV2(0,retired).kind==DashboardWidgetKind::Empty);
    assert(nextDashboardWidgetKind(DashboardWidgetKind::BatteryCells,1)==DashboardWidgetKind::Controls);
    assert(nextDashboardWidgetKind(DashboardWidgetKind::Empty,-1)==DashboardWidgetKind::SpeedRpm);
    assert(nextDashboardWidgetKind(DashboardWidgetKind::DrivePack,1)==DashboardWidgetKind::PowerEnergy);
    assert(nextDashboardWidgetKind(DashboardWidgetKind::PowerEnergy,1)==DashboardWidgetKind::ControlHistory);
    static_assert(int(DashboardWidgetKind::Controls)==10,"Append widget IDs without changing saved layouts");
    static_assert(int(DashboardWidgetKind::RfMargin)==12&&int(DashboardWidgetKind::DrivePack)==13,"Append CU01 widget without renumbering saved IDs");
    static_assert(int(DashboardWidgetKind::PowerEnergy)==14&&int(DashboardWidgetKind::ControlHistory)==15,"Append metrics widgets without changing saved layouts");
    static_assert(int(DashboardWidgetKind::TripComputer)==16&&int(DashboardWidgetKind::RunSummary)==17&&int(DashboardWidgetKind::BatteryLoadBalance)==18,"Keep new session widget IDs stable");
    assert(nextDashboardWidgetKind(DashboardWidgetKind::ControlHistory,1)==DashboardWidgetKind::TripComputer);
    static_assert(int(DashboardWidgetKind::SpeedRpm)==19,"Append dual-dial kind without changing earlier IDs");
    assert(nextDashboardWidgetKind(DashboardWidgetKind::BatteryLoadBalance,1)==DashboardWidgetKind::SpeedRpm);
    assert(nextDashboardWidgetKind(DashboardWidgetKind::SpeedRpm,1)==DashboardWidgetKind::Empty);
    for(int style=0;style<int(DashboardWidgetStyle::Count);++style){
        DashboardWidget w;w.kind=DashboardWidgetKind::SpeedRpm;w.style=DashboardWidgetStyle(style);
        assert(dashboardWidgetKindAvailable(w.kind));
        const auto fixed=sanitizedDashboardWidget(0,w);
        assert(fixed.kind==DashboardWidgetKind::SpeedRpm&&fixed.style==DashboardWidgetStyle::Dials);
    }
    for(auto kind:{DashboardWidgetKind::Controls,DashboardWidgetKind::Trims,DashboardWidgetKind::RfMargin,DashboardWidgetKind::DrivePack,DashboardWidgetKind::PowerEnergy,DashboardWidgetKind::TripComputer,DashboardWidgetKind::RunSummary,DashboardWidgetKind::BatteryLoadBalance}){
        DashboardWidget w;w.kind=kind;w.style=DashboardWidgetStyle::Graph;
        assert(dashboardWidgetKindAvailable(kind));
        assert(sanitizedDashboardWidget(0,w).kind==kind);
        assert(sanitizedDashboardWidget(0,w).style==DashboardWidgetStyle::Numbers);
    }
    for(int style=0;style<int(DashboardWidgetStyle::Count);++style){
        DashboardWidget w;w.kind=DashboardWidgetKind::ControlHistory;w.style=DashboardWidgetStyle(style);
        assert(sanitizedDashboardWidget(0,w).style==DashboardWidgetStyle::Graph);
    }
    // Every prior stored kind retains its meaning; the two new types fit the
    // existing byte schema and every supported size without changing defaults.
    for(auto kind:{DashboardWidgetKind::PowerEnergy,DashboardWidgetKind::ControlHistory,DashboardWidgetKind::TripComputer,DashboardWidgetKind::RunSummary,DashboardWidgetKind::BatteryLoadBalance,DashboardWidgetKind::SpeedRpm}){
        for(int width=1;width<=2;++width)for(int height=1;height<=2;++height){
            DashboardWidget w;w.kind=kind;w.width=width;w.height=height;
            const auto fixed=sanitizedDashboardWidget(0,w);
            assert(fixed.kind==kind&&fixed.width==width&&fixed.height==height);
            const uint8_t bytes[]={uint8_t(kind),uint8_t(fixed.style),uint8_t(width),uint8_t(height),0,1};
            const auto decoded=dashboardWidgetFromV2(0,bytes);
            assert(decoded.kind==kind&&decoded.style==fixed.style);
        }
    }
    uint8_t monitor[DashboardMonitorMaxEntries]={};
    assert(dashboardMonitorEntries(0xffff,2,monitor)==21);
    assert(monitor[16]==(0x80|3)&&monitor[17]==(0x80|9));
    assert(dashboardMonitorEntries(0xffff,1,monitor)==5);
    assert(monitor[0]!=(0x80|10)&&monitor[1]!=(0x80|10));
    assert(monitor[2]==(0x80|2)&&monitor[3]==(0x80|4)&&monitor[4]==(0x80|5));
    assert(dashboardMonitorEntries(5,0,monitor)==2&&monitor[0]==0&&monitor[1]==2);

    assert(dashboardMonitorEntries(0xffff,2,monitor,0x1fffff)==0);
    assert(dashboardMonitorEntries(0xffff,2,monitor,dashboardMonitorBit(2)|dashboardMonitorBit(0x84))==19);
    for(int i=0;i<19;++i)assert(monitor[i]!=2&&monitor[i]!=0x84);
    assert(dashboardMonitorEntries(5,0,monitor,dashboardMonitorBit(2))==1&&monitor[0]==0);
    assert(dashboardMonitorEntries(0,1,monitor,dashboardMonitorBit(0x82))==4);
    static_assert(sizeof(DashboardWidget)==7,"Persisted widget shape changed; version the storage key");
    assert(dashboardChannelCapacity(147,162)==4);
    assert(dashboardChannelCapacity(300,162)==8);
    assert(dashboardChannelCapacity(147,330)==10);
    assert(dashboardChannelCapacity(300,330)==16);
    uint8_t channels[16];assert(dashboardConfiguredChannels(0x8005,channels)==3);
    assert(channels[0]==0&&channels[1]==2&&channels[2]==15);
    DashboardWidget all;all.kind=DashboardWidgetKind::AllChannels;all.style=DashboardWidgetStyle::Bars;
    assert(sanitizedDashboardWidget(0,all).kind==DashboardWidgetKind::AllChannels);
    all.style=DashboardWidgetStyle::VerticalBars;
    assert(sanitizedDashboardWidget(0,all).style==DashboardWidgetStyle::VerticalBars);
    static_assert(static_cast<int>(DashboardWidgetStyle::Switches)==4,"Existing style IDs must remain stable");
    assert(dashboardChannelCapacity(147,162,all.style)==2);
    assert(dashboardChannelCapacity(300,330,all.style)==16);
    assert(dashboardChannelCapacity(227,92,all.style)==3);
    all.kind=DashboardWidgetKind::Channels;
    assert(sanitizedDashboardWidget(0,all).style!=DashboardWidgetStyle::VerticalBars);
    DashboardWidget speed;speed.kind=DashboardWidgetKind::Sensor;speed.style=DashboardWidgetStyle::Dials;
    assert(sanitizedDashboardWidget(0,speed).style==DashboardWidgetStyle::Dials);
    speed.source=1;assert(sanitizedDashboardWidget(0,speed).kind==DashboardWidgetKind::Empty);
    // Pack summaries retain IDs; old per-cell widgets migrate to the group.
    for(uint8_t source=0;source<DashboardSensorCount;++source){
        speed.source=source;speed.style=DashboardWidgetStyle::Graph;
        assert(sanitizedDashboardWidget(0,speed).source==(source>=8 && source<12?0:source));
        const uint8_t stored[]={4,3,1,1,source,1};
        assert(dashboardWidgetFromV2(0,stored).source==(source>=8 && source<12?0:source));
    }
    speed.source=DashboardSensorCount;assert(sanitizedDashboardWidget(0,speed).source==0);
    assert(nextDashboardSensorSource(7,1)==14);
    assert(nextDashboardSensorSource(14,-1)==7);
    assert(displaySensorSource(0)==0 && displaySensorSource(1)==2 && displaySensorSource(10)==11);
    assert(displaySensorSource(11)==14 && displaySensorSource(14)==17);
    assert(displaySensorSource(15)==DashboardSensorCount);
    assert(nextDashboardSensorSource(0,1)==2 && nextDashboardSensorSource(2,-1)==0);
    assert(nextDashboardSensorSource(17,1)==0 && nextDashboardSensorSource(0,-1)==17);
    auto layout=defaultDashboardLayout();
    assert(dashboardWidgetMask(0,layout.cells[0])==3);
    auto timer=layout.cells[0];
    DashboardWidget map;map.kind=DashboardWidgetKind::Map;map.width=map.height=2;
    placeDashboardWidget(layout,0,map);
    assert(dashboardWidgetMask(0,layout.cells[0])==15);
    assert(visibleDashboardWidgets(layout)==1);
    assert(layout.cells[2].kind==DashboardWidgetKind::Power);
    assert(layout.cells[3].kind==DashboardWidgetKind::Channels);
    // Reload sanitation preserves covered widgets and all their settings.
    layout.cells[3].source=7;layout.cells[3].secondSource=10;
    sanitizeDashboardLayout(layout);
    assert(layout.cells[3].source==7&&layout.cells[3].secondSource==10);
    map.width=map.height=1;placeDashboardWidget(layout,0,map);
    assert(visibleDashboardWidgets(layout)==13);
    assert(layout.cells[3].style==DashboardWidgetStyle::Bars);
    // Hidden widgets can be edited without removing the larger panel.
    map.width=map.height=2;placeDashboardWidget(layout,0,map);
    placeDashboardWidget(layout,3,timer);
    assert(layout.cells[0].kind==DashboardWidgetKind::Map);
    assert(layout.cells[3].kind==DashboardWidgetKind::Timer);
    assert(visibleDashboardWidgets(layout)==1);
    map.height=1;placeDashboardWidget(layout,0,map);
    assert(visibleDashboardWidgets(layout)==13);
    // A hidden wide widget must not hide other panels itself.
    DashboardWidget wide=timer;wide.width=2;placeDashboardWidget(layout,2,wide);
    assert(visibleDashboardWidgets(layout)==5);
    wide.width=1;placeDashboardWidget(layout,2,wide);
    assert(visibleDashboardWidgets(layout)==13);
    // Malformed persistence cannot yield an invalid source, enum or off-screen span.
    layout.cells[0].kind=static_cast<DashboardWidgetKind>(255);
    layout.cells[1]=map;layout.cells[1].width=200;
    layout.cells[2].kind=DashboardWidgetKind::Sensor;layout.cells[2].source=255;
    layout.cells[2].style=DashboardWidgetStyle::Switches;
    sanitizeDashboardLayout(layout);
    assert(layout.cells[0].kind==DashboardWidgetKind::Empty);
    assert(layout.cells[1].width==1 && layout.cells[1].height==1);
    assert(layout.cells[2].source==0 && layout.cells[2].style==DashboardWidgetStyle::Numbers);
    // Repeated edits are deterministic and leave the other model untouched.
    auto other=defaultDashboardLayout();placeDashboardWidget(layout,0,map);
    assert(other.cells[2].kind==DashboardWidgetKind::Power);
    const uint8_t oldSpeed[]={4,3,2,1,0,16};
    auto migrated=dashboardWidgetFromV2(0,oldSpeed);
    assert(migrated.kind==DashboardWidgetKind::Sensor && migrated.style==DashboardWidgetStyle::Graph);
    assert(migrated.width==2 && migrated.height==1 && migrated.source==0 && migrated.secondSource==16);
    assert(migrated.speedValue==SpeedWidgetValue::Current);
    migrated.speedValue=static_cast<SpeedWidgetValue>(255);
    assert(sanitizedDashboardWidget(0,migrated).speedValue==SpeedWidgetValue::Current);
    for(auto value:{SpeedWidgetValue::Current,SpeedWidgetValue::Maximum,SpeedWidgetValue::Average}){
        migrated.speedValue=value;assert(sanitizedDashboardWidget(0,migrated).speedValue==value);
    }
    for(uint8_t anchor:{0,1}){
        DashboardLayout tallLayout;for(auto &w:tallLayout.cells)w.kind=DashboardWidgetKind::Channels;
        auto tall=tallLayout.cells[anchor];tall.height=2;placeDashboardWidget(tallLayout,anchor,tall);
        assert(tallLayout.cells[anchor].width==1&&tallLayout.cells[anchor].height==2);
        assert(dashboardWidgetMask(anchor,tall)==(anchor==0?5:10));
        assert(!(visibleDashboardWidgets(tallLayout)&(1U<<(anchor+2))));
        sanitizeDashboardLayout(tallLayout);assert(tallLayout.cells[anchor].height==2);
        tall.height=1;placeDashboardWidget(tallLayout,anchor,tall);assert(visibleDashboardWidgets(tallLayout)==15);
    }
    for(uint8_t anchor=0;anchor<4;++anchor){
        DashboardWidget w;const int sizes=anchor==0?4:anchor==3?1:2;
        for(int i=0;i<sizes;++i){cycleDashboardWidgetSize(anchor,w,1);assert(anchor%2+w.width<=2&&anchor/2+w.height<=2);}
        assert(w.width==1&&w.height==1);
        cycleDashboardWidgetSize(anchor,w,-1);cycleDashboardWidgetSize(anchor,w,1);assert(w.width==1&&w.height==1);
        w.height=2;if(anchor>=2)assert(sanitizedDashboardWidget(anchor,w).height==1);
    }
    for(uint8_t source=8;source<12;++source){
        DashboardWidget old;old.kind=DashboardWidgetKind::Sensor;old.source=source;old.style=DashboardWidgetStyle::Graph;
        auto cells=sanitizedDashboardWidget(0,old);
        assert(cells.kind==DashboardWidgetKind::BatteryCells && cells.source==0 && cells.style==DashboardWidgetStyle::Numbers);
        for(uint8_t count=0;count<=12;++count){cells.source=count;assert(sanitizedDashboardWidget(0,cells).source==count);}
        cells.source=13;assert(sanitizedDashboardWidget(0,cells).source==0);
    }
    // The combined CU01 card keeps the same layout bytes and every supported span.
    for(uint8_t width:{1,2})for(uint8_t height:{1,2}){
        DashboardWidget pack;pack.kind=DashboardWidgetKind::DrivePack;pack.width=width;pack.height=height;
        const auto sanitized=sanitizedDashboardWidget(0,pack);
        assert(sanitized.kind==pack.kind&&sanitized.width==width&&sanitized.height==height);
    }
    const auto unchangedDefault=defaultDashboardLayout();
    assert(unchangedDefault.cells[0].kind==DashboardWidgetKind::Timer);
    assert(unchangedDefault.cells[2].kind==DashboardWidgetKind::Power);
    struct Cu01Sample {
        bool ersBatteryValid=false,ersRemainingValid=false;
        uint16_t ersVoltageDeciV=164,ersCurrentDeciA=123;
        uint32_t ersCapacityMah=1234,ersBatteryUpdatedMs=100;
        uint8_t ersRemainingPercent=75;
        bool receiverVoltageValid=true,vehicleCellsValid=true;
        uint16_t receiverVoltageDecivolts=62,vehiclePackMv=15400;
    }t;
    auto reading=drivePackWidgetReading(t,100);
    assert(!reading.fresh&&!reading.remainingValid); // Other supplies never fill missing CU01 data.
    t.ersBatteryValid=true;t.ersRemainingValid=true;
    reading=drivePackWidgetReading(t,3100);
    assert(reading.fresh&&reading.voltageDeciV==164&&reading.currentDeciA==123&&reading.usedMah==1234);
    assert(reading.remainingValid&&reading.remainingPercent==75);
    reading=drivePackWidgetReading(t,3101);assert(!reading.fresh&&!reading.remainingValid);
    t.ersBatteryUpdatedMs=0xfffffff0U;
    reading=drivePackWidgetReading(t,0x10);assert(reading.fresh&&reading.remainingValid);
    assert(drivePackWidgetReading(t,t.ersBatteryUpdatedMs+3000).fresh);
    assert(!drivePackWidgetReading(t,t.ersBatteryUpdatedMs+3001).fresh);
    for(uint8_t percent:{0,100,101,255}){
        t.ersRemainingPercent=percent;
        assert(drivePackWidgetReading(t,0x10).remainingValid==(percent<=100));
    }
    t.ersRemainingValid=false;t.ersRemainingPercent=75;
    assert(!drivePackWidgetReading(t,0x10).remainingValid);
    puts("dashboard_widgets_test: PASS");
}
