#pragma once
#include "gps_basemap.h"

#include <Arduino.h>
#include <lvgl.h>
#include "channel_response.h"
#include "dashboard_widgets.h"
#include "speed_statistics.h"
#include "control_configuration.h"
#include "control_assignment.h"
#include "crsf_telemetry.h"
#include "crsf_channel_output.h"
#include "crsf_management.h"
#include "output_safety.h"
#include "physical_input.h"
#include "proportional_input.h"

struct SystemSettings {
    bool buzzerEnabled = true;
    bool keyClicksEnabled = true;
    uint16_t buzzerPitchHz = 2400;
    uint16_t keyClickDurationMs = 40;
    uint16_t screenTimeoutSeconds = 60;
    bool logoScreensaverEnabled = true;
    uint8_t displayBrightnessPercent = 100;
    bool nightMode = false;
    uint8_t darkColourScheme = 0; // Retained when switching to Light.
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
    BootSplash,
    LogoSaver,
    Dashboard,
    StartupWarning,
    Telemetry,
    MainMenu,
    ModelSelect,
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
    FirmwareInfo,
    ElrsBinding,
    ElrsTxConfig,
    EscProgramming,
    Calibration,
    Tune,
    ValueEditor,
    Gps,
    ChannelMonitor,
    SensorDetail,
    WidgetSelect,
    RadioDiagnostics,
    WidgetEditor,
    BatteryMenu,
    ColourScheme,
};

