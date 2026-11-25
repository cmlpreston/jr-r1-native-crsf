// Stable dashboard source IDs index the same metadata in all sensor views.
namespace {
struct SensorReading {
    int32_t value;
    uint32_t stamp;
    bool valid;
};

enum class SensorFormat { Integer, WholeDegrees, Tenths, Thousandths };

struct SensorDescriptor {
    const char *name;
    const char *unit;
    SensorFormat format;
    SensorReading (*read)(const CrsfTelemetryState &, uint32_t);
};

template <unsigned Cell>
SensorReading cellReading(const CrsfTelemetryState &t, uint32_t)
{
    return {t.vehicleCellsMv[Cell], t.vehicleCellsUpdatedMs, t.vehicleCellsValid && Cell < t.vehicleCellCount};
}

const SensorDescriptor sensorDescriptors[] = {
    {"SPEED", "km/h", SensorFormat::Tenths, [](const CrsfTelemetryState &t, uint32_t now) {
        return SensorReading{t.groundSpeedDeciKmh, t.gpsSpeedUpdatedMs, gpsSpeedAvailable(t, now)};
    }},
    {"TEMPERATURE", "C", SensorFormat::WholeDegrees, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.temperatureDeciC, t.temperatureUpdatedMs, t.temperatureValid};
    }},
    {"RPM", "rpm", SensorFormat::Integer, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.rpm, t.rpmUpdatedMs, t.rpmValid};
    }},
    {"RX VOLTAGE", "V", SensorFormat::Tenths, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.receiverVoltageDecivolts, t.batteryUpdatedMs, t.receiverVoltageValid};
    }},
    {"LINK QUALITY", "%", SensorFormat::Integer, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.uplinkLinkQuality, t.linkUpdatedMs, t.linkValid};
    }},
    {"SATELLITES", "sats", SensorFormat::Integer, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.satellites, t.gpsSatellitesUpdatedMs, t.gpsSatellitesValid && t.satellites > 0};
    }},
    {"DRIVE PACK", "V", SensorFormat::Thousandths, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.vehiclePackMv, t.vehicleCellsUpdatedMs, t.vehicleCellsValid};
    }},
    {"LOWEST CELL", "V", SensorFormat::Thousandths, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.vehicleMinimumCellMv, t.vehicleCellsUpdatedMs, t.vehicleCellsValid};
    }},
    {"CELL 1", "V", SensorFormat::Thousandths, cellReading<0>},
    {"CELL 2", "V", SensorFormat::Thousandths, cellReading<1>},
    {"CELL 3", "V", SensorFormat::Thousandths, cellReading<2>},
    {"CELL 4", "V", SensorFormat::Thousandths, cellReading<3>},
    {"FAS VOLTAGE", "V", SensorFormat::Thousandths, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.fasVoltageMv, t.fasVoltageUpdatedMs, t.fasVoltageValid};
    }},
    {"FAS CURRENT", "A", SensorFormat::Tenths, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.fasCurrentDeciA, t.fasCurrentUpdatedMs, t.fasCurrentValid};
    }},
    {"ERS PACK", "V", SensorFormat::Tenths, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.ersVoltageDeciV, t.ersBatteryUpdatedMs, t.ersBatteryValid};
    }},
    {"ERS CURRENT", "A", SensorFormat::Tenths, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.ersCurrentDeciA, t.ersBatteryUpdatedMs, t.ersBatteryValid};
    }},
    {"ERS USED", "mAh", SensorFormat::Integer, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{int32_t(t.ersCapacityMah), t.ersBatteryUpdatedMs, t.ersBatteryValid};
    }},
    {"ERS REMAINING", "%", SensorFormat::Integer, [](const CrsfTelemetryState &t, uint32_t) {
        return SensorReading{t.ersRemainingPercent, t.ersBatteryUpdatedMs, t.ersBatteryValid && t.ersRemainingValid};
    }},
};
static_assert(sizeof(sensorDescriptors) / sizeof(*sensorDescriptors) == DashboardSensorCount,
              "Sensor descriptors must cover persisted sources");

SensorReading sensorReading(const CrsfTelemetryState &telemetry, uint8_t source, uint32_t now)
{
    if (source >= DashboardSensorCount) return {0, 0, false};
    return sensorDescriptors[source].read(telemetry, now);
}

void sensorText(uint8_t source, int32_t value, char *text, size_t capacity)
{
    const auto format = sensorDescriptors[source].format;
    if (format == SensorFormat::WholeDegrees) value /= 10; // Whole-degree source.
    if (format == SensorFormat::Integer || format == SensorFormat::WholeDegrees) {
        snprintf(text, capacity, "%ld", static_cast<long>(value));
    } else {
        const bool milli = format == SensorFormat::Thousandths;
        const unsigned long scale = milli ? 1000 : 10;
        const auto magnitude = static_cast<unsigned long>(value < 0 ? -int64_t(value) : value);
        snprintf(text, capacity, "%s%lu.%0*lu", value < 0 ? "-" : "",
                 magnitude / scale, milli ? 3 : 1, magnitude % scale);
    }
}
}
