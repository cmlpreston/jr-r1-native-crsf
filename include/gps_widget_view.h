#pragma once
#include <stdint.h>

// Chooses a dashboard presentation from fresh data, not the GPS sensor's mode.
// The caller owns telemetry validation/freshness; this class never changes it.
class GpsWidgetView {
public:
    static constexpr uint32_t PositionGraceMs = 3000;
    enum class View : uint8_t { Map, Speed };
    enum class Status : uint8_t { Position, Waiting, Acquiring, SpeedOnly, Unavailable };
    struct Input {
        bool positionUsable = false;
        bool positionFieldsFresh = false;
        bool satellitesKnownFresh = false;
        uint8_t satellites = 0;
        bool speedFresh = false; // A measured zero is fresh speed too.
        uint32_t positionStamp = 0; // Actual last usable telemetry timestamp.
    };
    struct Result {
        View view = View::Map;
        Status status = Status::Waiting;
    };

    void reset() {
        result_.view = View::Map;
        result_.status = Status::Waiting;
        havePosition_ = seenData_ = false;
        lastPositionMs_ = 0;
    }

    Result update(const Input &input, uint32_t now) {
        const bool anyFresh = input.positionUsable || input.positionFieldsFresh ||
            input.satellitesKnownFresh || input.speedFresh;
        if (anyFresh) seenData_ = true;

        if (input.positionUsable) {
            havePosition_ = true;
            lastPositionMs_ = input.positionStamp;
            result_.view = View::Map;
            result_.status = Status::Position;
        } else if (input.positionFieldsFresh || input.satellitesKnownFresh) {
            // Known zero satellites differs from an absent satellite field.
            // Position-shaped data also prevents an acquisition phase being
            // mistaken for evidence that the sensor changed to speed mode.
            result_.view = View::Map;
            result_.status = input.satellitesKnownFresh && input.satellites < 4 ?
                Status::Acquiring : Status::Waiting;
        } else if (input.speedFresh) {
            const bool holdMap = havePosition_ &&
                uint32_t(now - lastPositionMs_) <= PositionGraceMs;
            result_.view = holdMap ? View::Map : View::Speed;
            result_.status = holdMap ? Status::Waiting : Status::SpeedOnly;
        } else {
            // Packet loss must not alternate the layout or invent zero speed.
            result_.status = seenData_ ? Status::Unavailable : Status::Waiting;
        }
        return result_;
    }

    Result result() const { return result_; }

private:
    Result result_;
    uint32_t lastPositionMs_ = 0;
    bool havePosition_ = false, seenData_ = false;
};