class TransmitterUi {
public:
    bool begin();
    void update(uint16_t rawAdc, uint16_t steeringRawAdc,
                int16_t rawInputPermille,
                int16_t outputPermille, uint16_t crsfValue,
                OutputSafetyStatus safetyStatus,
                const CrsfTelemetryState &telemetry,
                uint16_t txVoltageDecivolts, bool txVoltageValid,
                const CrsfChannelOutputModel &channelOutputs,
                bool channelOutputsLive);
    int16_t mapAdcToPermille(uint16_t adc) const;
    ProportionalInputProfile throttleInputProfile() const;
    ProportionalInputProfile steeringInputProfile() const;
    ControlConfigurationSnapshot controlConfigurationSnapshot() const;
    bool applyChannelParameterAction(uint16_t targetId, int16_t value);
    bool applyTransmitterFunctionAction(uint16_t targetId, int16_t value);
    bool applyDriveModeAction(uint16_t targetId, int16_t value);
    uint8_t activeControlContext() const;
    bool calibrationActive() const {
        return triggerInput_.calibrationActive() ||
               steeringInput_.calibrationActive();
    }
    uint16_t throttleCentreAdc() const { return triggerInput_.centre(); }
    const ChannelResponseProfile &settings() const { return throttleProfile(); }
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
    bool runSimulatorUiAudit();
    bool selectSimulatorTxParameter(uint8_t id) {
        const auto p = txConfig_.parameter(id);
        if (!txConfig_.ready() || !p) return false;
        if (p->type == 11) { txConfigFolder_ = id; txConfigSelected_ = 0; }
        else { txConfigSelected_ = id; txConfigDraft_ = p->value; }
        drawElrsTxConfigPage(); return true;
    }
    void showSimulatorPage(UiPage page) {
        if (page == UiPage::ElrsTxConfig) txConfig_.start(millis());
        activePage_ = page; redrawActivePage();
    }
    bool selectSimulatorModel(uint8_t slot) { return selectModelMemory(slot); }
    void selectSimulatorDisplayBank(uint8_t bank) {
        displaySetupBank_ = bank > 1 ? 1 : bank;
    }
    void selectSimulatorChannel(uint8_t channel) {
        selectedChannel_ = channel >= 1 && channel <= CRSF_CHANNEL_COUNT
            ? channel : selectedChannel_;
    }
#endif
    void setPhysicalInputStates(const PhysicalInputStateStore &states);
    void setStartupPreflight(bool complete, uint32_t unsafeMask);
    size_t copyBindings(ControlBinding *destination, size_t capacity) const;
    bool consumeBindingsChanged();
    bool consumeChannelRolesChanged();
    bool consumeSafetyResetRequested();
    bool consumeElrsManagementRequest(ElrsManagementRequest &request);

private:
    bool designPage() const;
    void drawDesignPage(UiPage page);
    void drawPortraitDetails(UiPage page);
    uint8_t portraitResponseBank_ = 0;
    void updateDesign(const CrsfTelemetryState &telemetry, OutputSafetyStatus safety,
                      uint16_t txVoltage, bool txValid);
    void openDesignEditor(uint8_t field, UiPage returnPage);
    int designValue() const;
    void applyDesignValue(int value);
    void toggleRunTimer();
    void resetSpeedStatistics();
    void drawDashboardWidgets();
    void updateDashboardWidgets(const CrsfTelemetryState &telemetry, uint16_t tx, bool txValid);
    ChannelResponseProfile &profileForChannel(uint8_t channel);
    const ChannelResponseProfile &profileForChannel(uint8_t channel) const;
    ChannelResponseProfile &throttleProfile();
    const ChannelResponseProfile &throttleProfile() const;
    ChannelResponseProfile &selectedResponseProfile();
    const ChannelResponseProfile &selectedResponseProfile() const;
    bool channelHasProportionalControl(uint8_t channel) const;
    int16_t proportionalInputValue(uint8_t channel,
                                   int16_t throttleFallback) const;
    void loadSettings();
    void saveSettings();
    void loadModelMemorySettings();
    void saveModelMemorySettings();
    void saveCurrentModelMemory();
    bool selectModelMemory(uint8_t slot);
    bool saveCalibration();
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
    void drawModelSelectPage();
    void drawBootSplash();
    void drawLogoSaver();
    void positionLogoSaver();
    void leaveLogoSaver();
    void refreshMainMenuFocus();
    void moveMainMenuFocus(int8_t direction);
    void activateMainMenuFocus();
    void beginPageFocus(bool registerStandardHeader = true);
    enum class PageControlEvent : int8_t {
        Decrease = -1,
        Activate = 0,
        Increase = 1,
    };
    void controlDesign(uint8_t item, PageControlEvent event);
    void controlPortraitBank(uint8_t item, PageControlEvent event);
    using PageControlHandler = void (TransmitterUi::*)(uint8_t,
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
    void controlModels(uint8_t item, PageControlEvent event);
    void controlTxBattery(uint8_t row, PageControlEvent event);
    void controlRxBattery(uint8_t row, PageControlEvent event);
    void controlDisplay(uint8_t row, PageControlEvent event);
    void controlColourScheme(uint8_t choice, PageControlEvent event);
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
    void controlEscProgramming(uint8_t item, PageControlEvent event);
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
    void drawRadioDiagnostics();
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
    void drawColourSchemePage();
    void drawDisplayRow(int row, int visibleRow);
    void drawFirmwareInfoPage();
    void drawElrsBindingPage();
    void drawElrsTxConfigPage();
    void controlElrsTxConfig(uint8_t item, PageControlEvent event);
    void handleElrsTxConfigTouch(int16_t x, int16_t y);
    void leaveElrsTxConfig();
    void drawEscProgrammingPage();
    void drawCalibrationPage();
    void drawCalibrationValues(uint16_t adc);
    void drawSettingRow(int row);
    void refreshSettingRowValue(uint8_t row);
    void scheduleSettingsSave();
    void drawLiveIndicator(int16_t outputPermille, uint16_t crsfValue);
    void handleTouch(int16_t x, int16_t y);
    void handleMainMenuTouch(int16_t x, int16_t y);
    void handleModelSelectTouch(int16_t x, int16_t y);
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
    void startCalibration(uint16_t adc, bool steering = false);
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

    ChannelResponseProfile channelProfiles_[CRSF_CHANNEL_COUNT] = {};
    static constexpr uint8_t kModelMemoryCount = 10;
    static constexpr uint8_t kModelNameCapacity = 13;
    DashboardLayout dashboardLayouts_[kModelMemoryCount];
    uint8_t widgetCell_ = 0;
    DashboardWidget widgetDraft_;
    struct DashboardWidgetView {
        lv_obj_t *panel = nullptr;
        lv_obj_t *basemap = nullptr;
        GpsMapViewport mapViewport;
        lv_obj_t *values[2] = {};
        lv_obj_t *bars[2] = {};
        lv_obj_t *arcs[2] = {};
        lv_obj_t *plot = nullptr;
        lv_obj_t *secondPlot = nullptr;
        lv_obj_t *marker = nullptr;
        lv_obj_t *detail = nullptr;
        lv_point_t points[48] = {};
        lv_point_t secondPoints[30] = {};
    };
    DashboardWidgetView widgetViews_[4];
    uint8_t tuneRole_ = 0; // steering, forward throttle, brake, selected AUX
    uint8_t tuneBank_ = 0, designBank_ = 0, setupBank_ = 0, sensorSelected_ = 0;
    uint8_t editorField_ = 0;
    int editorDraft_ = 0;
    int steeringEndpointDrafts_[2] = {100, 100}; // Left, right; applied together.
    uint8_t steeringEndpointSide_ = 0;
    int8_t steeringEndpointInputSide_ = 0;
    bool steeringEndpointEditor() const;
    void selectSteeringEndpoint(uint8_t side);
    void updateSteeringEndpointSelection();
    UiPage editorReturn_ = UiPage::Tune;
    bool editorFine_ = false;
    bool designTouchDispatch_ = false;
    bool runTimerRunning_ = false;
    uint32_t runTimerStartedMs_ = 0, runTimerElapsedMs_ = 0;
    OutputSafetyStatus designSafety_ = OutputSafetyStatus::Cut;
    CrsfTelemetryState designTelemetry_;
    uint16_t designTxVoltage_ = 0;
    bool designTxValid_ = false;
    lv_obj_t *designValues_[10] = {};
    lv_obj_t *designDetails_[8] = {};
    lv_obj_t *designBars_[4] = {};
    lv_obj_t *designStatus_ = nullptr;
    lv_obj_t *designLink_ = nullptr;
    lv_obj_t *designCurve_ = nullptr;
    lv_obj_t *designMarker_ = nullptr;
    GpsMapViewport designMapViewport_;
    lv_point_t designPoints_[33] = {};
    uint16_t channelHistory_[16][30] = {};
    int32_t sensorHistory_[6][30] = {};
    bool sensorHistoryValid_[6][30] = {};
    uint8_t sensorHistoryCount_ = 0;
    SpeedStatistics speedStatistics_;
    uint32_t nextSensorSampleMs_ = 0;
    int32_t gpsLat_[48] = {}, gpsLon_[48] = {};
    uint8_t gpsCount_ = 0;
    uint32_t gpsLastSampleMs_ = 0;
    bool gpsTrailVisible_ = true;
    uint8_t gpsZoom_ = 1;
    lv_point_t gpsPoints_[48] = {};
    uint8_t activeModelSlot_ = 0;
    uint8_t modelSelectBank_ = 0;
    uint8_t displaySetupBank_ = 0;
    char modelNames_[kModelMemoryCount][kModelNameCapacity] = {};
    char modelSelectStatus_[64] = {};
    ChannelRoleSettings channelRoleSettings_;
    ProportionalInput triggerInput_;
    ProportionalInput steeringInput_;
    bool calibratingSteering_ = false;
    UiPage calibrationReturnPage_ = UiPage::Channel2;
    SystemSettings systemSettings_;
    TxBatterySettings txBatterySettings_;
    RxBatterySettings rxBatterySettings_;
    DriveModeSettings driveModeSettings_;
    CrsfTxConfig txConfig_;
    uint32_t txConfigSequence_ = 0, txConfigRevision_ = 0;
    uint8_t txConfigFolder_ = 0, txConfigOffset_ = 0, txConfigSelected_ = 0;
    uint8_t txConfigDraft_ = 0;
    uint8_t txConfigRows_[4] = {};
    bool txConfigDraftDirty_ = false;
    char elrsBindPhrase_[52] = {};
    bool elrsKeyboardUppercase_ = false;
    bool elrsRequestPending_ = false;
    ElrsManagementRequest elrsPendingRequest_;
    bool elrsReceiverLinkAvailable_ = false;
    char elrsStatus_[80] = "Enter phrase, then SET TX";
    uint32_t lastTransmitterUidUpdateMs_ = 0;
    uint32_t lastReceiverUidUpdateMs_ = 0;
    bool displayReady_ = false;
    UiPage activePage_ = UiPage::BootSplash;
    UiPage logoSaverReturnPage_ = UiPage::Dashboard;
    uint32_t bootSplashStartedMs_ = 0;
    uint32_t nextLogoSaverMoveMs_ = 0;
    uint8_t logoSaverPosition_ = 0;
    bool startupPreflightComplete_ = false;
    uint32_t startupWarningMask_ = 0;
    uint32_t startupFaultMask_ = 0;
    UiPage throttleResponseReturnPage_ = UiPage::MainMenu;
    UiPage driveModesReturnPage_ = UiPage::Dashboard;
    uint8_t escProgrammingTab_ = 0;
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
    uint32_t lastInteractionMs_ = 0;
    uint16_t latestRawAdc_ = 0;
    uint16_t latestSteeringRawAdc_ = 0;
    bool touchWasDown_ = false;
    uint32_t touchDownStartedMs_ = 0;
    uint32_t lastTouchDurationMs_ = 0;
    uint32_t touchReleaseSinceMs_ = 0;
    uint32_t touchIgnoreUntilMs_ = 0;
    int16_t lastTouchX_ = 0;
    int16_t lastTouchY_ = 0;
    int16_t lastMarkerX_ = -1;
    uint16_t lastDisplayedCrsf_ = UINT16_MAX;
    uint16_t dashboardEnabledChannelMask_ = 0;
    uint16_t designRequested_[CRSF_CHANNEL_COUNT] = {};
    uint16_t dashboardChannelValues_[CRSF_CHANNEL_COUNT] = {};
    uint16_t lastDashboardChannelValues_[CRSF_CHANNEL_COUNT] = {};
    OutputSafetyStatus lastSafetyStatus_ = OutputSafetyStatus::ControlStale;
    bool lastLinkValid_ = false;
    bool lastReceiverVoltageValid_ = false;
    bool lastTxVoltageValid_ = false;
    bool dashboardPowerWarningActive_ = false;
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
    lv_obj_t *logoSaverLogo_ = nullptr;
    lv_obj_t *logoSaverDesignation_ = nullptr;
    int16_t logoSaverInputValues_[kPhysicalControlCount] = {};
    bool logoSaverInputPrimary_[kPhysicalControlCount] = {};
    bool logoSaverInputSecondary_[kPhysicalControlCount] = {};
    bool logoSaverInputFaulted_[kPhysicalControlCount] = {};
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
    bool pageFocusVisible_ = false;
    uint8_t pageFocusIndex_ = 0;
    bool pageStandardHeaderPending_ = false;
    lv_obj_t *pageFocusIndicator_ = nullptr;
    enum class DashboardChannelWidgetKind : uint8_t {
        None,
        HorizontalProportional,
        VerticalProportional,
        ToggleSwitch,
    };
    lv_obj_t *dashboardChannelBars_[CRSF_CHANNEL_COUNT] = {};
    lv_obj_t *dashboardChannelMarkers_[CRSF_CHANNEL_COUNT] = {};
    lv_obj_t *dashboardChannelValueLabels_[CRSF_CHANNEL_COUNT] = {};
    DashboardChannelWidgetKind dashboardChannelWidgetKinds_[CRSF_CHANNEL_COUNT] = {};
    lv_obj_t *dashboardSafetyPanel_ = nullptr;
    lv_obj_t *dashboardSafetyLabel_ = nullptr;
    lv_obj_t *dashboardSafetyIcon_ = nullptr;
    lv_obj_t *dashboardSafetyLockBody_ = nullptr;
    lv_obj_t *dashboardSafetyLockShackle_ = nullptr;
    lv_obj_t *dashboardHeader_ = nullptr;
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
    lv_obj_t *calibrationStatusLabel_ = nullptr;
    lv_obj_t *calibrationMinimumLabel_ = nullptr;
    lv_obj_t *calibrationLiveLabel_ = nullptr;
    lv_obj_t *calibrationMaximumLabel_ = nullptr;
    PhysicalInputState physicalInputStates_[kPhysicalControlCount] = {};
    lv_obj_t *inputMonitorStateLabels_[5] = {};
    lv_obj_t *inputMonitorEventLabels_[5] = {};
};
