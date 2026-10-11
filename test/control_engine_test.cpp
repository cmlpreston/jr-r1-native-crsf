#include "control_engine.h"
#include "control_output_mailbox.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

static ControlConfigurationSnapshot configuration() {
    ControlConfigurationSnapshot c;
    c.revision = 1; c.armRequested = true;
    for (auto *p : {&c.throttleInput, &c.steeringInput}) {
        p->calibration = {1000, 2000, 3000};
        p->validMinimumAdc = 100; p->validMaximumAdc = 4090;
        p->filterShift = 0; p->deadbandAdc = 80;
    }
    return c;
}
static ControlCycleInput input(uint32_t now) {
    ControlCycleInput i;
    i.nowMs = now; i.throttleAdc = i.steeringAdc = 2000;
    i.startupComplete = i.linkValid = true; i.linkQuality = 100;
    return i;
}
static void qualify(ControlEngine &engine, ControlConfigurationSnapshot &c, uint32_t start) {
    assert(engine.update(c, input(start)).safety == OutputSafetyStatus::LinkRequired);
    assert(engine.update(c, input(start+500)).safety == OutputSafetyStatus::NeutralRequired);
    assert(engine.update(c, input(start+1499)).safety == OutputSafetyStatus::NeutralRequired);
    assert(engine.update(c, input(start+1500)).safety == OutputSafetyStatus::Armed);
}
static void testSafety() {
    auto c = configuration(); ControlEngine e;
    qualify(e, c, 0); // zero timestamps are valid, not unset sentinels
    auto i = input(1504); i.linkValid = false; i.throttleAdc = 3000;
    auto r = e.update(c, i);
    assert(r.safety == OutputSafetyStatus::Armed);
    assert(permilleFromCrsfValue(r.outputs.requested(2)) > 0);
    i.steeringAdc = 4095;
    r = e.update(c, i);
    assert(r.safety == OutputSafetyStatus::InputFault);
    assert(r.outputs.requested(1) == r.outputs.safe(1));
    i = input(1600); i.resetSequence = 1;
    assert(e.update(c, i).safety == OutputSafetyStatus::LinkRequired);
    c.armRequested = false; ++c.revision;
    assert(e.update(c, i).safety == OutputSafetyStatus::Cut);
    c.armRequested = true; ++c.revision;
    qualify(e, c, 2000);
    c.calibrationActive = true; ++c.revision;
    assert(e.update(c, input(3504)).safety == OutputSafetyStatus::Calibration);
    c.calibrationActive = false; c.armRequested = false; ++c.revision;
    e.update(c, input(4000)); c.armRequested = true; ++c.revision;
    qualify(e, c, 0xfffffc00U); // dwell survives millisecond wrap
}
static void testStructuralEditsAndLiveTrim() {
    auto c = configuration(); ControlEngine e; qualify(e, c, 10);
    c.channelParameters[1].trimPermille = 50; ++c.revision;
    assert(e.update(c, input(1514)).safety == OutputSafetyStatus::Armed);
    c.channelProfiles[1].reversed = true; ++c.revision;
    assert(e.update(c, input(1518)).safety == OutputSafetyStatus::Cut);
    assert(e.update(c, input(9999)).safety == OutputSafetyStatus::Cut);
    c.armRequested = false; ++c.revision;
    e.update(c, input(10000)); c.armRequested = true; ++c.revision;
    qualify(e, c, 10004);
    c.channelParameters[1].subtrimPermille = 10; ++c.revision;
    assert(e.update(c, input(11508)).safety == OutputSafetyStatus::Cut);
    c.modelStorageValid = false; ++c.revision;
    assert(e.update(c, input(11512)).safety == OutputSafetyStatus::ConfigurationFault);
}
static void testArmingVetoes() {
    auto c = configuration(); ControlEngine e;
    auto i = input(0); i.startupComplete = false;
    assert(e.update(c, i).safety == OutputSafetyStatus::StartupWarning);
    i = input(1000); i.linkQuality = 49;
    assert(e.update(c, i).safety == OutputSafetyStatus::LinkRequired);
    i = input(2000);
    assert(e.update(c, i).safety == OutputSafetyStatus::LinkRequired);
    i = input(2500); i.throttleAdc = 2500;
    assert(e.update(c, i).safety == OutputSafetyStatus::NeutralRequired);
    i = input(3000);
    assert(e.update(c, i).safety == OutputSafetyStatus::NeutralRequired);
    i = input(3999); i.throttleAdc = 2500;
    assert(e.update(c, i).safety == OutputSafetyStatus::NeutralRequired);
    i = input(4000);
    assert(e.update(c, i).safety == OutputSafetyStatus::NeutralRequired);
    i = input(4999);
    assert(e.update(c, i).safety == OutputSafetyStatus::NeutralRequired);
    i = input(5000);
    assert(e.update(c, i).safety == OutputSafetyStatus::Armed);
    i.throttleAdc = 4095;
    assert(e.update(c, i).safety == OutputSafetyStatus::InputFault);
}
static void testResetPublicationOrder() {
    ControlOutputMailbox mailbox; CrsfChannelOutputModel model;
    model.configure(2, crsfValueFromPermille(1000), CRSF_CHANNEL_MID);
    auto old = mailbox.commands();
    assert(mailbox.publish(model, OutputSafetyStatus::Armed, 100, old.resetSequence));
    mailbox.requestReset();
    assert(!mailbox.publish(model, OutputSafetyStatus::Armed, 104, old.resetSequence));
    assert(mailbox.outputs().controlSafetyStatus == OutputSafetyStatus::Cut);
    assert(mailbox.outputs().updatedUs == 100);
    auto latest = mailbox.commands();
    assert(mailbox.publish(model, OutputSafetyStatus::NeutralRequired, 108, latest.resetSequence));
    mailbox.requestReset(); mailbox.requestReset();
    assert(!mailbox.publish(model, OutputSafetyStatus::Armed, 112, latest.resetSequence));
    mailbox.completeStartup(); assert(mailbox.commands().startupPreflightComplete);
    assert(mailbox.publish(model, OutputSafetyStatus::Cut, 116, mailbox.commands().resetSequence));
    mailbox.requireConfiguration();
    latest = mailbox.commands();
    assert(!mailbox.publish(model, OutputSafetyStatus::Armed, 120, latest.resetSequence));
    mailbox.acceptConfiguration(1);
    assert(!mailbox.publish(model, OutputSafetyStatus::Armed, 124, latest.resetSequence, 0));
    assert(mailbox.publish(model, OutputSafetyStatus::Cut, 128, latest.resetSequence, 1));
}
static void testMixCompilation() {
    auto c = configuration();
    c.mixing[2].rules[0].enabled = 1; c.mixing[2].rules[0].source = 22;
    c.mixing[3].rules[0].enabled = 1; c.mixing[3].rules[0].source = 19;
    const auto plan = compileConfiguredMixing(c); assert(plan.valid && plan.count == 16);
    unsigned position[16];
    for (unsigned n = 0; n < 16; ++n) position[plan.order[n]] = n;
    assert(position[0] < position[3] && position[3] < position[2]);
    MixInputSnapshot inputs; inputs.valid = 3; inputs.values[0] = 700;
    ChannelSlewRuntime a,b; ChannelMixRuntime ma,mb; CrsfChannelOutputModel oa,ob;
    processConfiguredMixedOutputs(c, oa, inputs, ma, a, 100, true);
    processConfiguredMixedOutputs(c, ob, inputs, mb, b, 100, true, &plan);
    for (uint8_t ch = 1; ch <= 16; ++ch) {
        assert(oa.requested(ch) == ob.requested(ch)); assert(oa.safe(ch) == ob.safe(ch));
    }
    inputs.valid = 2;
    assert(!inspectConfiguredMixInputs(c, inputs, oa, plan).inputsValid);
    c.mixing[0].rules[0].enabled = 1; c.mixing[0].rules[0].source = 21;
    assert(!compileConfiguredMixing(c).valid);
    c = configuration(); c.steeringChannel = c.throttleChannel;
    assert(!compileConfiguredMixing(c).valid);
}
int main() {
    testSafety(); testArmingVetoes(); testStructuralEditsAndLiveTrim(); testResetPublicationOrder(); testMixCompilation();
    puts("Control engine: input faults, dwell, edits, reset publication, compiled mixes and wrap PASS");
}
