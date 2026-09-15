#pragma once
#include <stdint.h>

// Persisted UI-only data. Each model has a two-column, two-row dashboard.
// Fields are bytes (no ABI padding); version is encoded in the NVS key.
enum class DashboardWidgetKind : uint8_t { Empty, Timer, Power, Channels, Sensor, Map, Model, Count };
enum class DashboardWidgetStyle : uint8_t { Numbers, Bars, Dials, Graph, Switches, Count };
enum class SpeedWidgetValue : uint8_t { Current, Maximum, Average, Count };
struct DashboardWidget {
    DashboardWidgetKind kind = DashboardWidgetKind::Empty;
    DashboardWidgetStyle style = DashboardWidgetStyle::Numbers;
    uint8_t width = 1, height = 1;
    uint8_t source = 0, secondSource = 1;
    SpeedWidgetValue speedValue = SpeedWidgetValue::Current;
};
struct DashboardLayout { DashboardWidget cells[4]; };
inline uint8_t dashboardWidgetMask(uint8_t cell, const DashboardWidget &widget)
{
    if (cell >= 4 || widget.kind == DashboardWidgetKind::Empty) return 0;
    uint8_t mask = 0;
    for (uint8_t y=0;y<widget.height;++y)
        for (uint8_t x=0;x<widget.width;++x)
            if (cell%2+x<2 && cell/2+y<2) mask |= 1U << (cell+y*2+x);
    return mask;
}
inline bool dashboardStyleAllowed(DashboardWidgetKind kind, DashboardWidgetStyle style)
{
    if (style == DashboardWidgetStyle::Numbers) return true;
    if (kind == DashboardWidgetKind::Power) return style == DashboardWidgetStyle::Bars || style == DashboardWidgetStyle::Dials;
    if (kind == DashboardWidgetKind::Channels) return style < DashboardWidgetStyle::Count;
    if (kind == DashboardWidgetKind::Sensor) return style == DashboardWidgetStyle::Graph;
    return false;
}
inline DashboardWidget sanitizedDashboardWidget(uint8_t cell, DashboardWidget widget)
{
    if (widget.speedValue >= SpeedWidgetValue::Count) widget.speedValue = SpeedWidgetValue::Current;
    if (widget.kind >= DashboardWidgetKind::Count) widget = DashboardWidget{};
    if (!dashboardStyleAllowed(widget.kind, widget.style)) widget.style = DashboardWidgetStyle::Numbers;
    widget.width = widget.width == 2 && cell%2==0 ? 2 : 1;
    widget.height = widget.height == 2 && cell/2==0 && widget.width==2 ? 2 : 1;
    if (widget.source > 15) widget.source = 0;
    if (widget.secondSource > 16) widget.secondSource = 1;
    if (widget.kind == DashboardWidgetKind::Sensor && widget.source > 5) widget.source = 0;
    return widget;
}
// v2 stored six bytes per widget; decode explicitly rather than copying structs.
inline DashboardWidget dashboardWidgetFromV2(uint8_t cell, const uint8_t *bytes)
{
    DashboardWidget widget;
    widget.kind = static_cast<DashboardWidgetKind>(bytes[0]);
    widget.style = static_cast<DashboardWidgetStyle>(bytes[1]);
    widget.width = bytes[2]; widget.height = bytes[3];
    widget.source = bytes[4]; widget.secondSource = bytes[5];
    return sanitizedDashboardWidget(cell,widget);
}
inline DashboardLayout defaultDashboardLayout()
{
    DashboardLayout layout;
    layout.cells[0].kind = DashboardWidgetKind::Timer;
    layout.cells[0].width = 2;
    layout.cells[2].kind = DashboardWidgetKind::Power;
    layout.cells[2].style = DashboardWidgetStyle::Numbers;
    layout.cells[3].kind = DashboardWidgetKind::Channels;
    layout.cells[3].style = DashboardWidgetStyle::Bars;
    return layout;
}
inline void placeDashboardWidget(DashboardLayout &layout, uint8_t cell, DashboardWidget widget)
{
    if (cell >= 4) return;
    widget = sanitizedDashboardWidget(cell,widget);
    uint8_t mask = dashboardWidgetMask(cell,widget);
    for (uint8_t i=0;i<4;++i)
        if (i==cell || (dashboardWidgetMask(i,layout.cells[i]) & mask)) layout.cells[i] = DashboardWidget{};
    layout.cells[cell] = widget;
}
inline void sanitizeDashboardLayout(DashboardLayout &layout)
{
    uint8_t occupied=0;
    for(uint8_t i=0;i<4;++i){
        layout.cells[i]=sanitizedDashboardWidget(i,layout.cells[i]);
        uint8_t mask=dashboardWidgetMask(i,layout.cells[i]);
        if(mask & occupied)layout.cells[i]=DashboardWidget{};
        else occupied |= mask;
    }
}
