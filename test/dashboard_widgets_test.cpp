#include "dashboard_widgets.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main()
{
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
    speed.source=1;assert(sanitizedDashboardWidget(0,speed).style==DashboardWidgetStyle::Numbers);
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
    puts("dashboard_widgets_test: PASS");
}
