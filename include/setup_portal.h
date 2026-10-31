#pragma once
#include <cstdint>
class TransmitterUi;
namespace SetupPortal {
struct Network { char ssid[33] = {}; int rssi = 0; bool secure = true; };
struct State {
    bool active = false, running = false, connected = false, scanning = false;
    bool storageReady = false;
    bool controlAllowed = false, hotspot = false;
    bool autoConnect = true, connecting = false;
    char ssid[33] = {}, ip[20] = {}, apName[33] = {}, apPassword[17] = {};
    char hostname[33] = "r1evo", activeHostname[33] = "r1evo";
    char message[96] = "Open setup to enable Wi-Fi";
    Network networks[8];
    uint8_t networkCount = 0;
    uint32_t revision = 0;
};
const State &state();
bool active();
bool canControl();
bool firmwareUpdating();
void begin(TransmitterUi *ui);
void start();
void stop();
bool requestRestart();
void tick(bool outputsSafe, bool allowConnect);
inline void tick(bool outputsSafe){tick(outputsSafe,outputsSafe);}
void scan();
bool connect(const char *ssid, const char *password);
bool hasSavedNetwork(const char *ssid);
void forget();
bool setAutoConnect(bool enabled);
bool setHostname(const char *hostname);
void captureScreen(int x,int y,int width,int height,const uint16_t *pixels);
void screenChanged();
void resizeScreen(int width,int height);
// Shared hotspot/web credential; saved separately from home Wi-Fi settings.
bool changePassword(const char *password);
}
