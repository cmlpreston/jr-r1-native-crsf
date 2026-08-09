#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "crsf_telemetry.h"

struct Channel2Settings {
    uint8_t brakeEndpointPercent = 100;
    uint8_t forwardEndpointPercent = 100;
    uint8_t brakeRatePercent = 100;
    uint8_t forwardRatePercent = 100;
    int8_t brakeExpoPercent = 0;
    int8_t forwardExpoPercent = 0;
    int8_t subtrimPercent = 0;
    bool reversed = false;
};

struct ChannelRoleSettings {
    uint8_t steeringChannel = 1;
    uint8_t throttleChannel = 2;
};

struct ThrottleCalibration {
    uint16_t minimum = 1447;
    uint16_t centre = 1970;
    uint16_t maximum = 4015;
};

struct SystemSettings {
    bool buzzerEnabled = true;
    bool keyClicksEnabled = true;
    uint16_t buzzerPitchHz = 2400;
    uint16_t keyClickDurationMs = 40;
};

enum class BatteryChemistry : uint8_t {
    LiFe,
    LiPo,
    NiMH,
};

struct TxBatterySettings {
    BatteryChemistry chemistry = BatteryChemistry::LiFe;
    uint8_t cellCount = 3;
    uint16_t lowAlarmDecivolts = 90;
    bool voltageSenseEnabled = false;
};

enum class UiPage : uint8_t {
    Dashboard,
    MainMenu,
    Channels,
    ChannelRoles,
    Channel2,
    ThrottleResponse,
    ChannelPlaceholder,
    Setup,
    TxBatterySetup,
    Calibration,
};

class Channel2Ui {
public:
    bool begin();
    void update(uint16_t rawAdc, int16_t rawInputPermille,
                int16_t outputPermille, uint16_t crsfValue,
                bool throttleArmed, const CrsfTelemetryState &telemetry,
                uint16_t txVoltageDecivolts, bool txVoltageValid);
    int16_t mapAdcToPermille(uint16_t adc) const;
    int16_t applySettings(int16_t rawInputPermille) const;
    bool calibrationActive() const { return calibrationActive_; }
    uint16_t throttleCentreAdc() const { return calibration_.centre; }
    const Channel2Settings &settings() const { return settings_; }
    const ThrottleCalibration &calibration() const { return calibration_; }
    bool txVoltageSenseEnabled() const { return txBatterySettings_.voltageSenseEnabled; }
    uint8_t steeringChannel() const { return channelRoleSettings_.steeringChannel; }
    uint8_t throttleChannel() const { return channelRoleSettings_.throttleChannel; }
    bool consumeChannelRolesChanged();

private:
    void loadSettings();
    void saveSettings();
    void saveCalibration();
    void loadChannelRoleSettings();
    void saveChannelRoleSettings();
    void loadSystemSettings();
    void saveSystemSettings();
    void loadTxBatterySettings();
    void saveTxBatterySettings();
    void drawMainMenu();
    void drawDashboard();
    void drawDashboardLive(uint16_t crsfValue, bool throttleArmed,
                           const CrsfTelemetryState &telemetry,
                           uint16_t txVoltageDecivolts,
                           bool txVoltageValid);
    void drawChannelsPage();
    void drawChannelRolesPage();
    void drawChannelRoleRow(int row);
    void drawChannelPlaceholder();
    void drawPage();
    void drawThrottleResponsePage();
    void drawThrottleResponseRow(int row);
    void drawSetupPage();
    void drawSetupRow(int row);
    void drawTxBatteryPage();
    void drawTxBatteryRow(int row);
    void drawCalibrationPage();
    void drawCalibrationValues(uint16_t adc);
    void drawSettingRow(int row);
    void drawLiveIndicator(int16_t outputPermille, uint16_t crsfValue);
    void handleTouch(int16_t x, int16_t y);
    void handleMainMenuTouch(int16_t x, int16_t y);
    void handleDashboardTouch(int16_t x, int16_t y);
    void handleChannelsTouch(int16_t x, int16_t y);
    void handleChannelRolesTouch(int16_t x, int16_t y);
    void handleThrottleResponseTouch(int16_t x, int16_t y);
    void handleSetupTouch(int16_t x, int16_t y);
    void handleTxBatteryTouch(int16_t x, int16_t y);
    void startCalibration(uint16_t adc);
    void updateCalibration(uint16_t adc);
    void cancelCalibration();
    bool readTouch(int16_t &x, int16_t &y, uint16_t &rawX, uint16_t &rawY,
                   uint16_t &rawZ);
    uint16_t readTouchRegister(uint8_t command);
    void playBuzzer(uint16_t durationMs);
    void playKeyClick();
    void stopBuzzer();
    void resetLvglObjects();

    Channel2Settings settings_;
    ChannelRoleSettings channelRoleSettings_;
    ThrottleCalibration calibration_;
    SystemSettings systemSettings_;
    TxBatterySettings txBatterySettings_;
    bool displayReady_ = false;
    UiPage activePage_ = UiPage::Dashboard;
    uint8_t channelsBank_ = 0;
    uint8_t selectedChannel_ = 2;
    bool calibrationActive_ = false;
    bool channelRolesChanged_ = false;
    bool calibrationSawMinimum_ = false;
    bool calibrationSawMaximum_ = false;
    uint16_t calibrationMinimum_ = UINT16_MAX;
    uint16_t calibrationMaximum_ = 0;
    uint16_t calibrationStableAdc_ = 0;
    uint32_t calibrationStableSinceMs_ = 0;
    uint16_t latestRawAdc_ = 0;
    bool touchWasDown_ = false;
    int16_t lastTouchX_ = 0;
    int16_t lastTouchY_ = 0;
    int16_t lastMarkerX_ = -1;
    uint16_t lastDisplayedCrsf_ = UINT16_MAX;
    bool lastDashboardArmed_ = false;
    bool lastLinkValid_ = false;
    bool lastReceiverVoltageValid_ = false;
    bool lastTxVoltageValid_ = false;
    uint8_t lastLinkQuality_ = UINT8_MAX;
    uint8_t lastRssiDbm_ = UINT8_MAX;
    uint16_t lastReceiverVoltageDecivolts_ = UINT16_MAX;
    uint16_t lastTxVoltageDecivolts_ = UINT16_MAX;
    uint32_t nextLowBatteryAlarmMs_ = 0;
    uint32_t nextDisplayUpdateMs_ = 0;
    lv_obj_t *dashboardThrottleBar_ = nullptr;
    lv_obj_t *dashboardSafetyLabel_ = nullptr;
    lv_obj_t *dashboardLinkLabel_ = nullptr;
    lv_obj_t *dashboardRssiLabel_ = nullptr;
    lv_obj_t *dashboardRxVoltageLabel_ = nullptr;
    lv_obj_t *dashboardTxVoltageLabel_ = nullptr;
    lv_obj_t *channel2LiveBar_ = nullptr;
    lv_obj_t *channel2ValueLabel_ = nullptr;
    lv_obj_t *calibrationMinimumLabel_ = nullptr;
    lv_obj_t *calibrationLiveLabel_ = nullptr;
    lv_obj_t *calibrationMaximumLabel_ = nullptr;
};
