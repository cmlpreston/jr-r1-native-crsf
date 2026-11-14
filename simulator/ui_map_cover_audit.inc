// Included inside runSimulatorUiAudit. Exercise the actual widget view and LVGL
// raster renderer; the companion host test checks every sampled row/column.
bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;armRequested_=false;
OfflineMap::Package coverMap{};coverMap.valid=true;coverMap.width=coverMap.height=64;
coverMap.south=GpsMap::DemoLat-2000;coverMap.north=GpsMap::DemoLat+2000;
coverMap.west=GpsMap::DemoLon-4000;coverMap.east=GpsMap::DemoLon+4000;
strcpy(coverMap.attribution,"Synthetic map coverage test");
auto *coverPixels=OfflineMap::allocatePixels(64*64);assert(coverPixels);
for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)
    coverPixels[y*64+x]=((x/8+y/8)%2)?0x4dd4:0x92a7;
OfflineMap::install(coverMap,coverPixels);
auto *coverDisplay=lv_disp_get_default();const auto oldCoverMonitor=coverDisplay->driver->monitor_cb;
coverDisplay->driver->monitor_cb=auditRefreshMonitor;
for(unsigned orientation=0;orientation<2;++orientation){
    for(unsigned span=0;span<4;++span)for(bool maximized:{false,true}){
        dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
        DashboardWidget coverWidget;coverWidget.kind=DashboardWidgetKind::Map;
        coverWidget.width=(span&1)?2:1;coverWidget.height=(span&2)?2:1;
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,coverWidget);
        maximizedWidget_=maximized?0:-1;gpsCount_=0;++gpsTrailRevision_;
        drawDashboard();lv_obj_update_layout(lv_scr_act());
        auto &coverView=widgetViews_[0];auto map=coverView.basemap;
        const int width=lv_obj_get_width(map),height=lv_obj_get_height(map);
        assert(coverView.mapViewport.fillSavedRaster);
        CrsfTelemetryState coverTelemetry{};
        auto render=[&](){
            coverTelemetry.gpsValid=coverTelemetry.gpsSpeedValid=gpsCount_>0;
            coverTelemetry.gpsSatellitesValid=true;coverTelemetry.satellites=8;
            if(gpsCount_){coverTelemetry.latitudeE7=gpsLat_[gpsCount_-1];coverTelemetry.longitudeE7=gpsLon_[gpsCount_-1];}
            coverTelemetry.gpsUpdatedMs=coverTelemetry.gpsSpeedUpdatedMs=coverTelemetry.gpsSatellitesUpdatedMs=gpsLastSampleMs_=millis();
            updateDashboardWidgets(coverTelemetry,99,true);lv_obj_update_layout(lv_scr_act());lv_refr_now(nullptr);
        };
        auto covered=[&](){
            const auto &v=coverView.mapViewport;
            const double metresLon=GpsMap::MetresPerE7*GpsMap::longitudeScale(v.latitude);
            assert(v.longitude-width/2.0/v.pixelsPerMetre/metresLon>=coverMap.west);
            assert(v.longitude+(width-1-width/2.0)/v.pixelsPerMetre/metresLon<coverMap.east);
            assert(v.latitude+height/2.0/v.pixelsPerMetre/GpsMap::MetresPerE7<=coverMap.north);
            assert(v.latitude-(height-1-height/2.0)/v.pixelsPerMetre/GpsMap::MetresPerE7>coverMap.south);
        };
        auto shot=[&](const char *phase){if(const char *prefix=getenv("JR_R1_MAP_COVER_SCREENSHOTS")){
            char path[256];snprintf(path,sizeof(path),"%s-%s-%u-%u-%s.bmp",prefix,portraitUi?"portrait":"landscape",span,unsigned(maximized),phase);
            simulatorSaveScreenshot(path);lv_area_t bounds;lv_obj_get_coords(map,&bounds);
            Serial.printf("Map cover pixels: %s x=%d y=%d width=%d height=%d\n",path,bounds.x1,bounds.y1,width,height);
        }};
        render();covered();assert(lv_obj_has_flag(coverView.marker,LV_OBJ_FLAG_HIDDEN));shot("no-fix");
        gpsCount_=2;gpsLat_[0]=GpsMap::DemoLat;gpsLon_[0]=GpsMap::DemoLon;
        gpsLat_[1]=GpsMap::DemoLat+50;gpsLon_[1]=GpsMap::DemoLon+50;++gpsTrailRevision_;
        render();covered();assert(!lv_obj_has_flag(coverView.marker,LV_OBJ_FLAG_HIDDEN));shot("fix");
        auto aligned=[&](){
            const auto point=gpsViewportPoint(coverView.mapViewport,gpsLat_[1],gpsLon_[1],width,height);
            assert(lv_obj_get_x(coverView.marker)+8==point.x&&lv_obj_get_y(coverView.marker)+8==point.y);
            assert(coverView.points[1].x+lv_obj_get_x(coverView.plot)==point.x);
            assert(coverView.points[1].y+lv_obj_get_y(coverView.plot)==point.y);
        };
        aligned();render();auditRedrawnPixels=0;
        for(unsigned n=0;n<10;++n)render();assert(auditRedrawnPixels==0);
        gpsLat_[1]=coverMap.north-1;gpsLon_[1]=coverMap.west+1;++gpsTrailRevision_;
        render();covered();aligned();shot("edge");
        // Repeated edge fixes must not keep panning the image against its limit.
        render();auditRedrawnPixels=0;for(unsigned n=0;n<10;++n)render();assert(auditRedrawnPixels==0);
        assert(!armRequested_);
    }
    maximizedWidget_=-1;gpsCount_=1;gpsLat_[0]=GpsMap::DemoLat;gpsLon_[0]=GpsMap::DemoLon;++gpsTrailRevision_;
    gpsZoom_=-3;drawDesignPage(UiPage::Gps);lv_obj_update_layout(lv_scr_act());
    CrsfTelemetryState fullGps{};fullGps.gpsValid=fullGps.gpsSpeedValid=true;
    fullGps.latitudeE7=gpsLat_[0];fullGps.longitudeE7=gpsLon_[0];
    fullGps.gpsUpdatedMs=fullGps.gpsSpeedUpdatedMs=gpsLastSampleMs_=millis();
    updateDesign(fullGps,OutputSafetyStatus::Cut,99,true);lv_refr_now(nullptr);
    assert(!designMapViewport_.fillSavedRaster);
    auto fullMap=lv_obj_get_parent(designCurve_);
    const double fullScale=gpsBasemapPreviewScale(fullMap,true)*std::pow(2.0,gpsZoom_);
    assert(designMapViewport_.pixelsPerMetre==fullScale);
    if(const char *prefix=getenv("JR_R1_MAP_COVER_SCREENSHOTS")){
        char path[256];snprintf(path,sizeof(path),"%s-%s-full-page.bmp",prefix,portraitUi?"portrait":"landscape");simulatorSaveScreenshot(path);
    }
    assert(requestDisplayRotation());applyDisplayRotation();
}
coverDisplay->driver->monitor_cb=oldCoverMonitor;OfflineMap::install(OfflineMap::Package{});
Serial.println("GPS widget cover: real raster fill without fix/with fix/at edges, all sizes/maximized, aligned marker/trail, unchanged idle pixels and independent full-page zoom; both orientations PASS");
return true;
