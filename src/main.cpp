#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/uart.h>
#include <hal/uart_ll.h>
#include <soc/gpio_sig_map.h>
#include <esp_system.h>

#include "channel2_ui.h"
#include "config.h"
#include "crsf_telemetry.h"

namespace {

HardwareSerial rangerSerial(1);
HardwareSerial rangerRxSerial(2);

constexpr uart_port_t RANGER_UART = UART_NUM_1;
constexpr uint8_t CRSF_SYNC_BYTE = 0xC8;
constexpr uint8_t CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16;
constexpr uint16_t CRSF_CHANNEL_MIN = 172;
constexpr uint16_t CRSF_CHANNEL_MID = 992;
constexpr uint16_t CRSF_CHANNEL_MAX = 1811;
constexpr uint16_t CRSF_CHANNEL_ABSOLUTE_MIN = 0;
constexpr uint16_t CRSF_CHANNEL_ABSOLUTE_MAX = 2047;
constexpr size_t CRSF_CHANNEL_COUNT = 16;
constexpr size_t CRSF_PAYLOAD_BYTES = 22;
constexpr size_t CRSF_FRAME_BYTES = 26;

uint16_t filteredAdc = THROTTLE_ADC_CENTER;
uint32_t filteredTxBatteryAdcMv = 0;
uint32_t nextTxBatterySampleMs = 0;
bool throttleArmed = false;
bool wasCalibrating = false;
uint32_t neutralSinceMs = 0;
uint32_t nextFrameUs = 0;
uint32_t nextLogMs = 0;
Channel2Ui channel2Ui;
CrsfTelemetryParser telemetryParser;
volatile uint32_t crsfBusEdges = 0;
volatile uint16_t sharedTransmittedThrottle = CRSF_CHANNEL_MID;
volatile uint8_t sharedThrottleChannel = 2;
CrsfTelemetryState telemetrySnapshot;
portMUX_TYPE telemetryMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t crsfTaskHandle = nullptr;

void IRAM_ATTR countCrsfBusEdge()
{
    ++crsfBusEdges;
}

uint8_t crc8DvbS2(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    while (length-- != 0) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80U) ? static_cast<uint8_t>((crc << 1U) ^ 0xD5U)
                                : static_cast<uint8_t>(crc << 1U);
        }
    }
    return crc;
}

uint16_t mapPermilleToCrsf(int16_t value)
{
    int32_t mapped;
    if (value <= 0) {
        mapped = CRSF_CHANNEL_MID +
                 static_cast<int32_t>(value) *
                 (CRSF_CHANNEL_MID - CRSF_CHANNEL_MIN) / 1000;
    } else {
        mapped = CRSF_CHANNEL_MID +
                 static_cast<int32_t>(value) *
                 (CRSF_CHANNEL_MAX - CRSF_CHANNEL_MID) / 1000;
    }
    return static_cast<uint16_t>(constrain(mapped,
                                           CRSF_CHANNEL_ABSOLUTE_MIN,
                                           CRSF_CHANNEL_ABSOLUTE_MAX));
}

void updateNeutralInterlock(uint16_t adc)
{
    if (throttleArmed) {
        return;
    }

    const int delta = static_cast<int>(adc) -
                      static_cast<int>(channel2Ui.throttleCentreAdc());
    if (abs(delta) <= ARM_NEUTRAL_ADC_WINDOW) {
        if (neutralSinceMs == 0) {
            neutralSinceMs = millis();
        } else if (millis() - neutralSinceMs >= ARM_NEUTRAL_TIME_MS) {
            throttleArmed = true;
            Serial.println("ARMED: throttle input is now being transmitted");
        }
    } else {
        neutralSinceMs = 0;
    }
}

void updateTriggerSafety(uint16_t adc, bool calibrationActive)
{
    // Calibration is handled by ProportionalInput. This is only the output
    // safety policy: never arm during calibration and require neutral again
    // after calibration finishes.
    if (wasCalibrating && !calibrationActive) {
        throttleArmed = false;
        neutralSinceMs = 0;
    }
    wasCalibrating = calibrationActive;
    if (!calibrationActive) updateNeutralInterlock(adc);
}

