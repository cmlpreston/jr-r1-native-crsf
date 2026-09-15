#include "dashboard_widgets.h"
#include <cassert>
#include <cstdio>
int main()
{
    static_assert(sizeof(DashboardWidget)==6,"Persisted widget shape changed; version the storage key");
    auto layout=defaultDashboardLayout();
    assert(dashboardWidgetMask(0,layout.cells[0])==3);
    auto timer=layout.cells[0];
    DashboardWidget map;map.kind=DashboardWidgetKind::Map;map.width=map.height=2;
    placeDashboardWidget(layout,0,map);
    assert(dashboardWidgetMask(0,layout.cells[0])==15);
    for(int i=1;i<4;++i)assert(layout.cells[i].kind==DashboardWidgetKind::Empty);
    // Replacing any covered cell removes the covering widget, never leaving overlap.
    placeDashboardWidget(layout,3,timer);
    assert(layout.cells[0].kind==DashboardWidgetKind::Empty && layout.cells[3].kind==DashboardWidgetKind::Timer);
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
    puts("dashboard_widgets_test: PASS");
}
