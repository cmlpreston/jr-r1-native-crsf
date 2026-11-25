        bootPinUnlocked_=true;pinPurpose_=PinPurpose::None;moduleLinkManaged_=false;
        const auto cutHandler=safetyCutHandler_;safetyCutHandler_=auditConfigurationCut;
        const auto original=dashboardLayouts_[activeModelSlot_];
        const auto cuts=auditCutRequests;
        CrsfTelemetryState t;
        t.receiverVoltageValid=true;t.receiverVoltageDecivolts=62;
        t.vehicleCellsValid=true;t.vehiclePackMv=15400;t.vehicleMinimumCellMv=3800;
        t.ersBatteryValid=true;t.ersRemainingValid=true;
        t.ersVoltageDeciV=164;t.ersCurrentDeciA=123;t.ersCapacityMah=1234;t.ersRemainingPercent=75;
        auto fresh=[&](){t.ersBatteryUpdatedMs=millis();designTelemetry_=t;updateDashboardWidgets(t,99,true);};
        auto values=[&](uint8_t cell,const char *voltage,const char *current,const char *used,const char *remaining){
            const char *expected[]={voltage,current,used,remaining};
            for(uint8_t n=0;n<4;++n)assert(!strcmp(lv_label_get_text(widgetViews_[cell].values[n]),expected[n]));
        };
        auto fit=[&](uint8_t cell){
            auto panel=widgetViews_[cell].panel;lv_obj_update_layout(panel);lv_refr_now(nullptr);
            lv_area_t bounds;lv_obj_get_coords(panel,&bounds);
            std::vector<lv_area_t> labels;
            for(uint32_t n=0;n<lv_obj_get_child_cnt(panel);++n){
                auto child=lv_obj_get_child(panel,n);
                if(!lv_obj_check_type(child,&lv_label_class)||lv_obj_has_flag(child,LV_OBJ_FLAG_HIDDEN))continue;
                lv_area_t area;lv_obj_get_coords(child,&area);
                assert(area.x1>=bounds.x1&&area.y1>=bounds.y1&&area.x2<=bounds.x2&&area.y2<=bounds.y2);
                const auto font=lv_obj_get_style_text_font(child,0);
                assert(font->line_height<=lv_obj_get_height(child));
                const char *text=lv_label_get_text(child);
                assert(lv_txt_get_width(text,strlen(text),font,0,LV_TEXT_FLAG_NONE)<=lv_obj_get_width(child));
                for(const auto &other:labels)
                    assert(area.x2<other.x1||area.x1>other.x2||area.y2<other.y1||area.y1>other.y2);
                labels.push_back(area);
            }
            auditTypography(panel);
            assert(armRequested_&&!safetyResetRequested_&&auditCutRequests==cuts);
        };
        auto shot=[&](const char *name){
            if(const char *prefix=getenv("JR_R1_DRIVE_PACK_WIDGET_IMAGES")){
                char path[256];snprintf(path,sizeof(path),"%s-%s-%s.bmp",prefix,portraitUi?"portrait":"landscape",name);
                simulatorSaveScreenshot(path);
            }
        };
        for(unsigned orientation=0;orientation<2;++orientation){
            for(uint8_t width:{1,2})for(uint8_t height:{1,2}){
                dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
                DashboardWidget pack;pack.kind=DashboardWidgetKind::DrivePack;pack.width=width;pack.height=height;
                placeDashboardWidget(dashboardLayouts_[activeModelSlot_],0,pack);
                armRequested_=true;safetyResetRequested_=false;designSafety_=OutputSafetyStatus::Armed;
                t.ersRemainingPercent=75;t.ersCapacityMah=1234;t.ersBatteryValid=t.ersRemainingValid=true;
                t.ersBatteryUpdatedMs=millis();designTelemetry_=t;drawDashboard();fresh();
                values(0,"16.4 V","12.3 A","1234 mAh","75%");fit(0);
                assert(!strcmp(lv_label_get_text(widgetViews_[0].title),"Drive pack / CU01"));
                if(width==1&&height==1){
                    const auto rendered=uiMetrics.rendered;
                    for(unsigned n=0;n<20;++n){fresh();lv_refr_now(nullptr);}
                    assert(uiMetrics.rendered==rendered); // Identical telemetry must not redraw the card.
                }
                char name[24];snprintf(name,sizeof(name),"%ux%u",width,height);shot(name);
                for(uint8_t remaining:{0,100,255}){
                    t.ersRemainingPercent=remaining;fresh();
                    values(0,"16.4 V","12.3 A","1234 mAh",remaining==0?"0%":remaining==100?"100%":"--");fit(0);
                }
                t.ersRemainingValid=false;t.ersRemainingPercent=75;fresh();
                values(0,"16.4 V","12.3 A","1234 mAh","--");
                t.ersRemainingValid=true;t.ersCapacityMah=0x00ffffff;fresh();
                values(0,"16.4 V","12.3 A","16777215 mAh","75%");fit(0);
                // Staleness is independent of the telemetry parser's validity expiry.
                t.ersBatteryUpdatedMs=millis()-3001;updateDashboardWidgets(t,99,true);
                values(0,"--","--","--","--");fit(0);
                t.ersBatteryValid=false;fresh();values(0,"--","--","--","--");
                t.ersBatteryValid=true;t.ersCapacityMah=1234;fresh();fit(0);
                // A tap adds no read/reset/write action; a normal widget hold maximizes.
                const auto saved=dashboardLayouts_[activeModelSlot_];
                controlDesign(130,activate);assert(activePage_==UiPage::Dashboard);
                auto panel=widgetViews_[0].panel;lv_area_t area;lv_obj_get_coords(panel,&area);
                const int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;
                assert(handleDashboardWidgetHold(x,y,x,y,700));assert(maximizedWidget_==0);
                fresh();values(0,"16.4 V","12.3 A","1234 mAh","75%");fit(0);shot("maximized");
                assert(!memcmp(&saved,&dashboardLayouts_[activeModelSlot_],sizeof(saved)));
                controlDesign(243,activate);assert(maximizedWidget_==-1);fresh();fit(0);
                assert(!memcmp(&saved,&dashboardLayouts_[activeModelSlot_],sizeof(saved)));
            }
            // All four origins retain normal maximize behaviour with this appended kind.
            for(uint8_t cell=0;cell<4;++cell){
                dashboardLayouts_[activeModelSlot_]=DashboardLayout{};
                DashboardWidget pack;pack.kind=DashboardWidgetKind::DrivePack;
                placeDashboardWidget(dashboardLayouts_[activeModelSlot_],cell,pack);
                drawDashboard();fresh();fit(cell);
                lv_area_t area;lv_obj_get_coords(widgetViews_[cell].panel,&area);
                const int x=(area.x1+area.x2)/2,y=(area.y1+area.y2)/2;
                assert(handleDashboardWidgetHold(x,y,x,y,700)&&maximizedWidget_==cell);
                fresh();fit(cell);controlDesign(243,activate);assert(maximizedWidget_==-1);
            }
            // Picker cycling selects the stable appended kind; Cancel preserves the saved layout.
            const auto beforePicker=dashboardLayouts_[activeModelSlot_];
            drawDesignPage(UiPage::WidgetSelect);controlDesign(110,activate);
            widgetDraft_.kind=DashboardWidgetKind::RfMargin;controlDesign(120,activate);
            assert(widgetDraft_.kind==DashboardWidgetKind::DrivePack);
            assert(!strcmp(dashboardKindName(widgetDraft_.kind),"Drive pack"));
            controlDesign(125,activate);
            assert(!memcmp(&beforePicker,&dashboardLayouts_[activeModelSlot_],sizeof(beforePicker)));
            assert(armRequested_&&auditCutRequests==cuts);
            armRequested_=false;assert(requestDisplayRotation());applyDisplayRotation();armRequested_=true;
        }
        // Persist/reload the optional widget without changing any other model or slot.
        armRequested_=false;dashboardLayouts_[activeModelSlot_]=original;
        const auto other=dashboardLayouts_[(activeModelSlot_+1)%kModelMemoryCount];
        DashboardWidget pack;pack.kind=DashboardWidgetKind::DrivePack;
        placeDashboardWidget(dashboardLayouts_[activeModelSlot_],3,pack);
        const auto saved=dashboardLayouts_[activeModelSlot_];saveModelMemorySettings();
        dashboardLayouts_[activeModelSlot_]=DashboardLayout{};loadModelMemorySettings();
        assert(!memcmp(&saved,&dashboardLayouts_[activeModelSlot_],sizeof(saved)));
        assert(!memcmp(&other,&dashboardLayouts_[(activeModelSlot_+1)%kModelMemoryCount],sizeof(other)));
        assert(!armRequested_&&auditCutRequests==cuts);
        safetyCutHandler_=cutHandler;
        Serial.println("Drive pack widget: CU01-only V/A/mAh/estimated remaining, stale/unknown guards, spans/origins/maximize, fitting typography, persistence and arm invariants; both orientations PASS");
        return true;