void packChannels(const uint16_t channels[CRSF_CHANNEL_COUNT], uint8_t payload[CRSF_PAYLOAD_BYTES])
{
    memset(payload, 0, CRSF_PAYLOAD_BYTES);
    uint32_t accumulator = 0;
    uint8_t bitsInAccumulator = 0;
    size_t outputIndex = 0;

    for (size_t channel = 0; channel < CRSF_CHANNEL_COUNT; ++channel) {
        accumulator |= static_cast<uint32_t>(channels[channel] & 0x07FFU)
                       << bitsInAccumulator;
        bitsInAccumulator += 11;

        while (bitsInAccumulator >= 8) {
            payload[outputIndex++] = static_cast<uint8_t>(accumulator & 0xFFU);
            accumulator >>= 8;
            bitsInAccumulator -= 8;
        }
    }
}

void buildChannelsFrame(uint16_t throttle, uint8_t throttleChannel,
                        uint8_t frame[CRSF_FRAME_BYTES])
{
    uint16_t channels[CRSF_CHANNEL_COUNT];
    for (uint16_t &channel : channels) {
        channel = CRSF_CHANNEL_MID;
    }

    throttleChannel = constrain(throttleChannel, 1, 16);
    channels[throttleChannel - 1] = throttle;

    frame[0] = CRSF_SYNC_BYTE;
    frame[1] = 24; // type + 22-byte channel payload + CRC
    frame[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    packChannels(channels, &frame[3]);
    frame[25] = crc8DvbS2(&frame[2], 23); // type + payload
}

void setCrsfBusReceive()
{
    // The Saleae capture of the actual Nano-adapter bus decodes as ordinary
    // non-inverted 400 kbaud UART (idle high, falling start bit). Release the
    // output driver and attach that physical polarity directly to UART RX.
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_INPUT);
    gpio_matrix_in(CRSF_SIGNAL_PIN, U2RXD_IN_IDX, true);
    gpio_pullup_en(CRSF_SIGNAL_PIN);
    gpio_pulldown_dis(CRSF_SIGNAL_PIN);
}

void setCrsfBusTransmit()
{
    // UART2 remains attached as a passive listener. It receives a clean echo
    // of our outbound frame as well as the Nano's replies; the parser safely
    // ignores outbound frame types. Keeping it attached avoids losing the
    // beginning of a fast Nano response during a GPIO-matrix handover.
    gpio_set_pull_mode(CRSF_SIGNAL_PIN, GPIO_FLOATING);
    gpio_set_level(CRSF_SIGNAL_PIN, 0);
    gpio_set_direction(CRSF_SIGNAL_PIN, GPIO_MODE_OUTPUT);
    gpio_matrix_out(CRSF_SIGNAL_PIN, U1TXD_OUT_IDX, true, false);
}

void transmitCrsfFrame(const uint8_t *frame, size_t length)
{
    setCrsfBusTransmit();
    rangerSerial.write(frame, length);
    // Poll the hardware directly. Waiting through the UART driver's event path
    // leaves the line driven for too long and can hide the Nano's immediate
    // half-duplex response.
    while (!uart_ll_is_tx_idle(UART_LL_GET_HW(RANGER_UART))) {
    }
    setCrsfBusReceive();
}

void sendChannelsFrame(uint16_t throttle, uint8_t throttleChannel)
{
    uint8_t frame[CRSF_FRAME_BYTES];
    buildChannelsFrame(throttle, throttleChannel, frame);
    transmitCrsfFrame(frame, sizeof(frame));
}

void readReturnedTelemetry()
{
    // This runs in the dedicated CRSF task. A generous bound prevents a bad
    // electrical stream from monopolising Core 0 while still draining normal
    // channel echoes and module telemetry faster than they can arrive.
    uint16_t budget = 2048;
    while (budget-- > 0 && rangerRxSerial.available() > 0) {
        const uint8_t value = static_cast<uint8_t>(rangerRxSerial.read());
        telemetryParser.feed(value);
    }
    telemetryParser.updateFreshness();
}

