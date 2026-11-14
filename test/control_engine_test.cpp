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
static void testAppliedSteeringEndpointReachesOutputAfterRearm() {
    for(bool reversed : {false,true}) {
        auto c=configuration();c.channelProfiles[0].reversed=reversed;
        ControlEngine e;qualify(e,c,10);
        auto turn=input(1514);turn.steeringAdc=reversed?3000:1000;
        assert(e.update(c,turn).outputs.requested(1)==crsfValueFromPermille(-1000));
        setChannelEndpointForOutput(c.channelProfiles[0],false,60);
        c.armRequested=false;++c.revision;
        assert(e.update(c,input(1600)).safety==OutputSafetyStatus::Cut);
        c.armRequested=true;++c.revision;qualify(e,c,2000);
        turn.nowMs=3504;
        auto result=e.update(c,turn);
        assert(result.safety==OutputSafetyStatus::Armed);
        assert(result.outputs.requested(1)==crsfValueFromPermille(-600));
        assert(result.outputs.requested(2)==CRSF_CHANNEL_MID);
        turn.steeringAdc=reversed?1000:3000;turn.nowMs=3508;
        assert(e.update(c,turn).outputs.requested(1)==crsfValueFromPermille(1000));
    }
}

static void testLiveSteeringEndpoints() {
    for (bool reversed : {false, true}) {
        auto c=configuration();c.channelProfiles[0].reversed=reversed;
        ControlEngine e;qualify(e,c,10);
        auto turn=input(1514);turn.steeringAdc=reversed?3000:1000;
        auto before=c;
        setChannelEndpointForOutput(c.channelProfiles[0],false,60);++c.revision;
        assert(!controlConfigurationRequiresRearm(before,c));
        auto r=e.update(c,turn);
        assert(r.safety==OutputSafetyStatus::Armed);
        uint16_t sent[16];r.outputs.resolve(true,sent);
        assert(sent[0]==crsfValueFromPermille(-600)&&sent[1]==CRSF_CHANNEL_MID);
        // Cancellation restores travel live; CUT never becomes enabled by edits.
        c.channelProfiles[0]=before.channelProfiles[0];++c.revision;turn.nowMs+=4;
        r=e.update(c,turn);assert(r.safety==OutputSafetyStatus::Armed);
        assert(r.outputs.requested(1)==crsfValueFromPermille(-1000));
        c.armRequested=false;++c.revision;turn.nowMs+=4;
        r=e.update(c,turn);assert(r.safety==OutputSafetyStatus::Cut);
        setChannelEndpointForOutput(c.channelProfiles[0],false,70);++c.revision;turn.nowMs+=4;
        r=e.update(c,turn);r.outputs.resolve(false,sent);
        assert(r.safety==OutputSafetyStatus::Cut&&sent[0]==CRSF_CHANNEL_MID&&sent[1]==CRSF_CHANNEL_MID);
        before=c;c.channelProfiles[1].positiveEndpointPercent=70;
        assert(controlConfigurationRequiresRearm(before,c));
        c=before;c.channelProfiles[0].reversed=!reversed;
        assert(controlConfigurationRequiresRearm(before,c));
        c=before;c.channelProfiles[0].positiveEndpointPercent=101;
        assert(controlConfigurationRequiresRearm(before,c));
    }
    // Logical channel and configured-role mix sources ignore mechanical travel.
    for (uint8_t source : {uint8_t(kMixChannelSource),uint8_t(kMixConfiguredSource)}) {
        auto c=configuration();c.mixing[1].rules[0].enabled=1;
        c.mixing[1].rules[0].source=source;c.mixing[1].rules[0].weight=20;
        ControlEngine e;qualify(e,c,10);auto i=input(1514);i.steeringAdc=3000;
        auto a=e.update(c,i);const auto throttle=a.outputs.requested(2);
        c.channelProfiles[0].positiveEndpointPercent=55;++c.revision;i.nowMs+=4;
        auto b=e.update(c,i);
        assert(b.safety==OutputSafetyStatus::Armed&&b.outputs.requested(2)==throttle);
        assert(b.outputs.requested(1)==crsfValueFromPermille(550));
        i.steeringAdc=4095;i.nowMs+=4;
        assert(e.update(c,i).safety==OutputSafetyStatus::InputFault);
    }
}

