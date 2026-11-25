#include "gps_widget_view.h"
#include <cassert>
#include <cstdio>

using View = GpsWidgetView::View;
using Status = GpsWidgetView::Status;

static void expect(GpsWidgetView::Result result, View view, Status status) {
    assert(result.view == view);
    assert(result.status == status);
}

static GpsWidgetView::Input position(uint32_t stamp) {
    GpsWidgetView::Input input;
    input.positionUsable = input.positionFieldsFresh = true;
    input.satellitesKnownFresh = input.speedFresh = true;
    input.satellites = 9;
    input.positionStamp = stamp;
    return input;
}

static GpsWidgetView::Input speed() {
    GpsWidgetView::Input input;
    input.speedFresh = true;
    return input;
}

int main() {
    GpsWidgetView policy;
    GpsWidgetView::Input none;
    expect(policy.result(), View::Map, Status::Waiting);
    expect(policy.update(none, 0), View::Map, Status::Waiting);
    expect(policy.update(none, 100000), View::Map, Status::Waiting);

    // Fresh zero speed is represented by the same validity as moving speed.
    // No position/satellite fields means speed data only, never a mode claim.
    expect(policy.update(speed(), 100001), View::Speed, Status::SpeedOnly);
    expect(policy.update(none, 103002), View::Speed, Status::Unavailable);
    expect(policy.update(speed(), 104000), View::Speed, Status::SpeedOnly);

    // Satellite presence, including zero, is positive acquisition information.
    GpsWidgetView::Input acquiring = speed();
    acquiring.satellitesKnownFresh = true;
    for (uint8_t count = 0; count < 4; ++count) {
        acquiring.satellites = count;
        expect(policy.update(acquiring, 105000 + count), View::Map, Status::Acquiring);
    }
    acquiring.satellites = 4;
    expect(policy.update(acquiring, 106000), View::Map, Status::Waiting);
    acquiring.satellites = 20;
    acquiring.speedFresh = false;
    expect(policy.update(acquiring, 106100), View::Map, Status::Waiting);
    expect(policy.update(none, 110000), View::Map, Status::Unavailable);

    // Fresh coordinates plus zero can also be a legacy bridge's unknown count.
    // The selector retains Map; UI labels it unconfirmed, never a confirmed fix.
    auto legacy = position(110001);legacy.satellites = 0;
    expect(policy.update(legacy, 110001), View::Map, Status::Position);

    // An unknown satellite field is not a reported count of zero.
    policy.reset();
    expect(policy.update(speed(), 0), View::Speed, Status::SpeedOnly);
    auto fields = speed();
    fields.positionFieldsFresh = true;
    expect(policy.update(fields, 1), View::Map, Status::Waiting);
    fields.satellitesKnownFresh = true;
    fields.satellites = 0;
    expect(policy.update(fields, 2), View::Map, Status::Acquiring);
    fields.satellites = 3;
    expect(policy.update(fields, 3), View::Map, Status::Acquiring);
    fields.satellites = 4;
    expect(policy.update(fields, 4), View::Map, Status::Waiting);

    // Global/no-fix-like fields stay on the map even when fresh speed exists.
    for (uint32_t now = 1000; now <= 6000; now += 1000) {
        fields.satellites = 2;
        expect(policy.update(fields, now), View::Map, Status::Acquiring);
    }
    expect(policy.update(position(7000), 7000), View::Map, Status::Position);
    expect(policy.update(speed(), 9999), View::Map, Status::Waiting);
    expect(policy.update(speed(), 10000), View::Map, Status::Waiting);
    expect(policy.update(speed(), 10001), View::Speed, Status::SpeedOnly);
    expect(policy.update(none, 14000), View::Speed, Status::Unavailable);
    expect(policy.update(position(15000), 15000), View::Map, Status::Position);
    expect(policy.update(none, 18001), View::Map, Status::Unavailable);

    // Repeated UI reads of one usable packet must not extend the grace period.
    policy.reset();
    expect(policy.update(position(20000), 20000), View::Map, Status::Position);
    expect(policy.update(position(20000), 22000), View::Map, Status::Position);
    expect(policy.update(speed(), 23000), View::Map, Status::Waiting);
    expect(policy.update(speed(), 23001), View::Speed, Status::SpeedOnly);

    // A timestamp of zero is valid; rollover uses unsigned elapsed time.
    policy.reset();
    expect(policy.update(position(0), 0), View::Map, Status::Position);
    expect(policy.update(speed(), 3000), View::Map, Status::Waiting);
    expect(policy.update(speed(), 3001), View::Speed, Status::SpeedOnly);
    policy.reset();
    const uint32_t stamp = UINT32_MAX - 1000;
    expect(policy.update(position(stamp), stamp), View::Map, Status::Position);
    expect(policy.update(speed(), stamp + uint32_t(2999)), View::Map, Status::Waiting);
    expect(policy.update(speed(), stamp + uint32_t(3000)), View::Map, Status::Waiting);
    expect(policy.update(speed(), stamp + uint32_t(3001)), View::Speed, Status::SpeedOnly);
    expect(policy.update(position(3000), 3000), View::Map, Status::Position);

    // Reset drops previous presentation and grace; no previous-model state leaks.
    policy.reset();
    expect(policy.result(), View::Map, Status::Waiting);
    expect(policy.update(none, 100), View::Map, Status::Waiting);
    expect(policy.update(speed(), 101), View::Speed, Status::SpeedOnly);
    puts("GPS widget view: availability, acquisition, grace, loss, reset and rollover PASS");
}
