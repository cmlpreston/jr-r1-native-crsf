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
static void testReadinessWhileCut() {
    auto c = configuration();
    c.armRequested = false;
    ControlEngine engine;
    auto sample = input(10);
    sample.throttleAdc = 2081;
    sample.linkQuality = 49;
    auto result = engine.update(c, sample);
    assert(result.safety == OutputSafetyStatus::Cut && result.neutralSinceMs == 0);
    assert(result.readiness.triggerValid && !result.readiness.triggerCentred);
    assert(result.readiness.linkValid && !result.readiness.linkMeetsMinimum);
    assert(result.readiness.configurationValid && result.readiness.inputsValid);
    assert(!result.readiness.throttleMixActive && result.readiness.throttleMixNeutral);

    sample.throttleAdc = 2080;
    sample.linkQuality = 50;
    ++sample.nowMs;
    result = engine.update(c, sample);
    assert(result.safety == OutputSafetyStatus::Cut);
    assert(result.readiness.linkMeetsMinimum && result.readiness.triggerCentred);

    c.mixing[1].constantPercent = 6;
    ++c.revision;
    sample.throttleAdc = 2000;
    ++sample.nowMs;
    result = engine.update(c, sample);
    assert(result.safety == OutputSafetyStatus::Cut && result.readiness.triggerCentred);
    assert(result.readiness.throttleMixValid && result.readiness.throttleMixActive);
    assert(result.readiness.throttleMixPermille == 60 && !result.readiness.throttleMixNeutral);
    sample.physicalInputsHealthy = false;
    ++sample.nowMs;
    result = engine.update(c, sample);
    assert(result.safety == OutputSafetyStatus::InputFault && !result.readiness.inputsValid);

    c.mixing[1].constantPercent = 5;
    ++c.revision;
    sample.physicalInputsHealthy = true;
    ++sample.nowMs;
    result = engine.update(c, sample);
    assert(result.safety == OutputSafetyStatus::Cut && result.readiness.throttleMixNeutral);
    c.armRequested = true;
    qualify(engine,c,2000);
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

static void testLiveSteeringSubtrimKeepsEndpointsAndThrottle() {
    for (uint8_t steering : {uint8_t(1), uint8_t(3)}) {
        for (bool reversed : {false, true}) {
            for (uint16_t throttle : {uint16_t(2000), uint16_t(3000)}) {
                auto c=configuration();c.steeringChannel=steering;
                auto &p=c.channelProfiles[steering-1];p.reversed=reversed;
                setChannelEndpointForOutput(p,false,60);
                setChannelEndpointForOutput(p,true,80);
                p.negativeExpoPercent=-20;p.positiveExpoPercent=35;
                ControlEngine e;qualify(e,c,10);
                auto i=input(1514);i.throttleAdc=throttle;
                const uint16_t expectedThrottle=throttle==2000?CRSF_CHANNEL_MID:crsfValueFromPermille(1000);
                for (int16_t offset : {int16_t(-200),int16_t(200),int16_t(0)}) {
                    const auto before=c;c.channelParameters[steering-1].subtrimPermille=offset;++c.revision;
                    assert(!controlConfigurationRequiresRearm(before,c));
                    i.steeringAdc=2000;i.nowMs+=4;
                    auto r=e.update(c,i);uint16_t sent[16];r.outputs.resolve(true,sent);
                    assert(r.safety==OutputSafetyStatus::Armed);
                    assert(sent[steering-1]==crsfValueFromPermille(offset));
                    assert(sent[1]==expectedThrottle);
                    // Changing centre at either limit must leave mechanical travel fixed.
                    i.steeringAdc=reversed?3000:1000;i.nowMs+=4;r=e.update(c,i);
                    assert(r.safety==OutputSafetyStatus::Armed);
                    assert(r.outputs.requested(steering)==crsfValueFromPermille(-600));
                    assert(r.outputs.requested(2)==expectedThrottle);
                    i.steeringAdc=reversed?1000:3000;i.nowMs+=4;r=e.update(c,i);
                    assert(r.safety==OutputSafetyStatus::Armed);
                    assert(r.outputs.requested(steering)==crsfValueFromPermille(800));
                    assert(r.outputs.requested(2)==expectedThrottle);
                }
                c.armRequested=false;++c.revision;i.nowMs+=4;
                assert(e.update(c,i).safety==OutputSafetyStatus::Cut);
                c.channelParameters[steering-1].subtrimPermille=100;++c.revision;i.nowMs+=4;
                auto r=e.update(c,i);uint16_t sent[16];r.outputs.resolve(false,sent);
                assert(r.safety==OutputSafetyStatus::Cut);
                assert(sent[steering-1]==crsfValueFromPermille(100));
                assert(sent[1]==CRSF_CHANNEL_MID); // Editing never re-enables a CUT model.
            }
        }
    }
}

static void testLiveSteeringSubtrimDoesNotLeakThroughMixSources() {
    for (uint8_t steering : {uint8_t(1),uint8_t(3)}) {
        for (bool configuredSource : {false,true}) {
            for (bool followTrim : {false,true}) {
                for (bool reversed : {false,true}) {
                    auto c=configuration();c.steeringChannel=steering;
                    c.channelProfiles[steering-1].reversed=reversed;
                    c.channelParameters[steering-1].trimPermille=100;
                    auto &rule=c.mixing[1].rules[0];rule.enabled=1;rule.weight=20;
                    rule.source=configuredSource?kMixConfiguredSource:kMixChannelSource+steering-1;
                    rule.followTrim=followTrim;
                    ControlEngine e;qualify(e,c,10);auto i=input(1514);i.steeringAdc=2600;
                    for (uint16_t throttle : {uint16_t(2000),uint16_t(2500)}) {
                        i.throttleAdc=throttle;i.nowMs+=4;
                        const auto baseline=e.update(c,i);
                        assert(baseline.safety==OutputSafetyStatus::Armed);
                        // Keep this unsaturated, so an unintended contribution cannot hide at a clamp.
                        assert(baseline.outputs.requested(2)>CRSF_CHANNEL_MID);
                        assert(baseline.outputs.requested(2)<crsfValueFromPermille(900));
                        for (int16_t offset : {int16_t(-200),int16_t(200)}) {
                            c.channelParameters[steering-1].subtrimPermille=offset;++c.revision;i.nowMs+=4;
                            const auto changed=e.update(c,i);
                            assert(changed.safety==OutputSafetyStatus::Armed);
                            assert(changed.outputs.requested(steering)!=baseline.outputs.requested(steering));
                            for (uint8_t ch=1;ch<=16;++ch) {
                                if(ch==steering)continue;
                                assert(changed.outputs.requested(ch)==baseline.outputs.requested(ch));
                                assert(changed.outputs.safe(ch)==baseline.outputs.safe(ch));
                            }
                        }
                        c.channelParameters[steering-1].subtrimPermille=0;++c.revision;i.nowMs+=4;
                        assert(e.update(c,i).safety==OutputSafetyStatus::Armed);
                    }
                }
            }
        }
    }
}

static void testLiveSteeringSubtrimExemptionIsNarrow() {
    const auto before=configuration();
    auto changed=before;changed.channelParameters[0].subtrimPermille=100;
    assert(!controlConfigurationRequiresRearm(before,changed));
    const auto live=changed;
    const auto requiresRearm=[&](const ControlConfigurationSnapshot &candidate) {
        assert(controlConfigurationRequiresRearm(before,candidate));
        auto c=before;ControlEngine e;qualify(e,c,10);c=candidate;++c.revision;
        assert(e.update(c,input(1514)).safety!=OutputSafetyStatus::Armed);
    };
    for (int16_t invalid : {int16_t(-201),int16_t(201)}) {
        changed=live;changed.channelParameters[0].subtrimPermille=invalid;requiresRearm(changed);
        auto damaged=before;damaged.channelParameters[0].subtrimPermille=invalid;
        changed=damaged;changed.channelParameters[0].subtrimPermille=0;
        assert(controlConfigurationRequiresRearm(damaged,changed));
    }
    for (uint8_t ch : {uint8_t(2),uint8_t(4)}) {
        changed=live;changed.channelParameters[ch-1].subtrimPermille=100;requiresRearm(changed);
    }
    changed=live;changed.channelProfiles[0].subtrimPercent=1;requiresRearm(changed);
    changed=live;changed.channelProfiles[0].reversed=true;requiresRearm(changed);
    changed=live;changed.channelProfiles[0].positiveRatePercent=90;requiresRearm(changed);
    changed=live;changed.steeringChannel=3;requiresRearm(changed);
    changed=live;changed.throttleChannel=1;requiresRearm(changed);
    changed=live;changed.steeringEnabled=false;requiresRearm(changed);
    changed=live;changed.steeringInput.deadbandAdc+=1;requiresRearm(changed);
    changed=live;changed.mixing[1].constantPercent=5;requiresRearm(changed);
    // The same-role/enabled guard also applies when both snapshots are disabled.
    auto disabled=before;disabled.steeringEnabled=false;changed=disabled;
    changed.channelParameters[0].subtrimPermille=100;
    assert(controlConfigurationRequiresRearm(disabled,changed));
}

static void testLiveSteeringSubtrimCannotBypassFaultsOrPhysicalCut() {
    for (unsigned fault=0;fault<4;++fault) {
        auto c=configuration();ControlEngine e;qualify(e,c,10);auto i=input(1514);
        c.channelParameters[0].subtrimPermille=100;++c.revision;i.throttleAdc=3000;
        if(fault==0)i.steeringAdc=4095;
        if(fault==1)i.throttleAdc=4095;
        if(fault==2)i.physicalInputsHealthy=false;
        if(fault==3)c.modelStorageValid=false;
        const auto r=e.update(c,i);
        assert(r.safety==(fault==3?OutputSafetyStatus::ConfigurationFault:OutputSafetyStatus::InputFault));
        uint16_t sent[16];r.outputs.resolve(false,sent);
        assert(sent[1]==r.outputs.safe(2)&&sent[1]==CRSF_CHANNEL_MID);
    }
    auto c=configuration();ControlEngine e;qualify(e,c,10);auto i=input(1514);
    c.channelParameters[0].subtrimPermille=100;++c.revision;i.throttleAdc=3000;
    const auto live=e.update(c,i);assert(live.safety==OutputSafetyStatus::Armed);
    const auto stale=arbitrateOutputSafety(live.safety,false);uint16_t sent[16];
    live.outputs.resolve(stale.transmitRequested,sent);
    assert(stale.status==OutputSafetyStatus::ControlStale&&!stale.transmitRequested);
    assert(sent[0]==crsfValueFromPermille(100)&&sent[1]==CRSF_CHANNEL_MID);

    ControlOutputMailbox mailbox;assert(mailbox.acceptConfiguration(c.revision,true));
    const auto old=mailbox.commands();
    assert(mailbox.publish(live.outputs,live.safety,100,old.resetSequence,c.revision));
    mailbox.requestPhysicalCut();
    c.channelParameters[0].subtrimPermille=-100;++c.revision;i.nowMs+=4;
    const auto stillArmed=e.update(c,i);assert(stillArmed.safety==OutputSafetyStatus::Armed);
    assert(!mailbox.publish(stillArmed.outputs,stillArmed.safety,104,old.resetSequence,c.revision));
    const auto cut=mailbox.commands();
    assert(!mailbox.acceptConfiguration(c.revision,true,cut.physicalCutSequence));
    assert(mailbox.outputs().controlSafetyStatus==OutputSafetyStatus::Cut);
    c.armRequested=false;++c.revision;i.resetSequence=cut.resetSequence;i.nowMs+=4;
    assert(mailbox.acceptConfiguration(c.revision,false,cut.physicalCutSequence));
    const auto safe=e.update(c,i);assert(safe.safety==OutputSafetyStatus::Cut);
    assert(mailbox.publish(safe.outputs,safe.safety,108,i.resetSequence,c.revision));
    mailbox.outputs().model.resolve(false,sent);
    assert(sent[0]==crsfValueFromPermille(-100)&&sent[1]==CRSF_CHANNEL_MID);
}

static ControlCycleInput expoChannelInput(uint8_t ch,int16_t value,uint32_t now) {
    auto i=input(now);
    if(ch==1)i.steeringAdc=2000+value;
    else if(ch==2)i.throttleAdc=2000+value;
    else assert(i.bindings.configure(ch,crsfValueFromPermille(value),CRSF_CHANNEL_MID));
    return i;
}

static void testLiveExpoEveryChannelAndBothSides() {
    for(uint8_t ch=1;ch<=16;++ch)for(bool reversed:{false,true}) {
        auto c=configuration();c.steeringInput.deadbandAdc=c.throttleInput.deadbandAdc=0;
        c.proportionalOutputMask|=uint16_t(1U<<(ch-1));
        auto &p=c.channelProfiles[ch-1];p.reversed=reversed;
        p.negativeEndpointPercent=60;p.positiveEndpointPercent=80;
        ControlEngine e;qualify(e,c,10);uint32_t now=1514;
        for(int side:{-1,1}) {
            auto &expo=side<0?p.negativeExpoPercent:p.positiveExpoPercent;
            const auto inactive=side<0?p.positiveExpoPercent:p.negativeExpoPercent;
            for(int8_t value:{int8_t(-100),int8_t(100),int8_t(0)}) {
                const auto before=c;expo=value;++c.revision;
                assert(!controlConfigurationRequiresRearm(before,c));
                auto r=e.update(c,expoChannelInput(ch,side*500,now+=4));
                assert(r.safety==OutputSafetyStatus::Armed);
                // At half input, +/-100 expo gives 87.5%/12.5% before endpoint scaling.
                const int magnitude=(side<0?600:800)*(value==-100?875:value==100?125:500)/1000;
                const int expected=magnitude*side*(reversed?-1:1);
                const int error=permilleFromCrsfValue(r.outputs.requested(ch))-expected;
                assert(error>=-3&&error<=3); // CRSF round-trip quantisation on AUX base values.
                r=e.update(c,expoChannelInput(ch,0,now+=4));
                assert(r.safety==OutputSafetyStatus::Armed&&r.outputs.requested(ch)==CRSF_CHANNEL_MID);
                for(int endpoint:{-1000,1000}) {
                    r=e.update(c,expoChannelInput(ch,endpoint,now+=4));
                    assert(r.safety==OutputSafetyStatus::Armed);
                    const int fixed=(endpoint<0?-600:800)*(reversed?-1:1);
                    assert(r.outputs.requested(ch)==crsfValueFromPermille(fixed));
                }
                assert((side<0?p.positiveExpoPercent:p.negativeExpoPercent)==inactive);
            }
        }
        // Changing either side while CUT cannot request live output.
        c.armRequested=false;++c.revision;
        assert(e.update(c,expoChannelInput(ch,500,now+=4)).safety==OutputSafetyStatus::Cut);
        p.positiveExpoPercent=75;p.negativeExpoPercent=-75;++c.revision;
        const auto cut=e.update(c,expoChannelInput(ch,500,now+=4));uint16_t sent[16];
        cut.outputs.resolve(false,sent);
        assert(cut.safety==OutputSafetyStatus::Cut&&sent[ch-1]==CRSF_CHANNEL_MID&&sent[1]==CRSF_CHANNEL_MID);
    }
}

static void testLiveExpoPropagatesThroughLogicalAndConfiguredMixes() {
    for(unsigned sourceCase=0;sourceCase<5;++sourceCase)for(bool negative:{false,true}) {
        auto c=configuration();c.steeringInput.deadbandAdc=c.throttleInput.deadbandAdc=0;
        const uint8_t sourceChannel=sourceCase<2?1:sourceCase==2?3:2;
        const uint8_t destination=sourceChannel==2?4:2;
        auto &m=c.mixing[destination-1];m.includeBase=0;
        auto &rule=m.rules[0];rule.enabled=1;rule.weight=40;
        rule.source=sourceCase==1?kMixConfiguredSource:sourceCase==4?kMixConfiguredSource+1:
            kMixChannelSource+sourceChannel-1;
        const auto plan=compileConfiguredMixing(c);assert(plan.valid);
        ControlEngine e;qualify(e,c,10);
        auto i=expoChannelInput(sourceChannel,negative?-500:500,1514);
        const auto baseline=e.update(c,i);assert(baseline.safety==OutputSafetyStatus::Armed);
        const int before=permilleFromCrsfValue(baseline.outputs.requested(destination));
        assert(negative?before<-190:before>190); // Confirm the rule is active and unsaturated.
        auto &profile=c.channelProfiles[sourceChannel-1];
        (negative?profile.negativeExpoPercent:profile.positiveExpoPercent)=100;++c.revision;i.nowMs+=4;
        const auto changed=e.update(c,i);assert(changed.safety==OutputSafetyStatus::Armed);
        const int after=permilleFromCrsfValue(changed.outputs.requested(destination));
        assert(negative?(after<0&&after>before):(after>0&&after<before));
        assert(after>=-52&&after<=52); // A half-input cubic source contributes approximately 5%.
        // Expo changes source shaping, not the dependency order/required inputs.
        const auto afterPlan=compileConfiguredMixing(c);
        assert(afterPlan.valid&&afterPlan.count==plan.count&&afterPlan.requiredMask==plan.requiredMask);
        for(unsigned n=0;n<plan.count;++n)assert(afterPlan.order[n]==plan.order[n]);
    }
    // A live expo revision must retain an in-progress switched-rate transition.
    auto c=configuration();c.steeringInput.deadbandAdc=c.throttleInput.deadbandAdc=0;
    auto &m=c.mixing[1];m.includeBase=0;m.rules[0].enabled=1;
    m.rules[0].source=kMixConfiguredSource;m.rules[0].weight=100;
    m.rateEnabled=1;m.lowNegative=m.lowPositive=50;m.highNegative=m.highPositive=100;m.transitionMs=1000;
    ControlEngine e(ControlSafetyPolicy{80,0,0,50});auto i=input(100);
    i.digitalInputs.valid=uint64_t(1)<<3;i.digitalInputs.values[3]=0;
    assert(e.update(c,i).safety==OutputSafetyStatus::Armed);
    i.nowMs=200;i.steeringAdc=2500;i.digitalInputs.values[3]=1000;
    auto r=e.update(c,i);assert(r.safety==OutputSafetyStatus::Armed);
    assert(r.outputs.requested(2)==crsfValueFromPermille(300));
    c.channelProfiles[0].positiveExpoPercent=100;++c.revision;i.nowMs=300;r=e.update(c,i);
    assert(r.safety==OutputSafetyStatus::Armed);
    assert(r.outputs.requested(2)==crsfValueFromPermille(87)); // 12.5% source at the continuing 70% rate.
}

static void testLiveExpoExemptionRetainsBoundsAndStructuralVetoes() {
    const auto before=configuration();
    for(uint8_t ch=1;ch<=16;++ch)for(bool negative:{false,true}) {
        auto changed=before;
        auto &value=negative?changed.channelProfiles[ch-1].negativeExpoPercent:changed.channelProfiles[ch-1].positiveExpoPercent;
        for(int8_t valid:{int8_t(-100),int8_t(100)}) {
            value=valid;assert(!controlConfigurationRequiresRearm(before,changed));
        }
        for(int8_t invalid:{int8_t(-101),int8_t(101)}) {
            value=invalid;assert(controlConfigurationRequiresRearm(before,changed));
            auto repaired=changed;
            (negative?repaired.channelProfiles[ch-1].negativeExpoPercent:repaired.channelProfiles[ch-1].positiveExpoPercent)=0;
            assert(controlConfigurationRequiresRearm(changed,repaired));
        }
    }
    auto live=before;live.channelProfiles[0].positiveExpoPercent=40;live.channelProfiles[1].negativeExpoPercent=-40;
    const auto requiresRearm=[&](const ControlConfigurationSnapshot &candidate) {
        assert(controlConfigurationRequiresRearm(before,candidate));
        auto c=before;ControlEngine e;qualify(e,c,10);c=candidate;++c.revision;
        assert(e.update(c,input(1514)).safety!=OutputSafetyStatus::Armed);
    };
    auto changed=live;changed.channelProfiles[1].positiveRatePercent=90;requiresRearm(changed);
    changed=live;changed.channelProfiles[0].negativeRampMs=100;requiresRearm(changed);
    changed=live;changed.channelProfiles[0].reversed=true;requiresRearm(changed);
    changed=live;changed.channelProfiles[1].positiveEndpointPercent=80;requiresRearm(changed);
    changed=live;changed.channelParameters[1].subtrimPermille=10;requiresRearm(changed);
    changed=live;changed.steeringChannel=3;requiresRearm(changed);
    changed=live;changed.steeringInput.deadbandAdc+=1;requiresRearm(changed);
    changed=live;changed.mixing[1].constantPercent=5;requiresRearm(changed);
}

static void testLiveExpoRetainsFaultStaleAndPhysicalCutVetoes() {
    for(unsigned fault=0;fault<7;++fault) {
        auto c=configuration();ControlEngine e;qualify(e,c,10);auto i=input(1514);
        c.channelProfiles[1].positiveExpoPercent=75;++c.revision;i.throttleAdc=2500;
        OutputSafetyStatus expected=OutputSafetyStatus::InputFault;
        if(fault==0)i.steeringAdc=4095;
        if(fault==1)i.throttleAdc=4095;
        if(fault==2)i.physicalInputsHealthy=false;
        if(fault==3){c.modelStorageValid=false;expected=OutputSafetyStatus::ConfigurationFault;}
        if(fault==4){c.calibrationActive=true;expected=OutputSafetyStatus::Calibration;}
        if(fault==5){c.escProgramming=true;expected=OutputSafetyStatus::Cut;}
        if(fault==6){c.throttleEnabled=false;expected=OutputSafetyStatus::ChannelDisabled;}
        const auto r=e.update(c,i);assert(r.safety==expected);uint16_t sent[16];
        r.outputs.resolve(false,sent);assert(sent[1]==CRSF_CHANNEL_MID);
    }
    auto c=configuration();ControlEngine e;qualify(e,c,10);auto i=input(1514);i.throttleAdc=2500;
    c.channelProfiles[1].positiveExpoPercent=75;++c.revision;
    const auto live=e.update(c,i);assert(live.safety==OutputSafetyStatus::Armed);
    const auto stale=arbitrateOutputSafety(live.safety,false);uint16_t sent[16];
    live.outputs.resolve(stale.transmitRequested,sent);
    assert(stale.status==OutputSafetyStatus::ControlStale&&!stale.transmitRequested&&sent[1]==CRSF_CHANNEL_MID);
    ControlOutputMailbox mailbox;assert(mailbox.acceptConfiguration(c.revision,true));
    const auto old=mailbox.commands();assert(mailbox.publish(live.outputs,live.safety,100,old.resetSequence,c.revision));
    mailbox.requestPhysicalCut();c.channelProfiles[1].positiveExpoPercent=-75;++c.revision;i.nowMs+=4;
    const auto latest=e.update(c,i);assert(latest.safety==OutputSafetyStatus::Armed);
    assert(!mailbox.publish(latest.outputs,latest.safety,104,old.resetSequence,c.revision));
    const auto cut=mailbox.commands();assert(!mailbox.acceptConfiguration(c.revision,true,cut.physicalCutSequence));
    assert(mailbox.outputs().controlSafetyStatus==OutputSafetyStatus::Cut);
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

static void testIndependentInputHeartbeat() {
    auto c=configuration();ControlEngine e;qualify(e,c,10);auto i=input(1514);
    i.physicalInputsHealthy=false;
    auto r=e.update(c,i);assert(r.safety==OutputSafetyStatus::InputFault);
    i.nowMs+=2000;assert(e.update(c,i).safety==OutputSafetyStatus::InputFault); // Cannot re-arm while sampler/chips remain unhealthy.
    uint16_t sent[16];r.outputs.resolve(false,sent);assert(sent[1]==CRSF_CHANNEL_MID);
    c.armRequested=false;++c.revision;i.physicalInputsHealthy=true;i.nowMs+=4;
    assert(e.update(c,i).safety==OutputSafetyStatus::Cut);
    c.armRequested=true;++c.revision;qualify(e,c,2000);
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
    testReadinessWhileCut();
    testEscSelectionSurvivesCutAndVetoesEveryArmRequest();
    testAppliedSteeringEndpointReachesOutputAfterRearm();
    testLiveSteeringEndpoints();
    testLiveSteeringSubtrimKeepsEndpointsAndThrottle();
    testLiveSteeringSubtrimDoesNotLeakThroughMixSources();
    testLiveSteeringSubtrimExemptionIsNarrow();
    testLiveSteeringSubtrimCannotBypassFaultsOrPhysicalCut();
    testLiveExpoEveryChannelAndBothSides();
    testLiveExpoPropagatesThroughLogicalAndConfiguredMixes();
    testLiveExpoExemptionRetainsBoundsAndStructuralVetoes();
    testLiveExpoRetainsFaultStaleAndPhysicalCutVetoes();
    testBrakeAbsSafety();
    testIndependentInputHeartbeat();
    testSafety(); testArmingVetoes(); testStructuralEditsAndLiveTrim(); testResetPublicationOrder(); testMixCompilation();
    puts("Control engine: live expo/subtrim, input faults, dwell, edits, reset publication, compiled mixes and wrap PASS");
}
