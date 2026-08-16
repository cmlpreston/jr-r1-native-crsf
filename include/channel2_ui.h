#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "control_assignment.h"
#include "crsf_telemetry.h"
#include "crsf_channel_output.h"
#include "crsf_management.h"
#include "physical_input.h"
#include "proportional_input.h"

struct Channel2Settings {
    uint8_t brakeEndpointPercent = 100;
    uint8_t forwardEndpointPercent = 100;
    uint8_t brakeRatePercent = 100;
    uint8_t forwardRatePercent = 100;
    int8_t brakeExpoPercent = 0;
    int8_t forwardExpoPercent = 0;
    uint16_t brakeSpeedMs = 0;
    uint16_t forwardSpeedMs = 0;
    int8_t subtrimPercent = 0;
    bool reversed = false;
};

struct ChannelRoleSettings {
    uint8_t steeringChannel = 1;
    uint8_t throttleChannel = 2;
    bool steeringEnabled = true;
    bool throttleEnabled = true;
};

struct ChannelParameterValues {
    int16_t trimPermille = 0;
    int16_t subtrimPermille = 0;
};

struct SystemSettings {
    bool buzzerEnabled = true;
    bool keyClicksEnabled = true;
    uint16_t buzzerPitchHz = 2400;
    uint16_t keyClickDurationMs = 40;
    uint16_t screenTimeoutSeconds = 60;
    uint8_t displayBrightnessPercent = 100;
    bool nightMode = false;
};

enum class DriveMode : uint8_t {
    Walk,
    Normal,
};

struct DriveModeSettings {
    // WALK limits physical forward drive only. Brake/reverse retains its full
    // configured authority. Arm/CUT is an independent safety state.
    DriveMode activeMode = DriveMode::Normal;
    uint8_t walkLimitPercent = 35;
};

enum class OutputSafetyStatus : uint8_t {
    Cut,
    StartupWarning,
    NeutralRequired,
    Armed,
    Calibration,
    InputFault,
    ControlStale,
    ChannelDisabled,
};

enum class BatteryChemistry : uint8_t {
    LiFe,
    LiPo,
    NiMH,
    LiIon,
};

struct TxBatterySettings {
    BatteryChemistry chemistry = BatteryChemistry::LiFe;
    uint8_t cellCount = 3;
    uint16_t lowAlarmDecivolts = 90;
    bool voltageSenseEnabled = false;
};

struct RxBatterySettings {
    BatteryChemistry chemistry = BatteryChemistry::LiFe;
    uint8_t cellCount = 2;
    uint16_t lowAlarmDecivolts = 60;
    bool alarmEnabled = true;
};

enum class UiPage : uint8_t {
    Dashboard,
    StartupWarning,
    Telemetry,
    MainMenu,
    Channels,
    ChannelRoles,
    Channel2,
    ThrottleResponse,
    ChannelSpeed,
    ControlAssignments,
    BindingEditor,
    InputMonitor,
    DriveModes,
    ChannelPlaceholder,
    Setup,
    TxBatterySetup,
    RxBatterySetup,
    DisplaySetup,
    ElrsBinding,
    Calibration,
};