void publishTelemetrySnapshot()
{
    portENTER_CRITICAL(&telemetryMux);
    telemetrySnapshot = telemetryParser.state();
    portEXIT_CRITICAL(&telemetryMux);
}

CrsfTelemetryState readTelemetrySnapshot()
{
    CrsfTelemetryState snapshot;
    portENTER_CRITICAL(&telemetryMux);
    snapshot = telemetrySnapshot;
    portEXIT_CRITICAL(&telemetryMux);
    return snapshot;
}

void crsfTask(void *)
{
    nextFrameUs = micros();
    uint32_t nextSnapshotMs = millis();
    for (;;) {
        const uint32_t nowUs = micros();
        if (static_cast<int32_t>(nowUs - nextFrameUs) >= 0) {
            // Do not send a burst after a scheduling hiccup; resume at the
            // current 250 Hz phase with the latest requested channel value.
            nextFrameUs = nowUs + CRSF_FRAME_PERIOD_US;
            const uint16_t throttle = sharedTransmittedThrottle;
            const uint8_t channel = sharedThrottleChannel;
            sendChannelsFrame(throttle, channel);
        }
        readReturnedTelemetry();
        const uint32_t nowMs = millis();
        if (static_cast<int32_t>(nowMs - nextSnapshotMs) >= 0) {
            nextSnapshotMs = nowMs + 20;
            publishTelemetrySnapshot();
        }
        vTaskDelay(1);
    }
}

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(700);
    Serial.println();
    Serial.println("JR R1 native CRSF throttle test");
    Serial.printf("BOOT: reset_reason=%u free_heap=%u bytes\n",
                  static_cast<unsigned>(esp_reset_reason()),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.println("WARNING: disconnect the motor / raise the wheels for first test");

    analogReadResolution(12);
    analogSetPinAttenuation(THROTTLE_ADC_PIN, ADC_11db);
    analogSetPinAttenuation(TX_BATTERY_ADC_PIN, ADC_11db);

    // UART1 drives the one-wire CRSF bus. UART2 passively samples the same pin;
    // separating TX and RX avoids remapping a live UART input at every 250 Hz
    // direction change.
    rangerSerial.begin(CRSF_BAUD, SERIAL_8N1, -1, CRSF_SIGNAL_PIN, false);
    rangerSerial.setTimeout(0);
    rangerRxSerial.setRxBufferSize(2048);
    rangerRxSerial.begin(CRSF_BAUD, SERIAL_8N1, CRSF_SIGNAL_PIN, -1, false);
    rangerRxSerial.setTimeout(0);
    setCrsfBusReceive();
    attachInterrupt(digitalPinToInterrupt(CRSF_SIGNAL_PIN), countCrsfBusEdge, CHANGE);

    xTaskCreatePinnedToCore(crsfTask, "crsf-link", 4096, nullptr, 4,
                            &crsfTaskHandle, 0);

    channel2Ui.begin();
    filteredAdc = analogRead(THROTTLE_ADC_PIN);
    nextLogMs = millis();
}

