#include "setup_portal.h"
#include "offline_map.h"
#include "transmitter_ui.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <atomic>
#include <new>
#include "remote_screen.h"
#include "browser_session.h"
#include "wifi_retry.h"
#include "network_hostname.h"
#include "display_profile.h"
#include <Preferences.h>
#ifndef JR_R1_SIMULATOR
#include <esp_system.h>
#include "memory_diagnostics.h"
#endif

namespace SetupPortal {
static State info;
static char browserSession[33]={};
static bool updateInProgress=false;
bool firmwareUpdating(){return updateInProgress;}
static bool restartPending=false;
static uint32_t restartAt=0;
bool requestRestart(){
    if(!info.controlAllowed)return false;
    restartPending=true;restartAt=millis()+750;return true;
}
static void serviceRestart(bool outputsSafe){
    if(!restartPending)return;
    if(!outputsSafe){restartPending=false;return;}
    if(static_cast<int32_t>(millis()-restartAt)<0)return;
    restartPending=false;
#ifndef JR_R1_SIMULATOR
    esp_restart();
#endif
}
static RemoteScreen remoteScreen;
static uint16_t *remotePixels=nullptr;
static bool usbScreenshotActive=false;
static uint32_t screenEpoch=1;
void resizeScreen(int width,int height){if(remotePixels)remoteScreen.attach(remotePixels,width,height);screenChanged();}
void screenChanged(){if(++screenEpoch==0)screenEpoch=1;}
void captureScreen(int x,int y,int width,int height,const uint16_t *pixels){
    if(info.running||usbScreenshotActive)remoteScreen.capture(x,y,width,height,pixels);
}
static void releaseScreen(){free(remotePixels);remotePixels=nullptr;remoteScreen.attach(nullptr,0,0);}

static TransmitterUi *handset = nullptr;
bool beginUsbScreenshot(){
    if(!handset)return false;
    if(!remotePixels){
        const int w=handset->screenWidth(),h=handset->screenHeight();
        remotePixels=OfflineMap::allocatePixels(w*h);
        if(!remotePixels)return false;
        remoteScreen.attach(remotePixels,w,h);
    }
    usbScreenshotActive=true;return true;
}
bool writeUsbScreenshot(Stream &output){
    if(!usbScreenshotActive||!remotePixels)return false;
    const int w=remoteScreen.width(),h=remoteScreen.height();
    char header[64];snprintf(header,sizeof(header),"TFTSHOT1 %d %d RGB888 %u\n",w,h,unsigned(w*h*3));
    output.print(header);
    static uint8_t row[480*3];
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            const uint16_t v=remoteScreen.data()[y*w+x];
            const uint8_t r=(v>>11)&31,g=(v>>5)&63,b=v&31;
            row[x*3]=(r<<3)|(r>>2);row[x*3+1]=(g<<2)|(g>>4);row[x*3+2]=(b<<3)|(b>>2);
        }
        if(output.write(row,w*3)!=size_t(w*3)){usbScreenshotActive=false;return false;}
        delay(0);
    }
    output.print("\nTFTSHOT-END\n");output.flush();usbScreenshotActive=false;return true;
}

static bool passwordRestartPending = false;
static char savedPassword[64]={};
#ifndef JR_R1_SIMULATOR
static bool stopping = false;
#endif
const State &state() { return info; }
bool active() { return info.active; }
bool canControl() {return info.running&&info.controlAllowed;}
static void message(const char *text) { snprintf(info.message, sizeof(info.message), "%s", text); ++info.revision; }
bool hasSavedNetwork(const char *ssid) {return ssid&&*ssid&&strcmp(ssid,info.ssid)==0;}
static void loadNetwork() {
    info.ssid[0]=savedPassword[0]=0;Preferences p;
    if(p.begin("r1-wifi",true)){p.getString("ssid","").toCharArray(info.ssid,sizeof(info.ssid));p.getString("password","").toCharArray(savedPassword,sizeof(savedPassword));p.end();}
}
static bool saveNetwork(const char *ssid,const char *password) {
    Preferences p;if(!p.begin("r1-wifi",false))return false;
    bool ok=p.putString("ssid",ssid)==strlen(ssid);
    ok=(p.putString("password",password)==strlen(password))&&ok;p.end();
    if(!ok||!p.begin("r1-wifi",true))return false;
    ok=strcmp(p.getString("ssid","").c_str(),ssid)==0&&strcmp(p.getString("password","").c_str(),password)==0;p.end();
    if(ok){strcpy(info.ssid,ssid);strcpy(savedPassword,password);}return ok;
}
static void loadHostname() {
    Preferences p;char value[33]={};
    if(p.begin("r1-network",true)){
        const String stored=p.getString("hostname","r1evo");
        if(normalizeHostname(stored.c_str(),value))strcpy(info.hostname,value);
        p.end();
    }
    strcpy(info.activeHostname,info.hostname);
}
bool setHostname(const char *hostname) {
    char value[33]={};
    if(!normalizeHostname(hostname,value)){message("Use 1-32 letters/digits; hyphens only inside");return false;}
    Preferences p;if(!p.begin("r1-network",false)){message("Hostname save failed");return false;}
    bool ok=p.putString("hostname",value)==strlen(value);p.end();
    if(ok&&p.begin("r1-network",true)){ok=strcmp(p.getString("hostname","").c_str(),value)==0;p.end();}else ok=false;
    if(!ok){message("Hostname save failed");return false;}
    strcpy(info.hostname,value);
    message(strcmp(info.hostname,info.activeHostname)?"Hostname saved; restart handset to apply":"Hostname saved");return true;
}
static void connectedMessage(){char text[80];snprintf(text,sizeof(text),"Connected: %s.local",info.activeHostname);message(text);}
static void loadAutoConnect() {
    Preferences p;info.autoConnect=true;
    if(p.begin("r1-network",true)){info.autoConnect=p.getBool("auto",true);p.end();}
}
bool setAutoConnect(bool enabled) {
    Preferences p;if(!p.begin("r1-network",false))return false;
    const bool ok=p.putBool("auto",enabled)==1;p.end();
    if(!ok)return false;
    info.autoConnect=enabled;message(enabled?"Auto-connect enabled":"Auto-connect disabled");return true;
}
static bool validPassword(const char *value) {
    if(!value||strlen(value)<8||strlen(value)>16)return false;
    for(const char *p=value;*p;++p)if(*p<33||*p>126)return false;
    return true;
}
static bool storePassword(const char *value) {
    Preferences p;if(!p.begin("r1-portal",false))return false;
    const bool written=p.putString("password",value)==strlen(value);p.end();
    if(!written||!p.begin("r1-portal",true))return false;
    const bool verified=strcmp(p.getString("password","").c_str(),value)==0;p.end();return verified;
}
static bool ensurePassword() {
    Preferences p;char stored[65]={};
    if(p.begin("r1-portal",true)){p.getString("password","").toCharArray(stored,sizeof(stored));p.end();}
    if(validPassword(stored)){strcpy(info.apPassword,stored);return true;}
    char generated[17];
#ifdef JR_R1_SIMULATOR
    strcpy(generated,"demo12345678");
#else
    snprintf(generated,sizeof(generated),"%08lx%04lx",(unsigned long)esp_random(),(unsigned long)(esp_random()&0xffff));
#endif
    if(!storePassword(generated)){message("Cannot save setup password; retry setup");return false;}
    strcpy(info.apPassword,generated);return true;
}
bool changePassword(const char *value) {
    if(!info.active||!validPassword(value)){message("Use 8-16 characters, without spaces");return false;}
    if(!storePassword(value)){message("Password save failed; please retry");return false;}
    strcpy(info.apPassword,value);browserSession[0]=0;passwordRestartPending=true;
    message("Password saved; reconnect with new password");return true;
}

}

