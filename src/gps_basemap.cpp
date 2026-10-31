#include "gps_basemap.h"
#include "ui_typography.h"
#include "offline_map.h"
#include <algorithm>

namespace {
struct MapVertex { int16_t east, north; };
struct MapTriangle { uint16_t a, b, c; uint8_t kind; };
struct MapPath { uint16_t offset, count; uint8_t kind; };
#include "highfield_map_data.inc"

void drawMap(lv_event_t *event) {
    auto *object = lv_event_get_target(event);
    const auto &view = *static_cast<GpsMapViewport *>(lv_event_get_user_data(event));
    auto *ctx = lv_event_get_draw_ctx(event);
    lv_area_t bounds; lv_obj_get_coords(object, &bounds);
    const int width = lv_area_get_width(&bounds), height = lv_area_get_height(&bounds);
    const double originEast = GpsMap::east(GpsMap::OriginLat, view.longitude, GpsMap::OriginLon);
    const double originNorth = GpsMap::north(view.latitude, GpsMap::OriginLat);
    const auto screenPoint = [&](MapVertex vertex) {
        return lv_point_t{
            static_cast<lv_coord_t>(std::max(-16000.0, std::min(16000.0, bounds.x1 + width/2.0 + (vertex.east-originEast)*view.pixelsPerMetre))),
            static_cast<lv_coord_t>(std::max(-16000.0, std::min(16000.0, bounds.y1 + height/2.0 - (vertex.north-originNorth)*view.pixelsPerMetre)))};
    };
    // The regular LVGL draw buffer clips these vectors to this map's viewport.
    const lv_area_t *previousClip = ctx->clip_area;
    lv_area_t clip;
    if (!_lv_area_intersect(&clip, previousClip, &bounds)) return;
    ctx->clip_area = &clip;
    lv_draw_line_dsc_t line; lv_draw_line_dsc_init(&line);
    lv_draw_rect_dsc_t fill; lv_draw_rect_dsc_init(&fill);
    fill.border_width = 0; fill.radius = 0;
    const uint32_t colours[] = {0xc5d9b7, 0xaac99b, 0xd4cbbd, 0xe4d8b5};
    const bool custom=OfflineMap::covers(view.latitude,view.longitude);
    if(custom && OfflineMap::pixels()){
        const auto &map=OfflineMap::current();
        // Sample only the current LVGL dirty rectangle. One scanline uses
        // <=960 bytes of internal RAM; the full map stays in PSRAM.
        lv_color_t row[480];
        const int span=clip.x2-clip.x1+1;
        lv_img_dsc_t image{};image.header.cf=LV_IMG_CF_TRUE_COLOR;image.header.w=span;image.header.h=1;
        image.data_size=span*sizeof(lv_color_t);image.data=reinterpret_cast<const uint8_t*>(row);
        lv_draw_img_dsc_t draw;lv_draw_img_dsc_init(&draw);
        const double metresLon=GpsMap::MetresPerE7*GpsMap::longitudeScale(view.latitude);
        if(span<=480&&view.pixelsPerMetre>0)for(int y=clip.y1;y<=clip.y2;++y){
            const double lat=view.latitude-(y-(bounds.y1+height/2.0))/view.pixelsPerMetre/GpsMap::MetresPerE7;
            int sy=int(std::floor((map.north-lat)/(double(map.north)-map.south)*map.height));
            for(int x=0;x<span;++x){
                double lon=view.longitude+(clip.x1+x-(bounds.x1+width/2.0))/view.pixelsPerMetre/metresLon;
                int sx=int(std::floor((lon-map.west)/(double(map.east)-map.west)*map.width));
                row[x]=lv_color_hex(0xeeeae0);
                if(sx>=0&&sx<map.width&&sy>=0&&sy<map.height)row[x].full=OfflineMap::pixels()[sy*map.width+sx];
            }
            lv_area_t area={clip.x1,static_cast<lv_coord_t>(y),clip.x2,static_cast<lv_coord_t>(y)};
            lv_draw_img(ctx,&draw,&area,&image);
        }
        lv_img_cache_invalidate_src(&image);
    }else if(custom){
        const auto &map=OfflineMap::current();line.color=lv_color_hex(0x00717c);line.width=2;
        auto point=[&](const OfflineMap::Point &p){return lv_point_t{
            static_cast<lv_coord_t>(std::max(-16000.0,std::min(16000.0,bounds.x1+width/2.0+GpsMap::east(view.latitude,p.lon,view.longitude)*view.pixelsPerMetre))),
            static_cast<lv_coord_t>(std::max(-16000.0,std::min(16000.0,bounds.y1+height/2.0-GpsMap::north(p.lat,view.latitude)*view.pixelsPerMetre)))};};
        for(unsigned i=0;i<map.pathCount;++i){const auto &path=map.paths[i];for(unsigned j=1;j<path.count;++j){auto a=point(map.points[path.offset+j-1]),b=point(map.points[path.offset+j]);lv_draw_line(ctx,&line,&a,&b);}}
    } else if (GpsMap::covers(view.latitude, view.longitude)) {
        for (const auto &triangle : mapTriangles) {
            const lv_point_t p[] = {screenPoint(mapVertices[triangle.a]),screenPoint(mapVertices[triangle.b]),screenPoint(mapVertices[triangle.c])};
            if (std::max({p[0].x,p[1].x,p[2].x})<clip.x1 || std::min({p[0].x,p[1].x,p[2].x})>clip.x2 ||
                std::max({p[0].y,p[1].y,p[2].y})<clip.y1 || std::min({p[0].y,p[1].y,p[2].y})>clip.y2) continue;
            fill.bg_color = lv_color_hex(colours[triangle.kind]);
            lv_draw_triangle(ctx, &fill, p);
        }
        for (const auto &path : mapPaths) {
            if (path.kind==0 || path.kind==2 || path.kind==3) continue;
            const int passes = path.kind==5 ? 2 : 1;
            for (int pass=0; pass<passes; ++pass) {
                line.color = lv_color_hex(path.kind==1 ? 0xe5efd9 : path.kind==4 ? 0xf7f1db : pass ? 0xffffff : 0xc6c4bb);
                line.width = path.kind==1 ? 1 : path.kind==4 ? 2 : std::max(2,int(7*view.pixelsPerMetre))+(pass ? 0 : 2);
                line.round_start = line.round_end = 1;
                for (unsigned i=1; i<path.count+(path.kind==1 ? 1U : 0U); ++i) {
                    auto a=screenPoint(mapVertices[path.offset+i-1]);
                    auto b=screenPoint(mapVertices[path.offset+i%path.count]);
                    lv_draw_line(ctx,&line,&a,&b);
                }
            }
        }
        lv_draw_label_dsc_t text; lv_draw_label_dsc_init(&text);
        text.font=UiText::font(UiText::Caption); text.color=lv_color_hex(0x405442);
        const auto label = [&](int east, int north, const char *caption, int w) {
            auto p=screenPoint({static_cast<int16_t>(east),static_cast<int16_t>(north)});
            lv_area_t area={p.x,p.y,static_cast<lv_coord_t>(p.x+w),static_cast<lv_coord_t>(p.y+24)};
            lv_draw_label(ctx,&text,&area,caption,nullptr);
        };
        label(-3,22,"Highfield Park",95);
        label(-115,150,"Highfield Rd",85);
        label(55,-5,"Pavilion",60);
    } else {
        line.color=lv_color_hex(0xc8d1d2); line.width=1;
        for (int x=0;x<width;x+=40) {lv_point_t a={static_cast<lv_coord_t>(bounds.x1+x),bounds.y1}, b={a.x,bounds.y2};lv_draw_line(ctx,&line,&a,&b);}
        for (int y=0;y<height;y+=40) {lv_point_t a={bounds.x1,static_cast<lv_coord_t>(bounds.y1+y)}, b={bounds.x2,a.y};lv_draw_line(ctx,&line,&a,&b);}
    }
    const char *attribution=custom?OfflineMap::current().attribution:GpsMap::covers(view.latitude,view.longitude)?"(c) OpenStreetMap":"No offline map for this location";
    lv_draw_label_dsc_t credit;lv_draw_label_dsc_init(&credit);credit.font=UiText::font(UiText::Caption);credit.color=lv_color_hex(0x405442);
    lv_area_t creditArea={static_cast<lv_coord_t>(bounds.x1+5),static_cast<lv_coord_t>(bounds.y2-17),static_cast<lv_coord_t>(bounds.x2-5),bounds.y2};
    fill.bg_color=lv_color_hex(0xeeeae0);fill.bg_opa=LV_OPA_90;lv_draw_rect(ctx,&fill,&creditArea);
    lv_draw_label(ctx,&credit,&creditArea,attribution,nullptr);
    ctx->clip_area = previousClip;
}
}

