#include "control_configuration.h"
#include "output_safety.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

static void near(int a,int b) { assert(std::abs(a-b)<=2); }
static void testThresholdSwitches() {
    ChannelMixSettings m;m.includeBase=0;
    auto &r=m.rules[0];r.enabled=1;r.source=1;r.sourceMode=2;
    r.thresholdPercent=-5;r.hysteresisPercent=3;
    MixInputSnapshot in;in.valid=(uint64_t{1}<<kMixSourceCount)-1;
    ChannelMixRuntime rt;
    const int positions[]={0,-50,-51,-49,-21,-20,-30,-60,1000};
    const int expected[]={0,0,1000,1000,1000,0,0,1000,0};
    for(unsigned n=0;n<sizeof(positions)/sizeof(*positions);++n){
        in.values[1]=positions[n];assert(rt.apply(3,m,in,false,700,n*4,true)==expected[n]);
    }
    // Independent latches on other destinations and the second rule.
    in.values[1]=-100;assert(rt.apply(3,m,in,false,0,50,true)==1000);
    in.values[1]=-30;assert(rt.apply(4,m,in,false,0,54,true)==0);
    assert(rt.apply(3,m,in,false,0,58,true)==1000);
    m.rules[1]=r;m.rules[0].enabled=0;
    assert(rt.apply(3,m,in,false,0,62,true)==0);
    m.rules[0].enabled=1;m.rules[1].enabled=0;
    in.values[1]=-100;assert(rt.apply(3,m,in,false,0,66,true)==1000);
    rt.reset();in.values[1]=-30;assert(rt.apply(3,m,in,false,0,70,true)==0);
    // CUT, missing source, and inactive activation clear retained state.
    in.values[1]=-100;rt.apply(3,m,in,false,0,74,true);
    in.values[1]=-30;assert(rt.apply(3,m,in,false,0,78,false)==0);
    assert(rt.apply(3,m,in,false,0,82,true)==0);
    in.values[1]=-100;rt.apply(3,m,in,false,0,86,true);
    in.valid&=~(uint64_t{1}<<1);assert(!channelMixInputsValid(m,in,false));
    rt.apply(3,m,in,false,0,90,true);in.valid|=uint64_t{1}<<1;
    in.values[1]=-30;assert(rt.apply(3,m,in,false,0,94,true)==0);
    in.values[1]=-100;assert(rt.apply(3,m,in,false,0,95,false)==1000);
    in.values[1]=-30;assert(rt.apply(3,m,in,false,0,96,true)==0);
    r.condition=2;in.values[3]=1000;in.values[1]=-100;
    rt.apply(3,m,in,false,0,98,true);in.values[3]=-1000;
    assert(rt.apply(3,m,in,false,0,102,true)==0);
    in.values[3]=1000;in.values[1]=-30;assert(rt.apply(3,m,in,false,0,106,true)==0);
    r.condition=0;r.sourceMode=1;r.thresholdPercent=50;r.hysteresisPercent=10;
    rt.reset();
    const int above[]={500,501,450,401,400,450};
    const int aboveExpected[]={0,1000,1000,1000,0,0};
    for(unsigned n=0;n<6;++n){in.values[1]=above[n];assert(rt.apply(3,m,in,false,0,110+n*4,true)==aboveExpected[n]);}
    // Zero hysteresis releases exactly at the trigger.
    r.hysteresisPercent=0;rt.reset();in.values[1]=501;
    assert(rt.apply(3,m,in,false,0,140,true)==1000);in.values[1]=500;
    assert(rt.apply(3,m,in,false,0,144,true)==0);
    r.thresholdPercent=-100;r.hysteresisPercent=1;assert(!validChannelMix(m));
    r.sourceMode=2;r.thresholdPercent=100;assert(!validChannelMix(m));
    r.hysteresisPercent=0;assert(validChannelMix(m));
    r.sourceMode=3;assert(!validChannelMix(m));
    r.sourceMode=2;r.thresholdPercent=-5;r.hysteresisPercent=3;
    // Output is binary regardless of proportional weights; Replace drops baseline.
    r.weight=-25;r.splitWeight=1;r.negativeWeight=-40;r.replace=1;m.constantPercent=60;
    in.values[1]=-100;assert(rt.apply(3,m,in,false,0,148,false)==1000);
    in.values[1]=0;assert(rt.apply(3,m,in,false,0,152,false)==0);
    // Channel dependencies, safety, and preview use the same binary contribution.
    ControlConfigurationSnapshot c;c.mixing[2]=m;c.mixing[2].rules[0].source=20;
    ChannelSlewRuntime slew;CrsfChannelOutputModel out;in.values[0]=0;in.values[1]=-100;
    assert(validConfiguredMixing(c));
    processConfiguredMixedOutputs(c,out,in,rt,slew,160,true);
    near(permilleFromCrsfValue(out.requested(3)),1000);
    in.values[1]=-30;processConfiguredMixedOutputs(c,out,in,rt,slew,164,true);
    near(permilleFromCrsfValue(out.requested(3)),1000);
    uint16_t resolved[16];out.resolve(false,resolved);near(permilleFromCrsfValue(resolved[2]),0);
    in.values[1]=-20;processConfiguredMixedOutputs(c,out,in,rt,slew,168,true);
    near(permilleFromCrsfValue(out.requested(3)),0);
    const int16_t sources[]={-100,0};auto preview=previewChannelMix(c,3,c.mixing[2],0,sources,false,false);
    assert(preview.contributions[0]==1000&&preview.mixed==1000&&preview.output==1000);
    const int16_t band[]={-30,0};preview=previewChannelMix(c,3,c.mixing[2],0,band,false,false);
    assert(preview.contributions[0]==0&&preview.output==0); // preview starts OFF
    // Threshold latches cannot reach steering/throttle, directly or indirectly.
    c.mixing[1].rules[0].enabled=1;c.mixing[1].rules[0].source=21;
    assert(!validConfiguredMixing(c));assert(!configuredMixedThrottleNeutral(c,in));
    c.mixing[1]=ChannelMixSettings{};
    c.mixing[0]=c.mixing[2];assert(!validConfiguredMixing(c));
    // v3 settings migrate without changing existing behavior or enabling thresholds.
    ChannelMixSettingsV3 old;old.rules[0].enabled=1;old.rules[0].splitWeight=1;
    old.rules[0].negativeWeight=-40;old.constantPercent=60;old.cutOverride=1;old.cutPercent=20;
    old.outputMinimum=20;old.outputMaximum=60;old.transitionMs=500;
    auto migrated=migrateChannelMix(old);assert(validChannelMix(migrated));
    assert(migrated.rules[0].sourceMode==0&&migrated.rules[0].hysteresisPercent==5);
    assert(migrated.rules[0].negativeWeight==-40&&migrated.constantPercent==60);
    assert(migrated.outputMinimum==20&&migrated.outputMaximum==60&&migrated.cutPercent==20);
    assert(migrated.cutOverride&&migrated.transitionMs==500);
    auto changed=c;changed.mixing[2].rules[0].hysteresisPercent=4;
    assert(!equivalentControlConfiguration(c,changed));
}
int main() {
    testThresholdSwitches();
    ChannelMixSettings settings;
    MixInputSnapshot inputs;inputs.valid=(1U<<19)-1;
    inputs.values[0]=800;inputs.values[1]=600;inputs.values[3]=-1000;
    ChannelMixRuntime mix;
    assert(mix.apply(3,settings,inputs,false,200,100,true)==200);
    auto &r=settings.rules[0];r.enabled=1;r.weight=-70;
    assert(mix.apply(3,settings,inputs,false,200,104,true)==-360);
    r.replace=1;
    assert(mix.apply(3,settings,inputs,false,200,108,true)==-560);
    r.condition=2; // high only
    assert(mix.apply(3,settings,inputs,false,200,112,true)==200);
    inputs.values[3]=1000;
    assert(mix.apply(3,settings,inputs,false,200,116,true)==-560);
    r.condition=3;
    assert(mix.apply(3,settings,inputs,false,200,120,true)==200);
    assert(mix.apply(3,settings,inputs,true,200,124,true)==-560);
    r.condition=4;
    assert(mix.apply(3,settings,inputs,true,200,128,true)==200);
    r.condition=0;r.weight=100;
    settings.rules[1]=r;settings.rules[1].replace=0;settings.rules[1].source=1;
    assert(mix.apply(3,settings,inputs,false,0,132,true)==1000);
    // Missing selector is unsafe even when the mix was previously inactive.
    r.condition=2;inputs.valid&=~(1U<<3);
    assert(!channelMixInputsValid(settings,inputs,false));
    inputs.valid=(1U<<19)-1;inputs.valid&=~1U;
    inputs.values[3]=-1000;
    settings.rules[1].enabled=0;
    assert(channelMixInputsValid(settings,inputs,false)); // inactive source needn't exist
    inputs.values[3]=1000;
    assert(!channelMixInputsValid(settings,inputs,false));
    inputs.valid=(1U<<19)-1;
    settings=ChannelMixSettings{};settings.rateEnabled=1;
    settings.lowNegative=100;settings.lowPositive=50;inputs.values[3]=-1000;
    assert(mix.apply(2,settings,inputs,false,1000,140,true)==500);
    assert(mix.apply(2,settings,inputs,false,-1000,144,true)==-1000);
    inputs.values[3]=1000;
    assert(mix.apply(2,settings,inputs,false,1000,148,true)==1000);
    settings.transitionMs=1000;inputs.values[3]=-1000;
    assert(mix.apply(2,settings,inputs,false,1000,248,true)==900);
    assert(mix.apply(2,settings,inputs,false,1000,348,true)==800);
    assert(mix.apply(2,settings,inputs,false,1000,448,false)==500); // CUT doesn't retain ramp
    settings.rateControl=0;assert(!validChannelMix(settings));
    settings=ChannelMixSettings{};settings.rules[0].source=255;assert(!validChannelMix(settings));

    ControlConfigurationSnapshot config;
    config.channelProfiles[2].positiveEndpointPercent=80;
    config.channelProfiles[2].negativeEndpointPercent=60;
    config.mixing[2].rules[0].enabled=1;config.mixing[2].rules[0].replace=1;
    ChannelSlewRuntime slew;CrsfChannelOutputModel output;
    inputs.values[0]=1000;
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,500,true);
    near(permilleFromCrsfValue(output.requested(3)),800); // shaped once
    inputs.values[0]=-1000;
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,504,true);
    near(permilleFromCrsfValue(output.requested(3)),-600);
    uint16_t resolved[16];output.resolve(false,resolved);
    assert(resolved[2]==CRSF_CHANNEL_MID); // CUT ignores live mix
    inputs.valid&=~1U;
    assert(!configuredMixInputsValid(config,inputs));
    auto decision=arbitrateOutputSafety(OutputSafetyStatus::InputFault,true);
    assert(!decision.transmitRequested);
    output.resolve(decision.transmitRequested,resolved);assert(resolved[2]==CRSF_CHANNEL_MID);
    config.steeringEnabled=false;inputs.valid|=1U;
    config.mixing[0]=config.mixing[2];
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,508,true);
    output.resolve(true,resolved);assert(resolved[0]==CRSF_CHANNEL_MID);
    // Switched rates scale travel after expo and leave the neutral offset intact.
    inputs.values[0]=1000;config.mixing[2].rateEnabled=1;inputs.values[3]=-1000;
    config.channelProfiles[2].positiveExpoPercent=100;
    config.channelProfiles[2].subtrimPercent=10;
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,512,true);
    near(permilleFromCrsfValue(output.requested(3)),500); // 80% endpoint * 50% + 10%
    near(permilleFromCrsfValue(output.safe(3)),100);
    // Mix/rate edits participate in atomic configuration revision detection.
    auto changed=config;changed.mixing[2].rules[0].weight=50;
    assert(!equivalentControlConfiguration(config,changed));
    changed=config;changed.mixing[2].rateEnabled=0;
    assert(!equivalentControlConfiguration(config,changed));
    // Disabled features preserve all default control outputs.
    config=ControlConfigurationSnapshot{};inputs.values[0]=400;inputs.values[1]=600;
    output.reset();output.configure(4,crsfValueFromPermille(-1000),crsfValueFromPermille(-1000));
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,600,false);
    near(permilleFromCrsfValue(output.requested(1)),400);
    near(permilleFromCrsfValue(output.requested(2)),600);
    near(permilleFromCrsfValue(output.requested(4)),-1000);
    config.mixing[1].rules[0].enabled=1;config.mixing[1].rules[0].replace=1;
    inputs.values[1]=0;inputs.values[0]=500;
    assert(!configuredMixedThrottleNeutral(config,inputs));
    inputs.values[0]=0;assert(configuredMixedThrottleNeutral(config,inputs));
    
    // Two channel contributions, independent of source servo mechanics.
    config=ControlConfigurationSnapshot{};output.reset();inputs.valid=(1U<<19)-1;
    inputs.values[0]=500;inputs.values[1]=-250;
    config.channelProfiles[0].positiveEndpointPercent=50;
    config.channelProfiles[0].reversed=true;config.channelProfiles[0].subtrimPercent=10;
    auto &sum=config.mixing[2];sum.includeBase=0;
    sum.rules[0].enabled=1;sum.rules[0].source=19;sum.rules[0].weight=60;
    sum.rules[1].enabled=1;sum.rules[1].source=20;sum.rules[1].weight=40;
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,700,false);
    near(permilleFromCrsfValue(output.requested(3)),200);
    sum.rules[1].splitWeight=1;sum.rules[1].negativeWeight=-40;
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,704,false);
    near(permilleFromCrsfValue(output.requested(3)),400);
    config.channelParameters[0].trimPermille=100;sum.rules[0].followTrim=1;
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,708,false);
    near(permilleFromCrsfValue(output.requested(3)),460);
    // Forward references resolve in dependency order, never from transmitted/CUT values.
    config.mixing[3].includeBase=0;config.mixing[3].rules[0].enabled=1;
    config.mixing[3].rules[0].source=21;
    assert(validChannelMixGraph(config.mixing));
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,712,false);
    near(permilleFromCrsfValue(output.requested(4)),460);
    sum.rules[0].source=22;assert(!validChannelMixGraph(config.mixing));
    assert(!configuredMixInputsValid(config,inputs));
    sum.rules[0].source=21;assert(!validChannelMixGraph(config.mixing));
    sum.rules[0].source=35;assert(validChannelMixGraph(config.mixing));
    inputs.valid&=~1U;assert(!configuredMixInputsValid(config,inputs));inputs.valid|=1;
    config.steeringEnabled=false;assert(!configuredMixInputsValid(config,inputs));
    // Legacy weights/base behavior are retained during migration.
    LegacyMixSettings old={};old.rules[0]={1,0,-70,1,0,3};old.rules[1].control=3;
    old.rateControl=3;old.lowNegative=50;old.lowPositive=50;old.highNegative=100;old.highPositive=100;
    auto migrated=migrateChannelMix(old);assert(validChannelMix(migrated));
    assert(migrated.includeBase&&migrated.rules[0].source==0&&migrated.rules[0].weight==-70);
    // Bench preview uses the actual mixer and destination response, with no writes.
    config=ControlConfigurationSnapshot{};ChannelMixSettings draft;draft.includeBase=0;
    draft.rules[0].enabled=1;draft.rules[0].source=19;draft.rules[0].weight=60;
    draft.rules[1].enabled=1;draft.rules[1].source=20;draft.rules[1].weight=40;
    const int16_t testSources[]={500,-250};
    config.channelProfiles[2].positiveEndpointPercent=80;config.channelProfiles[2].reversed=true;
    auto preview=previewChannelMix(config,3,draft,900,testSources,false,false);
    assert(preview.contributions[0]==300&&preview.contributions[1]==-100);
    assert(preview.mixed==200&&preview.output==-160);
    assert(!config.mixing[2].rules[0].enabled);
    draft.rules[1].replace=1;preview=previewChannelMix(config,3,draft,900,testSources,false,false);
    assert(preview.mixed==-100);

    // Gyro: independent baseline, forward-only reduction, mode-preserving bounds.
    config=ControlConfigurationSnapshot{};output.reset();mix=ChannelMixRuntime{};
    slew=ChannelSlewRuntime{};inputs.values[0]=0;inputs.values[1]=0;
    auto &gyro=config.mixing[2];gyro.includeBase=0;gyro.constantPercent=60;
    gyro.outputMinimum=20;gyro.outputMaximum=60;gyro.cutOverride=1;gyro.cutPercent=60;
    gyro.rules[0].enabled=1;gyro.rules[0].source=20;gyro.rules[0].weight=-40;
    gyro.rules[0].splitWeight=1;gyro.rules[0].negativeWeight=0;
    assert(validConfiguredMixing(config));
    const int16_t throttlePositions[]={0,500,1000,-1000};
    const int16_t gains[]={600,400,200,600};
    for(unsigned n=0;n<4;++n) {
        inputs.values[1]=throttlePositions[n];
        processConfiguredMixedOutputs(config,output,inputs,mix,slew,800+n*4,false);
        near(permilleFromCrsfValue(output.requested(3)),gains[n]);
        output.resolve(false,resolved);near(permilleFromCrsfValue(resolved[2]),600);
    }
    int16_t curve[kMixCurvePoints];const int16_t sourcePositions[]={0,0};
    previewChannelMixCurve(config,3,gyro,0,sourcePositions,false,false,1,curve);
    assert(curve[0]==600&&curve[20]==600&&curve[30]==400&&curve[40]==200);
    gyro.rules[1]=gyro.rules[0];gyro.rules[1].weight=-10;
    previewChannelMixCurve(config,3,gyro,0,sourcePositions,false,false,2,curve);
    assert(curve[30]==350&&curve[40]==200); // shared source swept together; final clamp
    gyro.rules[1].enabled=0;
    config.channelProfiles[2].reversed=true;
    config.channelProfiles[2].subtrimPercent=-10;
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,820,false);
    near(permilleFromCrsfValue(output.requested(3)),200);
    near(permilleFromCrsfValue(output.safe(3)),600); // final CUT value bypasses shaping
    const int16_t gyroSources[]={1000,0};
    preview=previewChannelMix(config,3,gyro,0,gyroSources,false,false);
    assert(preview.mixed==200&&preview.output==200);
    config.throttleChannel=3;assert(!validConfiguredMixing(config));
    assert(!configuredMixInputsValid(config,inputs));
    processConfiguredMixedOutputs(config,output,inputs,mix,slew,824,false);
    assert(permilleFromCrsfValue(output.safe(3))!=600);
    gyro.cutOverride=0;assert(!validConfiguredMixing(config)); // bounds still exclude neutral
    gyro.outputMinimum=-100;assert(validConfiguredMixing(config));
    config=ControlConfigurationSnapshot{};inputs.values[1]=0;
    config.mixing[1].constantPercent=60;
    assert(!configuredMixedThrottleNeutral(config,inputs));
    ChannelMixSettingsV2 oldV2;oldV2.rules[0].enabled=1;oldV2.rules[0].weight=-40;
    migrated=migrateChannelMix(oldV2);
    assert(migrated.constantPercent==0&&migrated.outputMinimum==-100&&migrated.outputMaximum==100);
    assert(!migrated.cutOverride&&migrated.rules[0].weight==-40);
    migrated.outputMinimum=60;migrated.outputMaximum=20;assert(!validChannelMix(migrated));
    puts("channel mixing: PASS");
}