#ifdef JR_R1_SIMULATOR
namespace SetupPortal {
void begin(TransmitterUi *ui) { handset = ui;loadAutoConnect();loadNetwork();loadHostname(); }
void start() { info.active=true;info.controlAllowed=true;info.hotspot=true;info.running=ensurePassword();strcpy(info.apName,"R-1-EVO-Setup");if(info.running)message("Simulator: Wi-Fi preview"); }
void stop() {passwordRestartPending=false;info.active=info.hotspot=false;info.running=info.autoConnect&&info.connected;if(!info.running){releaseScreen();info.controlAllowed=false;}message("Setup closed");}
void tick(bool outputsSafe, bool allowConnect) {
    serviceRestart(outputsSafe);info.controlAllowed=outputsSafe;passwordRestartPending=false;if(!info.active){if(!info.autoConnect||!info.ssid[0])info.connected=false;else if(allowConnect)info.connected=true;info.connecting=false;info.running=info.connected&&ensurePassword();}}
void scan() { info.networkCount=2;strcpy(info.networks[0].ssid,"Home Wi-Fi");strcpy(info.networks[1].ssid,"Workshop");message("Choose a network"); }
bool connect(const char *ssid,const char *pass) { if(!ssid||strlen(ssid)>32||!ssid[0]||!pass||strlen(pass)>63||(pass[0]&&strlen(pass)<8))return false;const char *chosen=!*pass&&hasSavedNetwork(ssid)?savedPassword:pass;
    char key[64];strcpy(key,chosen);if(!saveNetwork(ssid,key))return false;info.connected=true;strcpy(info.ip,"192.168.1.42");connectedMessage();return true; }
void forget() {Preferences p;if(p.begin("r1-wifi",false)){p.putString("ssid","");p.putString("password","");p.end();}info.ssid[0]=savedPassword[0]=0;info.connected=false;message("Saved network forgotten");}
}
#else
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "firmware_package.h"
#include <ESPmDNS.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "offline_map_json.h"
#include <esp_system.h>
#include "memory_diagnostics.h"
#include "setup_portal_page.h"