void loop()
{
    const uint16_t sample = analogRead(THROTTLE_ADC_PIN);
    // Simple 1/8 IIR smoothing; sufficient for the first trigger test.
    filteredAdc = static_cast<uint16_t>((filteredAdc * 7U + sample) / 8U);
    const bool calibrating = channel2Ui.calibrationActive();
    updateTriggerSafety(filteredAdc, calibrating);

    const int16_t rawThrottle = channel2Ui.mapAdcToPermille(filteredAdc);
    const int16_t configuredThrottle = channel2Ui.applySettings(rawThrottle);
    const uint16_t requestedThrottle = mapPermilleToCrsf(configuredThrottle);
    const uint16_t transmittedThrottle =
        (throttleArmed && !calibrating) ? requestedThrottle : CRSF_CHANNEL_MID;
    sharedTransmittedThrottle = transmittedThrottle;
    const CrsfTelemetryState telemetry = readTelemetrySnapshot();

    uint16_t txVoltageDecivolts = 0;
    bool txVoltageValid = false;
    const uint32_t nowMs = millis();
    if (channel2Ui.txVoltageSenseEnabled()) {
        if (static_cast<int32_t>(nowMs - nextTxBatterySampleMs) >= 0) {
            nextTxBatterySampleMs = nowMs + 20;
            const uint32_t sampleMv = analogReadMilliVolts(TX_BATTERY_ADC_PIN);
            filteredTxBatteryAdcMv = filteredTxBatteryAdcMv == 0
                ? sampleMv
                : (filteredTxBatteryAdcMv * 7U + sampleMv) / 8U;
        }
        const uint32_t packMv = filteredTxBatteryAdcMv *
            (TX_BATTERY_DIVIDER_TOP_OHMS + TX_BATTERY_DIVIDER_BOTTOM_OHMS) /
            TX_BATTERY_DIVIDER_BOTTOM_OHMS;
        txVoltageValid = packMv >= 2500;
        txVoltageDecivolts = static_cast<uint16_t>((packMv + 50U) / 100U);
    } else {
        filteredTxBatteryAdcMv = 0;
    }
    channel2Ui.update(filteredAdc, rawThrottle,
                      throttleArmed ? configuredThrottle : 0,
                      transmittedThrottle, throttleArmed,
                      telemetry, txVoltageDecivolts, txVoltageValid);
    if (channel2Ui.consumeChannelRolesChanged()) {
        // Publish neutral before changing the channel number. If the CRSF task
        // runs between these writes it can only send the previous channel at
        // neutral, never the previous live value on the newly selected output.
        sharedTransmittedThrottle = CRSF_CHANNEL_MID;
        sharedThrottleChannel = channel2Ui.throttleChannel();
        throttleArmed = false;
        neutralSinceMs = 0;
        Serial.printf("SAFE: throttle role changed to CH%u; release trigger to re-arm\n",
                      channel2Ui.throttleChannel());
    } else {
        sharedThrottleChannel = channel2Ui.throttleChannel();
    }

    if (static_cast<int32_t>(nowMs - nextLogMs) >= 0) {
        nextLogMs += USB_LOG_PERIOD_MS;
        const int neutralDelta = static_cast<int>(filteredAdc) -
                                 static_cast<int>(channel2Ui.throttleCentreAdc());
        Serial.printf("adc=%4u ctr=%4u delta=%+4d cal=%s neutralFor=%lums THR=CH%u requested=%4u sent=%4u armed=%s link=%s LQ=%u RSSI=%d TX=%s%u.%uV RX=%s%u.%uV edges=%lu bytes=%lu frames=%lu crc=%lu\n",
                      filteredAdc,
                      channel2Ui.throttleCentreAdc(),
                      neutralDelta,
                      channel2Ui.calibrationActive() ? "yes" : "no",
                      static_cast<unsigned long>(neutralSinceMs == 0 ? 0 : nowMs - neutralSinceMs),
                      channel2Ui.throttleChannel(),
                      requestedThrottle,
                      transmittedThrottle,
                      throttleArmed ? "yes" : "NO",
                      telemetry.linkValid ? "yes" : "no",
                      telemetry.uplinkLinkQuality,
                      telemetry.uplinkRssiDbm,
                      txVoltageValid ? "" : "--/",
                      txVoltageDecivolts / 10,
                      txVoltageDecivolts % 10,
                      telemetry.receiverVoltageValid ? "" : "--/",
                      telemetry.receiverVoltageDecivolts / 10,
                      telemetry.receiverVoltageDecivolts % 10,
                      static_cast<unsigned long>(crsfBusEdges),
                      static_cast<unsigned long>(telemetry.rawBytes),
                      static_cast<unsigned long>(telemetry.validFrames),
                      static_cast<unsigned long>(telemetry.crcErrors));
    }
}
