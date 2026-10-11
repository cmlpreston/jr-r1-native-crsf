#pragma once
#include <stdint.h>

// Persisted UI-only data. Each model has a two-column, two-row dashboard.
// Fields are bytes (no ABI padding); version is encoded in the NVS key.
enum class DashboardWidgetKind : uint8_t { Empty, Timer, Power, Channels, Sensor, Map, Model, AllChannels, Count };
enum class DashboardWidgetStyle : uint8_t { Numbers, Bars, Dials, Graph, Switches, VerticalBars, Count };
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
    if (kind == DashboardWidgetKind::AllChannels) return style == DashboardWidgetStyle::Bars || style == DashboardWidgetStyle::VerticalBars;
    if (kind == DashboardWidgetKind::Channels) return style <= DashboardWidgetStyle::Switches;
    if (kind == DashboardWidgetKind::Sensor) return style == DashboardWidgetStyle::Graph || style == DashboardWidgetStyle::Dials;
    return false;
}
inline DashboardWidget sanitizedDashboardWidget(uint8_t cell, DashboardWidget widget)
{
    if (widget.speedValue >= SpeedWidgetValue::Count) widget.speedValue = SpeedWidgetValue::Current;
    if (widget.kind >= DashboardWidgetKind::Count) widget = DashboardWidget{};
    if (!dashboardStyleAllowed(widget.kind, widget.style)) widget.style = DashboardWidgetStyle::Numbers;
    widget.width = widget.width == 2 && cell%2==0 ? 2 : 1;
    widget.height = widget.height == 2 && cell/2==0 ? 2 : 1;
    if(widget.kind==DashboardWidgetKind::AllChannels && widget.source>2)widget.source=0;
    if (widget.source > 15) widget.source = 0;
    if (widget.secondSource > 16) widget.secondSource = 1;
    if (widget.kind == DashboardWidgetKind::Sensor && widget.source > 5) widget.source = 0;
    if (widget.kind == DashboardWidgetKind::Sensor && widget.source != 0 && widget.style == DashboardWidgetStyle::Dials) widget.style = DashboardWidgetStyle::Numbers;
    return widget;
}
// Enumerate only spans that fit from this anchor; dimensions remain persisted bytes.
inline void cycleDashboardWidgetSize(uint8_t cell, DashboardWidget &widget, int direction)
{
    const uint8_t widths[]={1,2,1,2},heights[]={1,1,2,2};
    uint8_t allowed[4],count=0,current=0;
    for(uint8_t i=0;i<4;++i)if(cell<4&&cell%2+widths[i]<=2&&cell/2+heights[i]<=2){
        if(widget.width==widths[i]&&widget.height==heights[i])current=count;
        allowed[count++]=i;
    }
    if(!count)return;
    const uint8_t next=allowed[(current+(direction<0?count-1:1))%count];
    widget.width=widths[next];widget.height=heights[next];
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
// Earlier anchors cover later ones. Hidden widgets keep their complete settings.
inline uint8_t visibleDashboardWidgets(const DashboardLayout &layout)
{
    uint8_t occupied=0,visible=0;
    for(uint8_t i=0;i<4;++i){
        const uint8_t mask=dashboardWidgetMask(i,layout.cells[i]);
        if(mask && !(mask&occupied)){visible|=1U<<i;occupied|=mask;}
    }
    return visible;
}
inline void placeDashboardWidget(DashboardLayout &layout, uint8_t cell, DashboardWidget widget)
{
    if(cell<4)layout.cells[cell]=sanitizedDashboardWidget(cell,widget);
}
inline void sanitizeDashboardLayout(DashboardLayout &layout)
{
    for(uint8_t i=0;i<4;++i)layout.cells[i]=sanitizedDashboardWidget(i,layout.cells[i]);
}

// Compact monitor capacity: preserve readable labels in every widget size.
inline int dashboardVerticalColumns(int width) { return (width-16)/56>0?(width-16)/56:1; }
inline uint8_t dashboardChannelCapacity(int width,int height,DashboardWidgetStyle style=DashboardWidgetStyle::Bars) {
    if(style==DashboardWidgetStyle::VerticalBars){
        const int rows=(height-42)/72>0?(height-42)/72:1;
        const int capacity=dashboardVerticalColumns(width)*rows;
        return capacity>16?16:capacity;
    }
    const int columns=width>=220?2:1;
    const int rows=(height-42)/28;
    const int capacity=(rows>0?rows:1)*columns;
    return capacity>16?16:capacity;
}
inline uint8_t dashboardConfiguredChannels(uint16_t mask,uint8_t *channels) {
    uint8_t count=0;
    for(uint8_t ch=0;ch<16;++ch)if(mask&(1U<<ch))channels[count++]=ch;
    return count;
}

// All Channels source: 0 outputs, 1 physical inputs, 2 both. Hardware IDs
// have bit 7 set; DISPLAY_SW is deliberately absent (power module, not input).
constexpr uint8_t DashboardMonitorMaxEntries = 21;
inline uint32_t dashboardMonitorBit(uint8_t entry){
    if(!(entry&0x80))return entry<16?1UL<<entry:0;
    const uint8_t ids[]={3,9,2,4,5};
    for(uint8_t i=0;i<5;++i)if(entry==(0x80|ids[i]))return 1UL<<(16+i);
    return 0;
}
inline uint8_t dashboardMonitorEntries(uint16_t channels,uint8_t mode,uint8_t *entries,uint32_t hidden=0){
    uint8_t n=mode==1?0:dashboardConfiguredChannels(channels,entries);
    if(mode==1||mode==2){entries[n++]=0x80|3;entries[n++]=0x80|9;entries[n++]=0x80|2;entries[n++]=0x80|4;entries[n++]=0x80|5;}
    uint8_t visible=0;for(uint8_t i=0;i<n;++i)if(!(hidden&dashboardMonitorBit(entries[i])))entries[visible++]=entries[i];
    return visible;
}