class Channel2Ui {
public:
    bool begin();
    void update(uint16_t rawAdc, int16_t rawInputPermille,
                int16_t outputPermille, uint16_t crsfValue,
                OutputSafetyStatus safetyStatus,
                const CrsfTelemetryState &telemetry,
                uint16_t txVoltageDecivolts, bool txVoltageValid,
                const CrsfChannelOutputModel &channelOutputs,
                bool channelOutputsLive);
    int16_t mapAdcToPermille(uint16_t adc) const;
    int16_t applySettings(int16_t rawInputPermille) const;
    int16_t applyDriveMode(int16_t configuredPermille,
                           int16_t rawInputPermille) const;
    int16_t neutralOutputPermille() const;
    int16_t applyChannelAdjustments(uint8_t channel,
                                    int16_t inputPermille) const;
    int16_t channelNeutralPermille(uint8_t channel) const;
    bool applyChannelParameterAction(uint16_t targetId, int16_t value);
    bool applyTransmitterFunctionAction(uint16_t targetId, int16_t value);
    bool applyDriveModeAction(uint16_t targetId, int16_t value);
    uint8_t activeControlContext() const;
    int16_t applyChannelSpeed(int16_t targetPermille, uint32_t nowMs);
    void resetChannelSpeed();
    bool calibrationActive() const { return triggerInput_.calibrationActive(); }
    uint16_t throttleCentreAdc() const { return triggerInput_.centre(); }
    const Channel2Settings &settings() const { return settings_; }
    const ProportionalCalibration &calibration() const { return triggerInput_.calibration(); }
    bool txVoltageSenseEnabled() const { return txBatterySettings_.voltageSenseEnabled; }
    uint8_t steeringChannel() const { return channelRoleSettings_.steeringChannel; }
    uint8_t throttleChannel() const { return channelRoleSettings_.throttleChannel; }
    bool steeringEnabled() const { return channelRoleSettings_.steeringEnabled; }
    bool throttleEnabled() const { return channelRoleSettings_.throttleEnabled; }
    bool armRequested() const { return armRequested_; }
    DriveMode driveMode() const { return driveModeSettings_.activeMode; }
    bool captureScreenshot(Stream &output);
#ifdef JR_R1_SIMULATOR
    void showSimulatorPage(UiPage page) { activePage_ = page; redrawActivePage(); }
#endif
    void setPhysicalInputStates(const PhysicalInputStateStore &states);
    void setStartupPreflight(bool complete, uint32_t unsafeMask);
    size_t copyBindings(ControlBinding *destination, size_t capacity) const;
    bool consumeBindingsChanged();
    bool consumeChannelRolesChanged();
    bool consumeSafetyResetRequested();
    bool consumeElrsManagementRequest(ElrsManagementRequest &request);

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
    void loadRxBatterySettings();
    void saveRxBatterySettings();
    void loadDriveModeSettings();
    void saveDriveModeSettings();
    void loadElrsSettings();
    void saveElrsSettings();
    void drawMainMenu();
    void refreshMainMenuFocus();
    void moveMainMenuFocus(int8_t direction);
    void activateMainMenuFocus();
    void beginPageFocus(bool registerStandardHeader = true);
    enum class PageControlEvent : int8_t {
        Decrease = -1,
        Activate = 0,
        Increase = 1,
    };
    using PageControlHandler = void (Channel2Ui::*)(uint8_t,
                                                     PageControlEvent);
    void addPageActionTarget(int16_t x, int16_t y, int16_t width,
                             int16_t height, PageControlHandler handler,
                             uint8_t argument);
    lv_obj_t *makePageActionButton(int16_t x, int16_t y, int16_t width,
                                   int16_t height, const char *text,
                                   uint8_t fontSize, uint32_t fill,
                                   uint32_t textColour, uint32_t border,
                                   PageControlHandler handler,
                                   uint8_t argument);
    void addPageEditorTarget(int16_t x, int16_t y, int16_t width,
                             int16_t height,
                             int16_t decreaseX, int16_t decreaseY,
                             int16_t decreaseWidth, int16_t decreaseHeight,
                             int16_t increaseX, int16_t increaseY,
                             int16_t increaseWidth, int16_t increaseHeight,
                             PageControlHandler handler, uint8_t argument);
    void finishPageFocus();
    void refreshPageFocus();
    void movePageFocus(int8_t direction);
    void activatePageFocus();
    bool adjustPageFocus(int8_t direction);
    bool dispatchPageControlTouch(int16_t x, int16_t y);
    bool dispatchPageControl(uint8_t index, PageControlEvent event);
    void controlChannelRoles(uint8_t row, PageControlEvent event);
    void controlTxBattery(uint8_t row, PageControlEvent event);
    void controlRxBattery(uint8_t row, PageControlEvent event);
    void controlDisplay(uint8_t row, PageControlEvent event);
    void controlBindingEditor(uint8_t field, PageControlEvent event);
    void controlChannels(uint8_t item, PageControlEvent event);
    void controlAssignments(uint8_t item, PageControlEvent event);
    void controlDriveMode(uint8_t item, PageControlEvent event);
    void controlSetup(uint8_t item, PageControlEvent event);
    void controlThrottleSetting(uint8_t row, PageControlEvent event);
    void controlThrottleResponse(uint8_t row, PageControlEvent event);
    void controlChannelSpeed(uint8_t row, PageControlEvent event);
    void controlDashboard(uint8_t item, PageControlEvent event);
    void controlChannelConfig(uint8_t item, PageControlEvent event);
    void controlInputMonitor(uint8_t item, PageControlEvent event);
    void controlElrsBinding(uint8_t item, PageControlEvent event);
    void controlThrottleHeader(uint8_t item, PageControlEvent event);
    void controlNavigation(uint8_t item, PageControlEvent event);
    void navigateBack();
    bool navigateDashboardPage(int8_t direction);
    void drawDashboard();
    void drawStartupWarningPage();
    void drawDashboardLive(uint16_t crsfValue,
                           OutputSafetyStatus safetyStatus,
                           const CrsfTelemetryState &telemetry,
                           uint16_t txVoltageDecivolts,
                           bool txVoltageValid);
    void drawTelemetryPage();
    void drawTelemetryLive(const CrsfTelemetryState &telemetry,
                           uint16_t txVoltageDecivolts,
                           bool txVoltageValid);
    void drawChannelsPage();
    void drawChannelRolesPage();
    void drawChannelRoleRow(int row);
    void drawChannelPlaceholder();
    void drawPage();
    void drawThrottleResponsePage();
    void drawThrottleResponseRow(int row);
    void drawThrottleResponseGraph();
    void refreshThrottleResponseGraph(int16_t rawInputPermille);
    void drawChannelSpeedPage();
    void drawChannelSpeedRow(int row);
    void drawControlAssignmentsPage();
    void drawBindingEditorPage();
    void drawInputMonitorPage();
    void refreshInputMonitorRows();
    void drawDriveModesPage();
    void drawSetupPage();
    void drawSetupRow(int row);
    void drawTxBatteryPage();
    void drawTxBatteryRow(int row);
    void drawRxBatteryPage();
    void drawRxBatteryRow(int row);
    void drawDisplayPage();
    void drawDisplayRow(int row);
    void drawElrsBindingPage();
    void drawCalibrationPage();
    void drawCalibrationValues(uint16_t adc);
    void drawSettingRow(int row);
    void refreshSettingRowValue(uint8_t row);
    void scheduleSettingsSave();
    void drawLiveIndicator(int16_t outputPermille, uint16_t crsfValue);
    void handleTouch(int16_t x, int16_t y);
    void handleMainMenuTouch(int16_t x, int16_t y);
    void handleDashboardTouch(int16_t x, int16_t y);
    void handleTelemetryTouch(int16_t x, int16_t y);
    void handleChannelsTouch(int16_t x, int16_t y);
    void handleChannelRolesTouch(int16_t x, int16_t y);
    void handleChannelConfigTouch(int16_t x, int16_t y);
    void handleThrottleResponseTouch(int16_t x, int16_t y);
    void handleChannelSpeedTouch(int16_t x, int16_t y);
    void handleControlAssignmentsTouch(int16_t x, int16_t y);
    void handleBindingEditorTouch(int16_t x, int16_t y);
    void handleInputMonitorTouch(int16_t x, int16_t y);
    void handleDriveModesTouch(int16_t x, int16_t y);
    void handleSetupTouch(int16_t x, int16_t y);
    void handleTxBatteryTouch(int16_t x, int16_t y);
    void handleRxBatteryTouch(int16_t x, int16_t y);
    void handleDisplayTouch(int16_t x, int16_t y);
    void handleElrsBindingTouch(int16_t x, int16_t y);
    void appendElrsKeyboardCharacter(char character);
    void queueElrsRequest(ElrsManagementAction action,
                          ElrsManagementTarget target);
    void updateTouch();
    void startCalibration(uint16_t adc);
    void updateCalibration(uint16_t adc);
    void cancelCalibration();
    bool readTouch(int16_t &x, int16_t &y, uint16_t &rawX, uint16_t &rawY,
                   uint16_t &rawZ);
    uint16_t readTouchRegister(uint8_t command);
    bool initialiseTouchController();
    bool readGt911Register(uint16_t reg, uint8_t *data, size_t length);
    bool writeGt911Register(uint16_t reg, uint8_t value);
    void applyBacklight(uint8_t percent);
    void playBuzzer(uint16_t durationMs);
    void playKeyClick();
    void stopBuzzer();
    void resetLvglObjects();
    void redrawActivePage();
    void sleepDisplay();
    void wakeDisplay();
    void loadBindings();
    void saveBindings();
    void loadChannelParameters();
    void saveChannelParameters();
    void scheduleChannelParameterSave();
    void restoreDefaultBindings();
    int bindingIndexForInput(PhysicalControlId input) const;
    ControlBinding defaultBindingForInput(PhysicalControlId input) const;
    bool saveBindingDraft();
    bool removeSelectedBinding();
    bool bindingUsesChannel(uint8_t channel,
                            PhysicalControlId exceptInput) const;
    int bindingIndexForChannelParameter(uint8_t channel,
                                        ChannelParameterKind kind) const;
    bool assignChannelParameterControl(uint8_t channel,
                                       ChannelParameterKind kind,
                                       PhysicalControlId input);
    PhysicalControlId adjacentChannelParameterControl(
        uint8_t channel, ChannelParameterKind kind, int8_t direction) const;
    int16_t channelParameterValue(uint8_t channel,
                                  ChannelParameterKind kind) const;
    uint8_t adjacentBindingChannel(uint8_t current, int8_t direction) const;
    ControlBehaviour adjacentBindingBehaviour(PhysicalControlKind kind,
                                              ControlBehaviour current,
                                              int8_t direction) const;
    AssignmentTarget adjacentBindingTarget(PhysicalControlKind kind,
                                            AssignmentTarget current,
                                            int8_t direction) const;