static void testBrakeAbsSafety() {
    for(bool reversed:{false,true}) {
        auto c=configuration();c.brakeAbs.enabled=c.brakeAbs.forwardBrakeVerified=true;
        c.brakeAbs.delayMs=0;c.channelProfiles[1].reversed=reversed;
        ControlEngine e;qualify(e,c,10);auto i=input(1514);i.throttleAdc=1000;
        auto r=e.update(c,i);assert(r.safety==OutputSafetyStatus::Armed);
        assert(r.outputs.requested(2)==crsfValueFromPermille(reversed?1000:-1000));
        i.nowMs+=100;r=e.update(c,i);
        assert(r.safety==OutputSafetyStatus::Armed);
        assert(r.outputs.requested(2)==crsfValueFromPermille(reversed?700:-700));
        assert(r.outputs.requested(1)==CRSF_CHANNEL_MID);
        const auto stale=arbitrateOutputSafety(r.safety,false);uint16_t sent[16];
        r.outputs.resolve(stale.transmitRequested,sent);assert(sent[1]==r.outputs.safe(2));
        c.armRequested=false;++c.revision;i.nowMs+=4;r=e.update(c,i);
        r.outputs.resolve(false,sent);assert(r.safety==OutputSafetyStatus::Cut&&sent[1]==CRSF_CHANNEL_MID);
        c.brakeAbs.forwardBrakeVerified=false;++c.revision;
        assert(e.update(c,i).safety==OutputSafetyStatus::ConfigurationFault);
    }
    auto c=configuration();ControlEngine e;qualify(e,c,10);
    c.brakeAbs.forwardBrakeVerified=c.brakeAbs.enabled=true;++c.revision;
    assert(e.update(c,input(1514)).safety==OutputSafetyStatus::Cut); // edits require re-arm
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
static void testEscSelectionSurvivesCutAndVetoesEveryArmRequest() {
    auto c=configuration();c.escSelectorReserved=true;c.escProgramming=true;c.escProgrammerSelected=true;
    ControlEngine engine;
    auto i=input(0);i.throttleAdc=3000;
    i.bindings.configure(12,crsfValueFromPermille(-500),crsfValueFromPermille(300));
    c.channelParameters[11].trimPermille=-200;c.channelProfiles[11].reversed=true;
    c.mixing[11].cutOverride=1;c.mixing[11].cutPercent=-50;
    auto r=engine.update(c,i);assert(r.safety==OutputSafetyStatus::Cut);
    uint16_t sent[16];r.outputs.resolve(false,sent);assert(sent[11]==crsfValueFromPermille(1000));
    assert(sent[1]==r.outputs.safe(2));
    c.escProgrammerSelected=false;++c.revision;i.nowMs=10000;
    r=engine.update(c,i);assert(r.safety==OutputSafetyStatus::Cut);r.outputs.resolve(false,sent);assert(sent[11]==crsfValueFromPermille(-1000));
    c.armRequested=false;c.escProgramming=false;++c.revision;r=engine.update(c,i);assert(r.safety==OutputSafetyStatus::Cut);
    c.throttleChannel=12;++c.revision;assert(engine.update(c,i).safety==OutputSafetyStatus::ConfigurationFault);
}
int main() {
    testEscSelectionSurvivesCutAndVetoesEveryArmRequest();
    testAppliedSteeringEndpointReachesOutputAfterRearm();
    testLiveSteeringEndpoints();
    testBrakeAbsSafety();
    testSafety(); testArmingVetoes(); testStructuralEditsAndLiveTrim(); testResetPublicationOrder(); testMixCompilation();
    puts("Control engine: input faults, dwell, edits, reset publication, compiled mixes and wrap PASS");
}
