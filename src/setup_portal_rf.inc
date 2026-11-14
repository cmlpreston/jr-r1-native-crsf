// Reuses the single UI-owned ELRS parameter client and its readback checks.
server.on("/api/rf-config",HTTP_GET,[]{
    if(!authorized())return;
    int id=0;if(server.hasArg("id")&&!number("id",1,CrsfTxConfig::kMaxParameters,id)){server.send(400,"text/plain","Invalid parameter");return;}
    const auto &c=handset->portalRfConfig();DynamicJsonDocument d(12288);
    d["active"]=handset->portalRfConfigActive();d["address"]=c.address();d["device"]=c.device();
    d["ready"]=c.ready();d["confirming"]=c.confirming();d["status"]=c.status();d["count"]=c.count();d["revision"]=c.revision();
    auto a=d.createNestedArray("parameters");
    for(uint8_t i=1;i<=c.count();++i){
        if(id&&i!=id)continue;const auto p=c.parameter(i);if(!p)continue;
        auto o=a.createNestedObject();o["id"]=i;o["parent"]=p->parent;o["type"]=p->type;o["hidden"]=p->hidden;
        o["name"]=p->name;o["value"]=p->value;o["min"]=p->minimum;o["max"]=p->maximum;
        if(id){o["options"]=p->options;o["info"]=p->info;}
    }
    if(d.overflowed()){server.send(503,"text/plain","Parameter snapshot too large");return;}
    String out;serializeJson(d,out);server.send(200,"application/json",out);
});
server.on("/api/rf-config",HTTP_POST,[]{
    if(!authorized(true))return;
    if(server.arg("action")=="discover"){
        int address=0;if(!number("address",0xec,0xee,address)||(address!=0xec&&address!=0xee)){server.send(400,"text/plain","Choose receiver or TX module");return;}
        bool ok=handset->portalRfConfigStart(address);server.send(ok?202:409,"text/plain",ok?"Discovering device; outputs CUT":"Unlock handset, use CUT and finish the current operation first");return;
    }
    const String action=server.arg("action");int id=0,value=0,revision=0;
    if((action!="apply"&&action!="command")||!number("id",1,CrsfTxConfig::kMaxParameters,id)||!number("value",0,255,value)||!number("revision",0,INT32_MAX,revision)){server.send(400,"text/plain","Invalid parameter operation");return;}
    bool ok=handset->portalRfConfigAction(id,value,action=="command",revision);
    server.send(ok?202:409,"text/plain",ok?"Queued; awaiting device readback":"Refresh settings; use the active unlocked configuration page with outputs CUT");
});
