// Included inside startWeb(), alongside the authenticated setup endpoints.
server.on("/api/ers",HTTP_GET,[]{
    if(!authorized())return;
    DynamicJsonDocument doc(4096);
    const auto &config=handset->portalErs();
    doc["standardBatteryVehicle"]=handset->portalStandardBatteryVehicle();
    doc["status"]=config.status();doc["busy"]=config.busy();
    JsonArray devices=doc.createNestedArray("devices");
    for(uint8_t i=0;i<config.count();++i){
        const auto &d=config.device(i);auto o=devices.createNestedObject();
        o["type"]=d.type;o["id"]=d.id;o["name"]=d.name;o["known"]=d.known;
        o["gpsMode"]=d.gpsMode;o["motorPoles"]=d.motorPoles;
        o["capacityMah"]=d.capacityMah;o["telemetryId"]=d.telemetryId;
    }
    String out;serializeJson(doc,out);server.send(200,"application/json",out);
});
server.on("/api/ers",HTTP_POST,[]{
    if(!authorized(true))return;
    unsigned values[4];const char *keys[]={"type","id","command","value"};
    for(unsigned i=0;i<4;++i){
        String s=server.arg(keys[i]);if(s.length()==0 || s.length()>5){server.send(400,"text/plain","Invalid sensor command");return;}
        unsigned v=0;for(unsigned j=0;j<s.length();++j){if(s[j]<'0'||s[j]>'9'){server.send(400,"text/plain","Invalid sensor command");return;}v=v*10+s[j]-'0';}
        if(v>(i==3?65535u:255u)){server.send(400,"text/plain","Invalid sensor command");return;}values[i]=v;
    }
    ErsRequest r;r.type=values[0];r.id=values[1];r.command=values[2];r.value=values[3];
    if(!validErsRequest(r)){server.send(400,"text/plain","Unsupported sensor command or value");return;}
    const bool ok=handset->portalErsRequest(r);
    server.send(ok?202:409,"text/plain",ok?"Queued; awaiting sensor acknowledgement":"Use CUT with module link enabled; discover sensor first and wait for any pending operation");
});

server.on("/api/ers-source",HTTP_POST,[]{
    if(!authorized(true))return;
    const String v=server.arg("vehicle");if(v!="0" && v!="1"){server.send(400,"text/plain","Choose receiver or vehicle");return;}
    const bool ok=handset->portalSetStandardBatteryVehicle(v=="1");
    server.send(ok?200:409,"text/plain",ok?"Battery source saved":"Use CUT; battery source could not be saved");
});