namespace SetupPortal {
static WebServer server(80);
static bool routesReady=false, mdnsReady=false, connectPending=false, scanPending=false;
static char pendingSsid[33]={}, pendingPassword[64]={}, token[25]={};
static uint32_t connectStarted=0;
static bool stationOn=false, saveConnection=false;
static WifiRetry wifiRetry;
static bool disconnectSettling=false;
static uint32_t disconnectStarted=0, nextMdnsAttempt=0;
static std::atomic<uint32_t> disconnectSequence{0};
static std::atomic<unsigned> lastDisconnectReason{0};
static uint32_t attemptDisconnectSequence=0;
static uint32_t wifiAttemptCount=0, ipReadyMs=0, webReadyMs=0;
static File upload;
static size_t uploadBytes=0;
static bool uploadGood=false;
static const char *uploadError="Upload incomplete";
static constexpr size_t MaxUpload=8+2048+512*512*2;

// Browser form login avoids embedded clients' HTTP Basic dialog loops.
// Keep Basic auth available for existing clients and automation.
static bool authenticated() {
    if(browserSessionMatches(server.header("Cookie").c_str(),browserSession))return true;
    return server.authenticate("r1",info.apPassword);
}
static void loginPage(bool failed=false) {
    server.sendHeader("Cache-Control","no-store");
    server.sendHeader("Content-Security-Policy","default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'");
    String page=R"LOGIN(<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>R-1 EVO sign in</title>
<style>body{font:18px system-ui;background:#e5ebeb;color:#193039;margin:2rem auto;padding:1rem;max-width:24rem}input,button{box-sizing:border-box;width:100%;font:inherit;padding:.8rem;margin:.5rem 0 1rem}button{background:#007681;color:white;border:0}label{display:block}</style></head><body><h1>R-1 EVO</h1><p>Sign in to your handset.</p>)LOGIN";
    if(failed)page+="<p role=alert>Incorrect username or password.</p>";
    page+=R"LOGIN(<form method="post" action="/login"><label>Username<input name="username" value="r1" autocomplete="username" required maxlength="32"></label><label>Password<input type="password" name="password" autocomplete="current-password" required maxlength="64"></label><button>Sign in</button></form><p>Use the web password shown in the handset's Wi-Fi setup. The boot PIN is separate.</p></body></html>)LOGIN";
    server.send(failed?401:200,"text/html",page);
}
static bool authorized(bool write=false) {
    if (!info.running) {server.send(503,"text/plain","Web server unavailable");return false;}
    if (!authenticated()) {
        if(server.uri()=="/"&&server.method()==HTTP_GET)loginPage();
        else {server.sendHeader("Cache-Control","no-store");server.send(401,"text/plain","Sign in at the handset home page");}
        return false;
    }
    server.sendHeader("X-R1-Unlocked",handset&&handset->bootPinUnlocked()?"1":"0");
    server.sendHeader("X-R1-Control",canControl()?"1":"0");
    if(write&&!canControl()){server.send(423,"text/plain","Read-only: transmitter must be CUT for remote changes");return false;}
    if (write && server.header("X-R1-Token") != token) {server.send(403,"text/plain","Invalid setup session");return false;}
    server.sendHeader("Cache-Control","no-store");
    server.sendHeader("X-Content-Type-Options","nosniff");
    return true;
}
// The HTTP handler runs on the UI task. Keep setup active throughout an upload
// so the independently running control task cannot receive an arm request.
static uint8_t firmwareHeader[FirmwarePackage::HeaderSize];
static size_t firmwareHeaderBytes=0, firmwareBytes=0;
static FirmwarePackage::Header firmwareInfo;
static bool firmwareAccepted=false,firmwareComplete=false;
static const char *firmwareError="No firmware upload received";
static void failFirmware(const char *reason){
    if(Update.isRunning())Update.abort();
    firmwareAccepted=false;firmwareError=reason;
}
static void receiveFirmware(){
    auto &u=server.upload();
    if(u.status==UPLOAD_FILE_START){
        firmwareAccepted=firmwareComplete=false;firmwareHeaderBytes=firmwareBytes=0;
        firmwareInfo={};firmwareError="Upload incomplete";
        if(!authenticated()){firmwareError="Firmware authentication failed";return;}
        if(server.header("X-R1-Token")!=token){firmwareError="Invalid setup session";return;}
        if(!canControl()||restartPending){firmwareError="Firmware update requires CUT and no pending restart";return;}
#if !defined(JR_R1_XIAO_PCB) || defined(JR_R1_UI_PORTRAIT)
        firmwareError="OTA is supported only by the installed XIAO landscape profile";return;
#endif
        info.active=true;updateInProgress=true;firmwareAccepted=true;
        message("Firmware updating; keep power connected");
        Serial.println("OTA: authenticated upload started; outputs CUT");
    }else if(u.status==UPLOAD_FILE_WRITE&&firmwareAccepted){
        if(!canControl()){failFirmware("Firmware upload interrupted: outputs must remain CUT");return;}
        size_t offset=0;
        if(firmwareHeaderBytes<sizeof(firmwareHeader)){
            const size_t remaining=sizeof(firmwareHeader)-firmwareHeaderBytes;
            const size_t n=u.currentSize<remaining?u.currentSize:remaining;
            memcpy(firmwareHeader+firmwareHeaderBytes,u.buf,n);firmwareHeaderBytes+=n;offset=n;
            if(firmwareHeaderBytes<sizeof(firmwareHeader))return;
            const auto *partition=esp_ota_get_next_update_partition(nullptr);
            if(!partition||!FirmwarePackage::decode(firmwareHeader,sizeof(firmwareHeader),partition->size,firmwareInfo)){
                failFirmware("Invalid firmware package, target or size. Use tools/package_ota.py");return;}
            if(!Update.begin(firmwareInfo.size,U_FLASH)||!Update.setMD5(firmwareInfo.md5)){
                failFirmware("Could not open inactive firmware slot");return;}
        }
        const size_t n=u.currentSize-offset;
        if(n>firmwareInfo.size-firmwareBytes){failFirmware("Firmware package exceeds declared image size");return;}
        if(n&&Update.write(u.buf+offset,n)!=n){failFirmware("Firmware write failed; current firmware retained");return;}
        firmwareBytes+=n;
    }else if(u.status==UPLOAD_FILE_END&&firmwareAccepted){
        if(firmwareHeaderBytes!=sizeof(firmwareHeader)||firmwareBytes!=firmwareInfo.size){failFirmware("Incomplete firmware image; current firmware retained");return;}
        if(!canControl()||!Update.end()){failFirmware("Firmware validation failed; current firmware retained");return;}
        firmwareComplete=true;Serial.printf("OTA: validated %u application bytes\n",unsigned(firmwareBytes));
    }else if(u.status==UPLOAD_FILE_ABORTED){
        failFirmware("Firmware upload disconnected; current firmware retained");
        updateInProgress=false;message(firmwareError);
    }
}
static bool number(const char *key,int low,int high,int &out) {
    if(!server.hasArg(key))return false;
    String text=server.arg(key);char *end=nullptr;
    if(text.length()==0||text.length()>7)return false;
    long n=strtol(text.c_str(),&end,10);
    if(*end||n<low||n>high)return false;
    out=n;return true;
}
static bool readMap(const char *path, OfflineMap::Package &result, uint16_t *&pixels) {
    pixels=nullptr;auto file=LittleFS.open(path,"r");if(!file||file.size()>MaxUpload)return false;
    uint8_t header[8];if(file.read(header,8)!=8)return false;
    if(memcmp(header,"R1IM",4)==0){
        const uint32_t length=uint32_t(header[4])|(uint32_t(header[5])<<8)|(uint32_t(header[6])<<16)|(uint32_t(header[7])<<24);
        if(!length||length>2048||file.size()<8+length)return false;
        char *metadata=static_cast<char*>(malloc(length+1));if(!metadata)return false;
        if(file.read(reinterpret_cast<uint8_t*>(metadata),length)!=length){free(metadata);return false;}metadata[length]=0;
        DynamicJsonDocument doc(4096);
        const auto error=deserializeJson(doc,static_cast<const char*>(metadata),length);free(metadata);
        if(error||!OfflineMap::decodeImage(doc.as<JsonVariantConst>(),result))return false;
        const size_t bytes=size_t(result.width)*result.height*2;
        if(file.size()!=8+length+bytes)return false;
        pixels=OfflineMap::allocatePixels(result.width*result.height);if(!pixels)return false;
        if(file.read(reinterpret_cast<uint8_t*>(pixels),bytes)!=bytes){free(pixels);pixels=nullptr;return false;}
        return true;
    }
    if(file.size()>48000)return false;file.seek(0);
    DynamicJsonDocument doc(48000);
    if(deserializeJson(doc,file,DeserializationOption::NestingLimit(6)))return false;
    return OfflineMap::decode(doc.as<JsonVariantConst>(),result);
}
static const char *mapPath() {return LittleFS.exists("/driving-map.dat")?"/driving-map.dat":"/driving-map.json";}
static void loadMap() {
    std::unique_ptr<OfflineMap::Package> p(new(std::nothrow) OfflineMap::Package);
    uint16_t *pixels=nullptr;
    if(p && info.storageReady && readMap(mapPath(),*p,pixels))OfflineMap::install(*p,pixels);
}
static void routes() {
    if(routesReady)return;routesReady=true;
    const char *headers[]={"X-R1-Token","Cookie"};server.collectHeaders(headers,2);
    server.on("/login",HTTP_GET,[]{loginPage();});
    server.on("/login",HTTP_POST,[]{
        if(!info.running||!browserSession[0]){server.send(503,"text/plain","Web server unavailable");return;}
        if(server.arg("username")!="r1"||server.arg("password")!=info.apPassword){loginPage(true);return;}
        server.sendHeader("Set-Cookie",String("R1Session=")+browserSession+"; Path=/; HttpOnly; SameSite=Strict");
        server.sendHeader("Cache-Control","no-store");server.sendHeader("Location","/");
        server.send(303,"text/plain","");
    });
    server.on("/",HTTP_GET,[]{if(authorized()){server.sendHeader("Content-Security-Policy","default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self' https://photon.komoot.io https://overpass-api.de https://server.arcgisonline.com; img-src 'self' blob: https://tile.openstreetmap.org https://server.arcgisonline.com; frame-ancestors 'none'");server.send_P(200,"text/html",setupPortalPage);}});
    server.on("/api/screen",HTTP_GET,[]{
        if(!authorized())return;
        int hx,hy,hw,hh;char zone[48];
        if(handset->portalDriveHoldBounds(hx,hy,hw,hh))snprintf(zone,sizeof(zone),"%d,%d,%d,%d",hx,hy,hw,hh);else strcpy(zone,"-");
        server.sendHeader("X-R1-Hold-Zone",zone);
        server.sendHeader("X-R1-Wake",handset->portalScreenNeedsWake()?"1":"0");
        if(!remotePixels){
            const int w=handset->screenWidth(),h=handset->screenHeight();
            remotePixels=OfflineMap::allocatePixels(w*h);
            if(!remotePixels){server.send(503,"text/plain","Insufficient PSRAM for live view");return;}
            remoteScreen.attach(remotePixels,w,h);handset->portalEnableScreen();
            server.send(202,"text/plain","Preparing screen");return;
        }
        int since=0;if(server.hasArg("since")&&!number("since",0,INT32_MAX,since)){server.send(400,"text/plain","Invalid revision");return;}
        int epoch=0;if(!number("epoch",0,INT32_MAX,epoch)){server.send(400,"text/plain","Invalid page epoch");return;}
        if(uint32_t(epoch)!=screenEpoch)since=0;
        int first=0,count=0;if(!remoteScreen.region(since,first,count)){server.send(204,"text/plain","");return;}
        // R1SC / revision / page epoch / width / full height / first row / rows.
        uint8_t header[20]={'R','1','S','C'};
        auto put16=[&](int at,unsigned v){header[at]=v;header[at+1]=v>>8;};
        auto put32=[&](int at,uint32_t v){put16(at,v);put16(at+2,v>>16);};
        put32(4,remoteScreen.revision());put32(8,screenEpoch);
        put16(12,remoteScreen.width());put16(14,remoteScreen.height());put16(16,first);put16(18,count);
        const size_t bytes=size_t(remoteScreen.width())*count*2;
        server.setContentLength(sizeof(header)+bytes);server.send(200,"application/octet-stream","");
        server.sendContent(reinterpret_cast<const char*>(header),sizeof(header));
        server.sendContent(reinterpret_cast<const char*>(remoteScreen.data()+first*remoteScreen.width()),bytes);
    });
    server.on("/api/screen/input",HTTP_POST,[]{
        int epoch=0,x=0,y=0,action=0,hold=0;
        if(!number("action",0,9,action)){server.send(400,"text/plain","Invalid action");return;}
        if(!authorized(action!=7&&action!=9))return;
        if((action==7||action==9)&&server.header("X-R1-Token")!=token){server.send(403,"text/plain","Invalid setup session");return;}
        if(!remotePixels||!number("epoch",1,INT32_MAX,epoch)||uint32_t(epoch)!=screenEpoch){server.send(409,"text/plain","Screen changed; wait for live view");return;}
        if(action==9){const bool ok=handset->portalRequestRemoteControl();server.send(ok?202:409,"text/plain",ok?"CUT requested; wait for control readiness":"Finish the physical interaction first");return;}
        if(action==7){const bool ok=handset->portalWakeScreen();server.send(ok?200:409,"text/plain",ok?"Display awake":"Screen changed; please tap again");return;}
        if(action==8){
            int endX=0,endY=0,duration=0;
            if(!number("x",0,479,x)||!number("y",0,479,y)||!number("endX",0,479,endX)||
               !number("endY",0,479,endY)||!number("duration",0,1200,duration)){
                server.send(400,"text/plain","Invalid gesture");return;
            }
            const bool ok=handset->portalPageSwipe(x,y,endX,endY,duration);
            server.send(ok?200:409,"text/plain",ok?"Gesture applied":"Gesture unavailable on this screen");return;
        }
        if(!number("action",0,6,action)||((action==0||action==3||action==4||action==6)&&(!number("x",0,479,x)||!number("y",0,479,y)))||((action==4||action==5||action==6)&&!number("hold",1,INT32_MAX,hold))){server.send(400,"text/plain","Invalid tap");return;}
        Serial.printf("WEB TOUCH: action=%d xy=(%d,%d) epoch=%d\n",action,x,y,epoch);
        const bool ok=handset->portalRemoteInput(x,y,action,hold,epoch);
        if(ok&&action==3){server.send(200,"text/plain",String(handset->portalRemoteHoldId()));return;}
        server.send(ok?200:409,"text/plain",ok?(action==4?"Enable requested; neutral and link checks apply":"Input applied"):(action>=3?"Hold cancelled or expired; keep holding Drive until ready, then release":"Finish the current interaction, or use the physical LCD for connection/password changes"));
    });
    server.on("/api/pin",HTTP_POST,[]{
        if(!authorized(true))return;
        const bool ok=handset->changePin(server.arg("current").c_str(),server.arg("next").c_str(),server.arg("confirm").c_str());
        server.send(ok?200:400,"text/plain",ok?"PIN saved. Walk mode unchanged; outputs remain CUT.":"PIN change failed: check current PIN, four-digit new PIN and confirmation.");
    });
    server.on("/api/access",HTTP_GET,[]{if(authorized())server.send(200,"application/json",canControl()?"{\"controlAllowed\":true}":"{\"controlAllowed\":false}");});
    server.on("/api/hostname",HTTP_POST,[]{
        if(!authorized(true))return;
        const bool ok=setHostname(server.arg("hostname").c_str());
        server.send(ok?200:400,"text/plain",info.message);
    });
    server.on("/api/timer",HTTP_GET,[]{
        if(!authorized())return;
        const auto &timer=handset->portalTimer();const auto &s=timer.settings;
        DynamicJsonDocument d(4096);d["slot"]=handset->portalModelSlot()+1;d["mode"]=unsigned(s.mode);d["duration"]=s.durationSeconds;
        d["minimumLap"]=s.minimumLapSeconds;d["target"]=s.targetSeconds;d["autoStart"]=s.autoStart;d["raceAlarm"]=s.raceAlarm;
        d["onSeconds"]=s.onSeconds;d["elapsedMs"]=timer.elapsed();d["running"]=timer.running();d["ready"]=timer.ready();
        d["bestMs"]=timer.bestLap();d["averageMs"]=timer.averageLap();d["totalMs"]=timer.totalLaps();
        auto laps=d.createNestedArray("laps");for(unsigned i=0;i<timer.lapCount();++i)laps.add(timer.lap(i));
        String body;serializeJson(d,body);server.send(200,"application/json",body);
    });
    server.on("/api/timer",HTTP_POST,[]{
        if(!authorized(true))return;
        int slot;if(!number("slot",1,10,slot)||slot!=handset->portalModelSlot()+1){
            server.send(409,"text/plain","Model changed; refresh the timer before making changes");return;}
        if(server.hasArg("action")){
            int action;if(!number("action",0,8,action)||(action==5&&server.arg("confirm")!="clear-laps")){
                server.send(400,"text/plain","Invalid timer action or missing clear confirmation");return;}
            if(action==3&&!handset->portalTimer().settings.autoStart){server.send(409,"text/plain","Enable throttle auto-start before choosing Ready");return;}
            const bool ok=handset->portalTimerAction(action);
            server.send(ok?200:409,"text/plain",ok?"Timer updated; output state unchanged":"Pause the timer before clearing laps");return;
        }
        int mode,duration,minimum,target,autoStart,race;
        if(!number("mode",0,2,mode)||!number("duration",1,5999,duration)||!number("minimumLap",1,60,minimum)||
           !number("target",0,5999,target)||!number("autoStart",0,1,autoStart)||!number("raceAlarm",0,1,race)){
            server.send(400,"text/plain","Invalid timer settings");return;}
        auto settings=handset->portalTimer().settings;settings.mode=static_cast<TimerMode>(mode);
        settings.durationSeconds=duration;settings.minimumLapSeconds=minimum;settings.targetSeconds=target;
        settings.autoStart=autoStart;settings.raceAlarm=race;
        const bool ok=handset->portalSetTimerSettings(settings);
        server.send(ok?200:409,"text/plain",ok?"Timer settings saved; clock reset, lap history retained":"Timer settings rejected");
    });
    server.on("/api/memory",HTTP_GET,[]{
        if(!authorized())return;
        const auto m=MemoryDiagnostics::snapshot();
        DynamicJsonDocument d(2048);d["sampledMs"]=m.sampledMs;
        const MemoryDiagnostics::Pool pools[]={m.internal,m.psram,m.dma};
        const char *names[]={"internal","psram","dma"};
        for(int n=0;n<3;++n){auto p=d.createNestedObject(names[n]);p["total"]=pools[n].total;
            p["free"]=pools[n].free;p["minimum"]=pools[n].minimum;p["largest"]=pools[n].largest;}
        auto l=d.createNestedObject("lvgl");l["ready"]=m.lvglReady;l["total"]=m.lvglTotal;l["used"]=m.lvglUsed;
        l["peak"]=m.lvglPeak;l["largest"]=m.lvglLargest;l["fragmentation"]=m.lvglFragmentation;
        auto t=d.createNestedObject("stacks");t["ui"]=m.loopStack;t["control"]=m.controlStack;t["crsf"]=m.crsfStack;
        String body;serializeJson(d,body);server.send(200,"application/json",body);
    });
    server.on("/api/state",HTTP_GET,[]{
        if(!authorized())return;
        DynamicJsonDocument d(10000);d["version"]=1;d["controlAllowed"]=canControl();d["token"]=token;d["model"]=handset->portalModelName();d["slot"]=handset->portalModelSlot()+1;d["modelImage"]=handset->portalModelImage();d["modelColour"]=handset->portalModelColour();d["modelAccent"]=handset->portalModelAccent();d["modelBase"]=handset->portalModelBase();
        d["moduleLinkEnabled"]=handset->moduleLinkEnabled();d["moduleBatteryDetected"]=handset->moduleBatteryDetected();
        d["wifiAttempts"]=wifiAttemptCount;d["lastDisconnectReason"]=lastDisconnectReason.load();
        d["ipReadyMs"]=ipReadyMs;d["webReadyMs"]=webReadyMs;d["uptimeMs"]=millis();
        d["hostname"]=info.hostname;d["activeHostname"]=info.activeHostname;d["ssid"]=info.ssid;d["ip"]=info.ip;d["storage"]=info.storageReady;d["map"]=OfflineMap::current().name;
        auto config=handset->controlConfigurationSnapshot();auto profiles=d.createNestedArray("channels");
        for(unsigned i=0;i<16;++i){auto c=profiles.createNestedObject();auto &p=config.channelProfiles[i];
            c["channel"]=i+1;c["negativeEndpoint"]=p.negativeEndpointPercent;c["positiveEndpoint"]=p.positiveEndpointPercent;
            c["negativeRate"]=p.negativeRatePercent;c["positiveRate"]=p.positiveRatePercent;
            c["negativeExpo"]=p.negativeExpoPercent;c["positiveExpo"]=p.positiveExpoPercent;
            c["negativeRamp"]=p.negativeRampMs;c["positiveRamp"]=p.positiveRampMs;
            c["profileSubtrim"]=p.subtrimPercent;c["reversed"]=p.reversed;
            c["trim"]=config.channelParameters[i].trimPermille;c["subtrim"]=config.channelParameters[i].subtrimPermille;
        }
        String body;serializeJson(d,body);server.send(200,"application/json",body);
    });
    server.on("/api/controls",HTTP_GET,[]{
        if(!authorized())return;
        DynamicJsonDocument d(14000);auto list=d.createNestedArray("controls");
        auto config=handset->controlConfigurationSnapshot();
        for(unsigned i=0;i<kPhysicalControlCount;++i){auto o=list.createNestedObject();auto desc=physicalControlDescriptor(i);auto b=handset->portalBinding(i);
            o["id"]=i;o["name"]=desc.name;o["kind"]=static_cast<unsigned>(desc.kind);
            o["target"]=static_cast<unsigned>(b.target);o["selection"]=b.target==AssignmentTarget::CrsfChannel?b.crsfChannel:b.targetId;
            o["behaviour"]=static_cast<unsigned>(b.behaviour);o["context"]=b.contexts;o["enabled"]=b.enabled;
            o["minimum"]=b.minimum;o["maximum"]=b.maximum;o["step"]=b.step;
            o["initial"]=b.initial;o["failsafe"]=b.failsafe;o["inverted"]=b.inverted;
            if(i<2){o["target"]=4;o["selection"]=i==0?config.steeringChannel:config.throttleChannel;o["enabled"]=i==0?config.steeringEnabled:config.throttleEnabled;}
        }
        auto f=d.createNestedArray("functions");for(unsigned i=0;;++i){int id=TransmitterUi::portalFunction(i);if(id<0)break;auto o=f.createNestedObject();o["id"]=id;o["name"]=transmitterFunctionName(static_cast<TransmitterFunction>(id));}
        if(d.overflowed()){server.send(500,"text/plain","Control catalogue exceeds capacity");return;}
        String body;serializeJson(d,body);server.send(200,"application/json",body);
    });
    server.on("/api/control",HTTP_POST,[]{
        if(!authorized(true))return;
        int input,target,selection,behaviour,context,enabled;
        if(!number("input",0,kPhysicalControlCount-1,input)||!number("target",1,4,target)||!number("selection",0,65535,selection)||!number("behaviour",0,8,behaviour)||!number("context",1,31,context)||!number("enabled",0,1,enabled)){server.send(400,"text/plain","Invalid control settings");return;}
        ControlBinding values;const ControlBinding *range=nullptr;
        const char *fields[]={"minimum","maximum","step","initial","failsafe","inverted"};
        bool any=false;for(auto field:fields)any=any||server.hasArg(field);
        if(any){
            int low,high,step,start,fault,inverted;
            if(input<2||(target!=4&&target!=2)||!number("minimum",-1000,1000,low)||!number("maximum",-1000,1000,high)||!number("step",1,2000,step)||!number("initial",-1000,1000,start)||!number("failsafe",-1000,1000,fault)||!number("inverted",0,1,inverted)){
                server.send(400,"text/plain","Invalid assignment range settings");return;
            }
            values.minimum=low;values.maximum=high;values.step=step;values.initial=start;values.failsafe=fault;values.inverted=inverted;range=&values;
        }
        bool ok=input<2 ? (target==4&&selection>=1&&selection<=16&&handset->portalSetRole(input,selection,enabled)) : handset->portalSetBinding(input,target,selection,behaviour,context,enabled,range);
        server.send(ok?200:400,"text/plain",ok?"Assignment saved; outputs remain CUT":input<2?"Channel is occupied or role save failed":handset->portalBindingError());
    });
    server.on("/api/model-appearance",HTTP_POST,[]{
        if(!authorized(true))return;
        const String image=server.arg("image"),colour=server.arg("colour");
        bool valid=image.length()==1&&image[0]>='0'&&image[0]<='8';
        auto parsePaint=[&](String value,uint32_t &rgb){
            rgb=0x1000000;if(value=="original"||value.length()==0)return true;
            if(value.length()!=7||value[0]!='#')return false;rgb=0;
            for(unsigned i=1;i<7;++i){const char c=value[i];unsigned digit;
                if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10;
                else if(c>='A'&&c<='F')digit=c-'A'+10;else return false;
                rgb=(rgb<<4)|digit;
            }return true;
        };
        uint32_t rgb,accent,base;
        valid=valid&&parsePaint(colour,rgb)&&parsePaint(server.arg("accent"),accent)&&parsePaint(server.arg("base"),base);
        if(!valid){server.send(400,"text/plain","Select an image and valid paint colours");return;}
        const bool ok=handset->portalSetModelAppearance(image[0]-'0',rgb,accent,base);
        server.send(ok?200:400,"text/plain",ok?"Model appearance saved":"Could not save model appearance; outputs must be CUT");
    });
    server.on("/api/model",HTTP_POST,[]{if(!authorized(true))return;const bool ok=handset->portalRenameModel(server.arg("name").c_str());server.send(ok?200:400,"text/plain",ok?"Model name saved":"Use 1-12 printable ASCII characters");});
    server.on("/api/mixing",HTTP_GET,[]{
        if(!authorized())return;
        int ch;if(!number("channel",1,16,ch)){server.send(400,"text/plain","Invalid channel");return;}
        auto config=handset->controlConfigurationSnapshot();const auto &m=config.mixing[ch-1];
        DynamicJsonDocument d(2048);auto rules=d.createNestedArray("rules");
        for(const auto &r:m.rules){auto o=rules.createNestedObject();
            o["negativeWeight"]=r.splitWeight?r.negativeWeight:r.weight;o["followTrim"]=r.followTrim;
            o["sourceMode"]=r.sourceMode;o["thresholdPercent"]=r.thresholdPercent;o["hysteresisPercent"]=r.hysteresisPercent;
            o["enabled"]=r.enabled;o["source"]=r.source;o["weight"]=r.weight;o["replace"]=r.replace;o["condition"]=r.condition;o["control"]=r.control;
        }
        d["constantPercent"]=m.constantPercent;d["outputMinimum"]=m.outputMinimum;d["outputMaximum"]=m.outputMaximum;d["cutOverride"]=m.cutOverride;d["cutPercent"]=m.cutPercent;
        d["includeBase"]=m.includeBase;d["rateEnabled"]=m.rateEnabled;d["rateControl"]=m.rateControl;
        d["lowNegative"]=m.lowNegative;d["lowPositive"]=m.lowPositive;
        d["highNegative"]=m.highNegative;d["highPositive"]=m.highPositive;d["transitionMs"]=m.transitionMs;
        String body;serializeJson(d,body);server.send(200,"application/json",body);
    });
    server.on("/api/mixing",HTTP_POST,[]{
        if(!authorized(true))return;
        int ch;if(!number("channel",1,16,ch)){server.send(400,"text/plain","Invalid channel");return;}
        ChannelMixSettings m;
        int base;if(!number("includeBase",0,1,base)){server.send(400,"text/plain","Invalid base contribution");return;}m.includeBase=base;
        const char *extraKeys[]={"constantPercent","outputMinimum","outputMaximum","cutOverride","cutPercent"};
        int extra[5];const int lower[]={-100,-100,-100,0,-100},upper[]={100,100,100,1,100};
        for(int n=0;n<5;++n)if(!number(extraKeys[n],lower[n],upper[n],extra[n])){server.send(400,"text/plain","Invalid baseline / limits");return;}
        m.constantPercent=extra[0];m.outputMinimum=extra[1];m.outputMaximum=extra[2];m.cutOverride=extra[3];m.cutPercent=extra[4];
        const char *keys[]={"enabled","source","weight","replace","condition","control","negativeWeight","followTrim","sourceMode","thresholdPercent","hysteresisPercent"};
        const int lo[]={0,0,-100,0,0,0,-100,0,0,-100,0},hi[]={1,kMixSourceCount-1,100,1,4,int(kPhysicalControlCount)-1,100,1,2,100,100};
        for(int i=0;i<2;++i){
            int v[11];for(int j=0;j<11;++j){String key=String("mix")+i+keys[j];
                if(!number(key.c_str(),lo[j],hi[j],v[j])){server.send(400,"text/plain","Invalid mix rule");return;}}
            auto &r=m.rules[i];r.enabled=v[0];r.source=v[1];r.weight=v[2];r.replace=v[3];r.condition=v[4];r.control=v[5];r.negativeWeight=v[6];r.followTrim=v[7];r.splitWeight=1;r.sourceMode=v[8];r.thresholdPercent=v[9];r.hysteresisPercent=v[10];
        }
        const char *rateKeys[]={"rateEnabled","rateControl","lowNegative","lowPositive","highNegative","highPositive","transitionMs"};
        const int max[]={1,int(kPhysicalControlCount)-1,100,100,100,100,2000};int v[7];
        for(int j=0;j<7;++j)if(!number(rateKeys[j],0,max[j],v[j])){server.send(400,"text/plain","Invalid switched rate");return;}
        m.rateEnabled=v[0];m.rateControl=v[1];m.lowNegative=v[2];m.lowPositive=v[3];m.highNegative=v[4];m.highPositive=v[5];m.transitionMs=v[6];
        if(!validChannelMix(m)){server.send(400,"text/plain","Invalid mix: check minimum / maximum and maintained switch selection");return;}
        auto candidate=handset->controlConfigurationSnapshot();candidate.mixing[ch-1]=m;
        if(!validChannelMixGraph(candidate.mixing)){server.send(400,"text/plain","Circular channel mix: choose a source that does not depend on this destination");return;}
        if(!validConfiguredMixing(candidate)){server.send(400,"text/plain","Steering/throttle limits must include zero; CUT overrides and threshold switches are auxiliary-only");return;}
        if(server.arg("preview")=="1") {
            int base,a,b,high,walk;
            if(!number("testBase",-1000,1000,base)||!number("testA",-1000,1000,a)||!number("testB",-1000,1000,b)||
               !number("testHigh",0,1,high)||!number("testWalk",0,1,walk)) {server.send(400,"text/plain","Invalid preview input");return;}
            const int16_t sources[]={int16_t(a),int16_t(b)};
            const auto result=previewChannelMix(candidate,ch,m,base,sources,high,walk);
            int sweep=1;
            if(server.hasArg("graphSource")&&!number("graphSource",0,2,sweep)){server.send(400,"text/plain","Invalid graph input");return;}
            int16_t samples[kMixCurvePoints];
            previewChannelMixCurve(candidate,ch,m,base,sources,high,walk,sweep,samples);
            DynamicJsonDocument d(2048);auto curve=d.createNestedArray("curve");
            for(auto value:samples)curve.add(value);
            d["graphSource"]=sweep;d["graphPosition"]=sweep==0?base:sources[sweep-1];
            auto bars=d.createNestedArray("values");
            bars.add((m.includeBase?base:0)+int(m.constantPercent)*10);bars.add(result.contributions[0]);bars.add(result.contributions[1]);bars.add(result.mixed);bars.add(result.output);
            String body;serializeJson(d,body);server.send(200,"application/json",body);return;
        }
        const bool ok=handset->portalSetMixing(ch,m);
        server.send(ok?200:500,"text/plain",ok?"Mixing saved; outputs remain CUT":"Mixing save failed");
    });
    server.on("/api/channel",HTTP_POST,[]{
        if(!authorized(true))return;
        const char *keys[]={"channel","negativeEndpoint","positiveEndpoint","negativeRate","positiveRate","negativeExpo","positiveExpo","negativeRamp","positiveRamp","profileSubtrim","reversed","trim","subtrim"};
        const int lows[]={1,50,50,0,0,-100,-100,0,0,-20,0,-200,-200};
        const int highs[]={16,100,100,100,100,100,100,2000,2000,20,1,200,200};int v[13];
        for(unsigned i=0;i<13;++i)if(!number(keys[i],lows[i],highs[i],v[i])){server.send(400,"text/plain",String("Invalid ")+keys[i]);return;}
        ChannelResponseProfile p;p.negativeEndpointPercent=v[1];p.positiveEndpointPercent=v[2];p.negativeRatePercent=v[3];p.positiveRatePercent=v[4];p.negativeExpoPercent=v[5];p.positiveExpoPercent=v[6];p.negativeRampMs=v[7];p.positiveRampMs=v[8];p.subtrimPercent=v[9];p.reversed=v[10];
        bool ok=handset->portalSetChannel(v[0],p,v[11],v[12]);server.send(ok?200:500,"text/plain",ok?"Channel saved; outputs remain CUT":"Save failed");
    });
    server.on("/api/firmware",HTTP_POST,[]{
        if(!authorized(true)){if(Update.isRunning())Update.abort();updateInProgress=false;return;}
        if(!firmwareComplete){server.send(400,"text/plain",firmwareError);updateInProgress=false;message(firmwareError);return;}
        // end() has validated the image and selected the inactive slot for next boot.
        requestRestart();
        server.send(202,"text/plain","Firmware verified. Restarting in CUT; wait for Wi-Fi, then reload.");
    },receiveFirmware);
    server.on("/api/restart",HTTP_POST,[]{
        if(!authorized(true))return;
        if(server.arg("confirm")!="restart"){server.send(400,"text/plain","Restart confirmation required");return;}
        if(!requestRestart()){server.send(423,"text/plain","Restart requires CUT");return;}
        server.send(202,"text/plain","Restart scheduled. Wait for the handset to reconnect, then reload this page. Saved settings are preserved.");
    });
    server.on("/api/storage",HTTP_POST,[]{
        if(!authorized(true))return;
        if(server.arg("confirm")!="prepare"){server.send(400,"text/plain","Explicit storage preparation required");return;}
        if(info.storageReady){server.send(409,"text/plain","Storage already ready; no format performed");return;}
        info.storageReady=LittleFS.format()&&LittleFS.begin(false);++info.revision;
        server.send(info.storageReady?200:500,"text/plain",info.storageReady?"Map storage prepared":"Storage unavailable");
    });
    server.on("/api/map",HTTP_GET,[]{if(!authorized())return;auto file=LittleFS.open(mapPath(),"r");if(!file){server.send(404,"text/plain","No uploaded map");return;}server.streamFile(file,"application/octet-stream");});
    server.on("/api/map",HTTP_POST,[]{
        if(!authorized(true))return;
        std::unique_ptr<OfflineMap::Package> candidate(new(std::nothrow) OfflineMap::Package);
        uint16_t *pixels=nullptr;
        if(!candidate){LittleFS.remove("/map.pending");server.send(503,"text/plain","Insufficient working memory to validate map");return;}
        Serial.printf("MAP: validating %u uploaded bytes; free stack=%u\n",unsigned(uploadBytes),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
        if(!uploadGood){LittleFS.remove("/map.pending");server.send(400,"text/plain",uploadError);return;}
        if(!readMap("/map.pending",*candidate,pixels)){LittleFS.remove("/map.pending");server.send(400,"text/plain","Map file validation or allocation failed; export the map package for diagnosis");return;}
        // LittleFS rename atomically replaces the old destination; interrupted
        // uploads never touch the active map.
        if(!LittleFS.rename("/map.pending","/driving-map.dat")){free(pixels);server.send(500,"text/plain","Could not commit map");return;}
        OfflineMap::install(*candidate,pixels);++info.revision;Serial.println("MAP: saved and installed");server.send(200,"text/plain","Map saved; available on GPS view");
    },[]{
        auto &u=server.upload();
        if(u.status==UPLOAD_FILE_START){uploadError=!canControl()?"Map upload requires CUT":!info.storageReady?"Map storage is not ready. Use Prepare map storage first":!authenticated()?"Map upload authentication failed":server.header("X-R1-Token")!=token?"Session expired; reload this page":"";
            uploadGood=!*uploadError;uploadBytes=0;if(upload)upload.close();
            if(uploadGood){upload=LittleFS.open("/map.pending","w");uploadGood=bool(upload);if(!uploadGood)uploadError="Could not open map storage for writing";}}
        else if(u.status==UPLOAD_FILE_WRITE){if(!canControl()){uploadGood=false;uploadError="Map upload interrupted: transmitter is no longer CUT";}uploadBytes+=u.currentSize;if(uploadBytes>MaxUpload){uploadGood=false;uploadError="Map exceeds the 512 x 512 image package size limit";}if(uploadGood&&upload.write(u.buf,u.currentSize)!=u.currentSize){uploadGood=false;uploadError="Map storage write failed; storage may be full";}}
        else if(u.status==UPLOAD_FILE_END){if(upload)upload.close();}
        else if(u.status==UPLOAD_FILE_ABORTED){if(upload)upload.close();uploadGood=false;LittleFS.remove("/map.pending");}
    });
    server.onNotFound([]{server.send(404,"text/plain","Not found");});
}
void begin(TransmitterUi *ui) {
    handset=ui;loadAutoConnect();loadNetwork();loadHostname();
    static bool eventLogInstalled=false;
    if(!eventLogInstalled){eventLogInstalled=true;WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t details){
        if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
            lastDisconnectReason.store(details.wifi_sta_disconnected.reason);
            disconnectSequence.fetch_add(1);
            Serial.printf("WIFI t=%lums: disconnected reason=%u\n",(unsigned long)millis(),details.wifi_sta_disconnected.reason);
        }
        if(event==ARDUINO_EVENT_WIFI_STA_CONNECTED)Serial.printf("WIFI t=%lums: associated; waiting for DHCP\n",(unsigned long)millis());
        if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP)Serial.printf("WIFI t=%lums: DHCP address received\n",(unsigned long)millis());
    });}

    info.storageReady=LittleFS.begin(false);loadMap();
}
void start() {if(info.active)return;info.active=true;stopping=false;message("Entering setup; waiting for CUT");}
void stop() {stopping=true;message("Closing setup...");}
void scan() {if(info.running)scanPending=true;}
bool connect(const char *ssid,const char *password) {
    if(!info.active||!ssid||!*ssid||strlen(ssid)>32||!password||strlen(password)>63||(strlen(password)>0&&strlen(password)<8))return false;
    const char *chosen=!*password&&hasSavedNetwork(ssid)?savedPassword:password;
    if(info.connected&&hasSavedNetwork(ssid)&&strcmp(chosen,savedPassword)==0){message("Already connected to saved network");return true;}
    strcpy(pendingSsid,ssid);strcpy(pendingPassword,chosen);connectPending=true;saveConnection=true;message("Connection queued");return true;
}
void forget() {
    if(!info.active)return;Preferences p;if(p.begin("r1-wifi",false)){p.clear();p.end();}
    info.ssid[0]=savedPassword[0]=0;info.connecting=connectPending=disconnectSettling=false;wifiRetry.reset();WiFi.disconnect();message("Saved network forgotten");
}
static void queueSavedConnection() {
    strcpy(pendingSsid,info.ssid);strcpy(pendingPassword,savedPassword);
    connectPending=true;saveConnection=false;
}
static void stationTick(bool outputsSafe) {
    const uint32_t now=millis();
    if(connectPending && outputsSafe && !info.scanning){
        if(!disconnectSettling){
            Serial.printf("WIFI: preparing %s connection\n",saveConnection?"manual":"saved");
            WiFi.disconnect(false,false);disconnectStarted=now;disconnectSettling=true;
            info.connecting=true;message("Connecting to home Wi-Fi...");
        }
        if(now-disconnectStarted<300)return;
        connectPending=disconnectSettling=false;
        attemptDisconnectSequence=disconnectSequence.load();
        WiFi.begin(pendingSsid,pendingPassword);connectStarted=now;++wifiAttemptCount;
        Serial.printf("WIFI t=%lums: attempt %lu started\n",(unsigned long)now,(unsigned long)wifiAttemptCount);
        return;
    }
    // Association alone is not enough: wait for a usable DHCP address.
    const bool connected=!disconnectSettling && WiFi.status()==WL_CONNECTED && uint32_t(WiFi.localIP())!=0;
    if(connected!=info.connected){
        info.connected=connected;
        if(connected){
            WiFi.localIP().toString().toCharArray(info.ip,sizeof(info.ip));
            wifiRetry.reset();nextMdnsAttempt=now;ipReadyMs=now;
            Serial.printf("WIFI: connected IP=%s RSSI=%d\n",info.ip,WiFi.RSSI());
            connectedMessage();
        }else{
            info.ip[0]=0;if(mdnsReady)MDNS.end();mdnsReady=false;
            message("Home Wi-Fi disconnected; reconnect pending");
        }
    }
    if(connected&&!mdnsReady&&static_cast<int32_t>(now-nextMdnsAttempt)>=0){
        nextMdnsAttempt=now+5000;mdnsReady=MDNS.begin(info.activeHostname);
        if(mdnsReady&&info.running)MDNS.addService("http","tcp",80);
        if(!mdnsReady)Serial.println("WIFI: mDNS unavailable; will retry (IP access remains available)");
    }
    if(info.connecting&&!connectPending&&connected&&WiFi.SSID()==pendingSsid){
        info.connecting=false;wifiRetry.reset();
        if(saveConnection){
            const bool ok=saveNetwork(pendingSsid,pendingPassword);saveConnection=false;
            message(ok?"Connected and saved":"Connected; credential save failed");
        }
    }
    if(info.connecting&&!connectPending&&!connected){
        const auto status=WiFi.status();
        // Status may still describe the previous attempt. Only act early on a
        // disconnect event belonging to this attempt, allowing transient retries.
        const bool failed=now-connectStarted>=5000 && disconnectSequence.load()!=attemptDisconnectSequence;
        if(failed||now-connectStarted>=30000){
            info.connecting=false;
            if(outputsSafe)WiFi.disconnect(false,false);
            const uint32_t delay=wifiRetry.failed(now);
            char text[96];snprintf(text,sizeof(text),"Wi-Fi unavailable; retry in %lus when CUT",(unsigned long)(delay/1000));message(text);
            Serial.printf("WIFI: attempt failed status=%d; retry delay=%lums\n",int(status),(unsigned long)delay);
        }
    }
    if(info.autoConnect&&info.ssid[0]&&!connected&&!info.connecting&&!connectPending&&outputsSafe&&wifiRetry.ready(now))queueSavedConnection();
}
static void stopWeb() {
    server.stop();info.running=false;releaseScreen();
    if(mdnsReady)MDNS.end();mdnsReady=false;
}
static bool startWeb() {
    if(info.running)return true;
    if(!ensurePassword())return false;
    snprintf(token,sizeof(token),"%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
    snprintf(browserSession,sizeof(browserSession),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
    routes();server.begin();info.running=true;webReadyMs=millis();
    Serial.printf("WEB t=%lums: listening; IP ready at %lums; attempts=%lu\n",(unsigned long)webReadyMs,(unsigned long)ipReadyMs,(unsigned long)wifiAttemptCount);
    if(mdnsReady)MDNS.addService("http","tcp",80);
    return true;
}
static void radioOff() {
    stopWeb();if(mdnsReady)MDNS.end();mdnsReady=false;WiFi.scanDelete();WiFi.disconnect(true);WiFi.mode(WIFI_OFF);
    stationOn=false;info.connected=info.connecting=info.scanning=info.hotspot=false;info.ip[0]=0;connectPending=scanPending=disconnectSettling=false;wifiRetry.reset();
}
void tick(bool outputsSafe, bool allowConnect) {
    serviceRestart(outputsSafe);
    info.controlAllowed=outputsSafe;
    if(stopping){
        passwordRestartPending=false;WiFi.softAPdisconnect(true);
        info.active=info.hotspot=info.scanning=false;info.networkCount=0;stopping=scanPending=false;
        if(info.autoConnect&&info.ssid[0]){WiFi.mode(WIFI_STA);stationOn=true;}
        else radioOff();
        message("Setup closed; home web access stays available");
    }
    if(!info.active){
        if(!info.autoConnect||!info.ssid[0]){if(stationOn)radioOff();return;}
        if(!stationOn){
            if(!allowConnect)return;
            WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setHostname(info.activeHostname);WiFi.setAutoReconnect(false);WiFi.setSleep(false);
            stationOn=true;wifiRetry.reset();queueSavedConnection();
        }
        stationTick(allowConnect);
        if(info.connected){if(startWeb())server.handleClient();}
        else if(info.running)stopWeb();
        return;
    }
    if(!outputsSafe){if(info.running)server.handleClient();return;}
    if(passwordRestartPending){passwordRestartPending=false;stopWeb();WiFi.softAPdisconnect(true);info.hotspot=false;}
    if(!info.hotspot){
        if(!ensurePassword())return;
        snprintf(info.apName,sizeof(info.apName),"R-1-EVO-%04lx",(unsigned long)(ESP.getEfuseMac()&0xffff));
        WiFi.persistent(false);WiFi.mode(WIFI_AP_STA);WiFi.setHostname(info.activeHostname);WiFi.setAutoReconnect(false);WiFi.setSleep(false);stationOn=true;
        if(!WiFi.softAP(info.apName,info.apPassword)){message("Access point failed; close and retry");return;}
        info.hotspot=true;message("Setup ready: 192.168.4.1");
        if(info.ssid[0]&&WiFi.status()!=WL_CONNECTED&&!info.connecting)queueSavedConnection();
    }
    startWeb();stationTick(allowConnect);
    if(scanPending&&!info.scanning&&!info.connecting){scanPending=false;WiFi.scanDelete();info.scanning=WiFi.scanNetworks(true)!=WIFI_SCAN_FAILED;message("Scanning 2.4 GHz networks...");}
    if(info.scanning){int count=WiFi.scanComplete();if(count>=0){info.networkCount=0;for(int i=0;i<count&&info.networkCount<8;++i){String ssid=WiFi.SSID(i);if(!ssid.length())continue;bool duplicate=false;for(int j=0;j<info.networkCount;++j)if(ssid==info.networks[j].ssid)duplicate=true;if(duplicate)continue;auto &n=info.networks[info.networkCount++];ssid.toCharArray(n.ssid,sizeof(n.ssid));n.rssi=WiFi.RSSI(i);n.secure=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;}WiFi.scanDelete();info.scanning=false;message("Select a network or enter its name");}else if(count==WIFI_SCAN_FAILED){info.scanning=false;message("Scan failed; retry");}}
    if(info.running)server.handleClient();
}
}
#endif