    Channel2Settings settings_;
    ChannelRoleSettings channelRoleSettings_;
    ProportionalInput triggerInput_;
    SystemSettings systemSettings_;
    TxBatterySettings txBatterySettings_;
    RxBatterySettings rxBatterySettings_;
    DriveModeSettings driveModeSettings_;
    char elrsBindPhrase_[52] = {};
    bool elrsKeyboardUppercase_ = false;
    bool elrsRequestPending_ = false;
    ElrsManagementRequest elrsPendingRequest_;
    bool elrsReceiverLinkAvailable_ = false;
    char elrsStatus_[80] = "Enter phrase, then SET TX";
    uint32_t lastTransmitterUidUpdateMs_ = 0;
    uint32_t lastReceiverUidUpdateMs_ = 0;
    bool displayReady_ = false;
    UiPage activePage_ = UiPage::Dashboard;
    bool startupPreflightComplete_ = false;
    uint32_t startupWarningMask_ = 0;
    UiPage throttleResponseReturnPage_ = UiPage::MainMenu;
    UiPage driveModesReturnPage_ = UiPage::Dashboard;
    uint8_t channelsBank_ = 0;
    uint8_t selectedChannel_ = 2;
    uint8_t inputMonitorBank_ = 0;
    uint8_t assignmentBank_ = 0;
    PhysicalControlId selectedBindingInput_ = PhysicalControlId::TimerButton;
    ControlBinding bindingDraft_;
    char bindingError_[64] = {};
    ControlBinding bindings_[kPhysicalControlCount] = {};
    size_t bindingCount_ = 0;
    ChannelParameterValues channelParameters_[16] = {};
    bool channelRolesChanged_ = false;
    bool safetyResetRequested_ = false;
    bool bindingsChanged_ = false;
    bool armRequested_ = false;
    bool displaySleeping_ = false;
    // A touch-triggered sleep can leave the controller IRQ asserted for the
    // tail of the same gesture. Require an idle/released sample before that
    // IRQ is allowed to wake the display again.
    bool displayWakeArmed_ = false;
    uint32_t displayWakeIdleSinceMs_ = 0;
    uint32_t nextDisplayWakePollMs_ = 0;
    // A touch used to wake the display, or one beginning inside the post-tap
    // guard interval, must be discarded as one complete physical gesture.
    bool discardTouchUntilRelease_ = false;
    int16_t slewedThrottlePermille_ = 0;
    uint32_t lastSlewUpdateMs_ = 0;
    uint32_t lastInteractionMs_ = 0;
    uint16_t latestRawAdc_ = 0;
    bool touchWasDown_ = false;
    uint32_t touchReleaseSinceMs_ = 0;
    uint32_t touchIgnoreUntilMs_ = 0;
    int16_t lastTouchX_ = 0;
    int16_t lastTouchY_ = 0;
    int16_t lastMarkerX_ = -1;
    uint16_t lastDisplayedCrsf_ = UINT16_MAX;
    uint16_t dashboardEnabledChannelMask_ = 0;
    uint16_t dashboardChannelValues_[CRSF_CHANNEL_COUNT] = {};
    uint16_t lastDashboardChannelValues_[CRSF_CHANNEL_COUNT] = {};
    OutputSafetyStatus lastSafetyStatus_ = OutputSafetyStatus::ControlStale;
    bool lastLinkValid_ = false;
    bool lastReceiverVoltageValid_ = false;
    bool lastTxVoltageValid_ = false;
    uint8_t lastLinkQuality_ = UINT8_MAX;
    uint8_t lastRssiDbm_ = UINT8_MAX;
    uint16_t lastReceiverVoltageDecivolts_ = UINT16_MAX;
    uint16_t lastTxVoltageDecivolts_ = UINT16_MAX;
    uint32_t nextTxLowBatteryAlarmMs_ = 0;
    uint32_t nextRxLowBatteryAlarmMs_ = 0;
    uint32_t nextDisplayUpdateMs_ = 0;
    bool settingsSavePending_ = false;
    uint32_t settingsSaveDueMs_ = 0;
    bool channelParameterSavePending_ = false;
    uint32_t channelParameterSaveDueMs_ = 0;
    lv_obj_t *dashboardThrottleBar_ = nullptr;
    lv_obj_t *mainMenuButtons_[8] = {};
    uint8_t mainMenuFocusIndex_ = 1;
    struct PageFocusTarget {
        struct Rectangle {
            int16_t x = 0;
            int16_t y = 0;
            int16_t width = 0;
            int16_t height = 0;
        };
        Rectangle focus;
        Rectangle activate;
        Rectangle decrease;
        Rectangle increase;
        PageControlHandler handler = nullptr;
        uint8_t argument = 0;
    };
    // The ELRS on-screen keyboard is the largest current page (46 controls).
    // Keep one shared registry large enough that new pages do not need a
    // second, page-specific keypad implementation.
    static constexpr uint8_t kMaximumPageFocusTargets = 64;
    PageFocusTarget pageFocusTargets_[kMaximumPageFocusTargets] = {};
    uint8_t pageFocusCount_ = 0;
    uint8_t pageFocusIndex_ = 0;
    bool pageStandardHeaderPending_ = false;
    lv_obj_t *pageFocusIndicator_ = nullptr;
    lv_obj_t *dashboardChannelBars_[CRSF_CHANNEL_COUNT] = {};
    lv_obj_t *dashboardSafetyPanel_ = nullptr;
    lv_obj_t *dashboardSafetyLabel_ = nullptr;
    lv_obj_t *dashboardLinkLabel_ = nullptr;
    lv_obj_t *dashboardRxVoltageLabel_ = nullptr;
    lv_obj_t *dashboardTxVoltageLabel_ = nullptr;
    lv_obj_t *telemetryLinkStateLabel_ = nullptr;
    lv_obj_t *telemetryLinkQualityLabel_ = nullptr;
    lv_obj_t *telemetryRssiLabel_ = nullptr;
    lv_obj_t *telemetrySnrLabel_ = nullptr;
    lv_obj_t *telemetryRadioLabel_ = nullptr;
    lv_obj_t *telemetryTxVoltageLabel_ = nullptr;
    lv_obj_t *telemetryRxVoltageLabel_ = nullptr;
    lv_obj_t *telemetryFramesLabel_ = nullptr;
    lv_obj_t *telemetryCrcLabel_ = nullptr;
    lv_obj_t *telemetryBytesLabel_ = nullptr;
    lv_obj_t *telemetryAgeLabel_ = nullptr;
    lv_obj_t *channel2LiveBar_ = nullptr;
    lv_obj_t *channel2ValueLabel_ = nullptr;
    lv_obj_t *channel2SettingValueLabels_[4] = {};
    lv_obj_t *channel2DirectionToggle_ = nullptr;
    lv_obj_t *channel2DirectionToggleLabel_ = nullptr;
    static constexpr uint8_t kThrottleGraphPointCount = 33;
    lv_point_t throttleCurvePoints_[kThrottleGraphPointCount] = {};
    lv_point_t throttleLinearPoints_[2] = {};
    lv_point_t throttleBrakeEndpointPoints_[2] = {};
    lv_point_t throttleForwardEndpointPoints_[2] = {};
    lv_point_t throttleSubtrimPoints_[2] = {};
    lv_obj_t *throttleGraphCurve_ = nullptr;
    lv_obj_t *throttleGraphMarker_ = nullptr;
    lv_obj_t *throttleGraphLiveLabel_ = nullptr;
    lv_obj_t *calibrationMinimumLabel_ = nullptr;
    lv_obj_t *calibrationLiveLabel_ = nullptr;
    lv_obj_t *calibrationMaximumLabel_ = nullptr;
    PhysicalInputState physicalInputStates_[kPhysicalControlCount] = {};
    lv_obj_t *inputMonitorStateLabels_[5] = {};
    lv_obj_t *inputMonitorEventLabels_[5] = {};
};
