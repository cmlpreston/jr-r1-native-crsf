#pragma once
constexpr int ESP_RST_SW=3,ESP_RST_POWERON=1;
inline int resetReason=ESP_RST_SW;
inline int esp_reset_reason(){return resetReason;}