lv_obj_t *createGpsBasemap(lv_obj_t *parent, int x, int y, int width, int height,
                          GpsMapViewport &viewport) {
    auto *map=lv_obj_create(parent);lv_obj_remove_style_all(map);
    lv_obj_set_pos(map,x,y);lv_obj_set_size(map,width,height);
    lv_obj_set_style_bg_color(map,lv_color_hex(0xeeeae0),0);
    lv_obj_set_style_bg_opa(map,LV_OPA_COVER,0);
    lv_obj_clear_flag(map,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(map,drawMap,LV_EVENT_DRAW_MAIN,&viewport);
    return map;
}
void updateGpsBasemap(lv_obj_t *object, GpsMapViewport &view,
                      int32_t lat, int32_t lon, double scale) {
    if (!object || (view.latitude==lat && view.longitude==lon && view.pixelsPerMetre==scale && view.mapRevision==OfflineMap::revision())) return;
    view.latitude=lat;view.longitude=lon;view.pixelsPerMetre=scale;
    view.mapRevision=OfflineMap::revision();
    lv_obj_invalidate(object);
}

double gpsBasemapPreviewScale(lv_obj_t *object, bool fillViewport) {
    if(!object)return 0.5;
    const auto &map=OfflineMap::current();
    if(!map.valid)return 0.5;
    lv_obj_update_layout(object);
    const int32_t lat=map.south+(int64_t(map.north)-map.south)/2;
    const double width=std::max(1.0,GpsMap::east(lat,map.east,map.west));
    const double height=std::max(1.0,GpsMap::north(map.north,map.south));
    return std::max(0.001,fillViewport
        ?std::max(lv_obj_get_width(object)/width,lv_obj_get_height(object)/height)
        :std::min((lv_obj_get_width(object)-12)/width,(lv_obj_get_height(object)-20)/height));
}
void previewGpsBasemap(lv_obj_t *object, GpsMapViewport &view, bool fillViewport, double zoom) {
    if(!object)return;
    const auto &map=OfflineMap::current();
    const int32_t lat=map.valid?map.south+(int64_t(map.north)-map.south)/2:GpsMap::OriginLat;
    const int32_t lon=map.valid?map.west+(int64_t(map.east)-map.west)/2:GpsMap::OriginLon;
    updateGpsBasemap(object,view,lat,lon,gpsBasemapPreviewScale(object,fillViewport)*zoom);
}
