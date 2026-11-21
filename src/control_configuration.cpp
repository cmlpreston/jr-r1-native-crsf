#include "control_configuration.h"

#include <stddef.h>
#include <string.h>

namespace {

template <typename T>
T clampValue(T value, T minimum, T maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

bool equalCalibration(const ProportionalCalibration &left,
                      const ProportionalCalibration &right)
{
    return left.minimum == right.minimum &&
           left.centre == right.centre &&
           left.maximum == right.maximum;
}

bool equalInputProfile(const ProportionalInputProfile &left,
                       const ProportionalInputProfile &right)
{
    return equalCalibration(left.calibration, right.calibration) &&
           left.deadbandAdc == right.deadbandAdc &&
           left.validMinimumAdc == right.validMinimumAdc &&
           left.validMaximumAdc == right.validMaximumAdc &&
           left.filterShift == right.filterShift;
}

bool equalResponseProfile(const ChannelResponseProfile &left,
                          const ChannelResponseProfile &right)
{
    return left.negativeEndpointPercent == right.negativeEndpointPercent &&
           left.positiveEndpointPercent == right.positiveEndpointPercent &&
           left.negativeRatePercent == right.negativeRatePercent &&
           left.positiveRatePercent == right.positiveRatePercent &&
           left.negativeExpoPercent == right.negativeExpoPercent &&
           left.positiveExpoPercent == right.positiveExpoPercent &&
           left.negativeRampMs == right.negativeRampMs &&
           left.positiveRampMs == right.positiveRampMs &&
           left.subtrimPercent == right.subtrimPercent &&
           left.reversed == right.reversed;
}

bool validChannel(uint8_t channel)
{
    return channel >= 1 && channel <= CRSF_CHANNEL_COUNT;
}

} // namespace

bool equivalentControlConfiguration(const ControlConfigurationSnapshot &left,
                                    const ControlConfigurationSnapshot &right)
{
    if (!equalBrakeAbs(left.brakeAbs,right.brakeAbs) ||
        !equalInputProfile(left.throttleInput, right.throttleInput) ||
        !equalInputProfile(left.steeringInput, right.steeringInput) ||
        left.proportionalOutputMask != right.proportionalOutputMask ||
        left.steeringChannel != right.steeringChannel ||
        left.throttleChannel != right.throttleChannel ||
        left.steeringEnabled != right.steeringEnabled ||
        left.throttleEnabled != right.throttleEnabled ||
        left.escSelectorReserved != right.escSelectorReserved ||
        left.escProgramming != right.escProgramming ||
        left.escProgrammerSelected != right.escProgrammerSelected ||
        left.modelStorageValid != right.modelStorageValid ||
        left.armRequested != right.armRequested ||
        left.calibrationActive != right.calibrationActive ||
        left.driveMode != right.driveMode ||
        left.walkLimitPercent != right.walkLimitPercent)
        return false;

    for (size_t index = 0; index < CRSF_CHANNEL_COUNT; ++index) {
        if (memcmp(&left.mixing[index], &right.mixing[index], sizeof(ChannelMixSettings)) != 0 ||
            !equalResponseProfile(left.channelProfiles[index],
                                  right.channelProfiles[index]) ||
            left.channelParameters[index].trimPermille !=
                right.channelParameters[index].trimPermille ||
            left.channelParameters[index].subtrimPermille !=
                right.channelParameters[index].subtrimPermille)
            return false;
    }
    return true;
}

uint16_t configuredRoleOwnedChannels(
    const ControlConfigurationSnapshot &configuration)
{
    uint16_t mask = 0;
    if (validChannel(configuration.steeringChannel))
        mask |= static_cast<uint16_t>(1U << (configuration.steeringChannel - 1));
    if (validChannel(configuration.throttleChannel))
        mask |= static_cast<uint16_t>(1U << (configuration.throttleChannel - 1));
    return mask;
}

bool controlConfigurationRequiresRearm(const ControlConfigurationSnapshot &before,
                                      const ControlConfigurationSnapshot &after)
{
    auto candidate = after;
    candidate.armRequested = before.armRequested;
    for (size_t n = 0; n < CRSF_CHANNEL_COUNT; ++n) {
        candidate.channelParameters[n].trimPermille = before.channelParameters[n].trimPermille;
        auto validExpo=[](int value){return value>=CHANNEL_RESPONSE_MIN_EXPO_PERCENT&&
            value<=CHANNEL_RESPONSE_MAX_EXPO_PERCENT;};
        auto &profile=candidate.channelProfiles[n];
        const auto &previous=before.channelProfiles[n];
        if(validExpo(profile.negativeExpoPercent)&&validExpo(previous.negativeExpoPercent))
            profile.negativeExpoPercent=previous.negativeExpoPercent;
        if(validExpo(profile.positiveExpoPercent)&&validExpo(previous.positiveExpoPercent))
            profile.positiveExpoPercent=previous.positiveExpoPercent;
    }
    // Mechanical steering travel and centre are deliberately adjustable live.
    // Logical mixer sources omit endpoints and parameter subtrim, so these
    // adjustments cannot change throttle or another destination through a mix.
    // Other profile/role/input edits still require CUT.
    const uint8_t steering = before.steeringChannel;
    if (validChannel(steering) && steering == after.steeringChannel &&
        steering != before.throttleChannel && steering != after.throttleChannel &&
        before.steeringEnabled && after.steeringEnabled) {
        auto &p = candidate.channelProfiles[steering - 1];
        if (p.negativeEndpointPercent >= CHANNEL_RESPONSE_MIN_ENDPOINT_PERCENT &&
            p.negativeEndpointPercent <= CHANNEL_RESPONSE_MAX_ENDPOINT_PERCENT &&
            p.positiveEndpointPercent >= CHANNEL_RESPONSE_MIN_ENDPOINT_PERCENT &&
            p.positiveEndpointPercent <= CHANNEL_RESPONSE_MAX_ENDPOINT_PERCENT) {
            p.negativeEndpointPercent = before.channelProfiles[steering - 1].negativeEndpointPercent;
            p.positiveEndpointPercent = before.channelProfiles[steering - 1].positiveEndpointPercent;
        }
        auto &subtrim = candidate.channelParameters[steering - 1].subtrimPermille;
        const auto oldSubtrim = before.channelParameters[steering - 1].subtrimPermille;
        if (subtrim >= -200 && subtrim <= 200 && oldSubtrim >= -200 && oldSubtrim <= 200)
            subtrim = oldSubtrim;
    }
    return !equivalentControlConfiguration(before, candidate);
}

int16_t applyConfiguredResponse(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille)
{
    if (!validChannel(channel)) return inputPermille;
    return applyChannelResponse(configuration.channelProfiles[channel - 1],
                                inputPermille);
}

int16_t applyConfiguredDriveMode(
    const ControlConfigurationSnapshot &configuration,
    int16_t configuredPermille, int16_t rawInputPermille)
{
    if (!validChannel(configuration.throttleChannel)) return configuredPermille;
    const ChannelResponseProfile &profile =
        configuration.channelProfiles[configuration.throttleChannel - 1];
    const int16_t trim = static_cast<int16_t>(profile.subtrimPercent) * 10;
    int32_t driveComponent = static_cast<int32_t>(configuredPermille) - trim;
    if (configuration.driveMode == DriveMode::Walk && rawInputPermille > 0)
        driveComponent = driveComponent * configuration.walkLimitPercent / 100;
    return static_cast<int16_t>(clampValue<int32_t>(
        driveComponent + trim, -CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE,
        CHANNEL_RESPONSE_EXTENDED_LIMIT_PERMILLE));
}

int16_t applyConfiguredChannelAdjustments(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille, int16_t switchedRatePermille)
{
    if (!validChannel(channel)) return inputPermille;
    const ChannelParameterValues &values =
        configuration.channelParameters[channel - 1];
    int32_t shifted = inputPermille;
    if (values.subtrimPermille != 0) {
        // Sub-trim re-centres the response between its existing endpoints.
        // Use the same response, switched rate and drive limits as the caller.
        const int32_t centre = applyConfiguredResponse(configuration, channel, 0);
        auto endpoint = [&](int16_t raw) {
            int16_t value = centre +
                (int32_t(applyConfiguredResponse(configuration, channel, raw)) - centre) *
                switchedRatePermille / 1000;
            if (channel == configuration.throttleChannel)
                value = applyConfiguredDriveMode(configuration, value, raw);
            // Preserve internal extended travel too: clipping here would make
            // an opposing ordinary trim unexpectedly reduce a saturated end.
            return int32_t(value);
        };
        const int32_t a = endpoint(-1000), b = endpoint(1000);
        const int32_t low = a < b ? a : b, high = a > b ? a : b;
        const int32_t neutral = clampValue<int32_t>(centre, low, high);
        const int32_t adjusted = clampValue<int32_t>(
            neutral + values.subtrimPermille, low, high);
        // A collapsed side pins neutral to an endpoint: there is no independent
        // centre adjustment possible without moving that endpoint.
        if (low < neutral && neutral < high) {
            const int32_t input = clampValue<int32_t>(inputPermille, low, high);
            if (input <= neutral)
                shifted = low + (input - low) * (adjusted - low) / (neutral - low);
            else
                shifted = adjusted + (input - neutral) * (high - adjusted) / (high - neutral);
        }
    }
    return static_cast<int16_t>(clampValue<int32_t>(
        shifted + values.trimPermille, -1000, 1000));
}

int16_t configuredChannelNeutral(
    const ControlConfigurationSnapshot &configuration, uint8_t channel)
{
    int16_t neutral = applyConfiguredResponse(configuration, channel, 0);
    if (channel == configuration.throttleChannel)
        neutral = applyConfiguredDriveMode(configuration, neutral, 0);
    return applyConfiguredChannelAdjustments(configuration, channel, neutral);
}

int16_t ChannelSlewRuntime::apply(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t targetPermille, uint32_t nowMs)
{
    if (!validChannel(channel)) return targetPermille;
    const size_t index = channel - 1;
    if (lastUpdateMs_[index] == 0) {
        lastUpdateMs_[index] = nowMs;
        slewedPermille_[index] = 0;
    }
    const uint32_t elapsed = nowMs - lastUpdateMs_[index];
    const uint32_t elapsedMs = elapsed < 100U ? elapsed : 100U;
    lastUpdateMs_[index] = nowMs;

    const bool forwardSide = targetPermille > 0 ||
        (targetPermille == 0 && slewedPermille_[index] > 0);
    const ChannelResponseProfile &profile =
        configuration.channelProfiles[index];
    const uint16_t travelMs = forwardSide ? profile.positiveRampMs
                                          : profile.negativeRampMs;
    if (travelMs == 0 || elapsedMs == 0) {
        slewedPermille_[index] = targetPermille;
        return targetPermille;
    }

    int32_t maximumStep =
        (1000L * static_cast<int32_t>(elapsedMs) + travelMs - 1) / travelMs;
    if (maximumStep < 1) maximumStep = 1;
    const int32_t delta = static_cast<int32_t>(targetPermille) -
                          slewedPermille_[index];
    slewedPermille_[index] += static_cast<int16_t>(
        clampValue<int32_t>(delta, -maximumStep, maximumStep));
    return slewedPermille_[index];
}

void ChannelSlewRuntime::reset(uint32_t nowMs)
{
    for (size_t index = 0; index < CRSF_CHANNEL_COUNT; ++index) {
        slewedPermille_[index] = 0;
        lastUpdateMs_[index] = nowMs;
    }
}

ProcessedProportionalChannel processConfiguredProportionalChannel(
    const ControlConfigurationSnapshot &configuration, uint8_t channel,
    int16_t inputPermille, int16_t safeInputPermille,
    ChannelSlewRuntime &slew, uint32_t nowMs, bool advanceSlew,
    int16_t switchedRatePermille)
{
    int16_t requested = applyConfiguredResponse(configuration, channel,
                                                inputPermille);
    // Scale response around calibrated neutral, after expo/endpoints but
    // before trim and slew. A 50% switched rate remains half travel at any expo.
    const int16_t neutralResponse=applyConfiguredResponse(configuration,channel,0);
    requested=neutralResponse+(int32_t(requested)-neutralResponse)*switchedRatePermille/1000;
    int16_t safe = applyConfiguredResponse(configuration, channel,
                                           safeInputPermille);
    if (channel == configuration.throttleChannel) {
        requested = applyConfiguredDriveMode(configuration, requested,
                                             inputPermille);
        safe = applyConfiguredDriveMode(configuration, safe,
                                        safeInputPermille);
    }
    requested = applyConfiguredChannelAdjustments(configuration, channel,
                                                  requested, switchedRatePermille);
    // Use the same active endpoint anchors for CUT. If a very low rate limits
    // subtrim authority, CUT neutral must still match the live neutral.
    safe = applyConfiguredChannelAdjustments(configuration, channel, safe,
                                             switchedRatePermille);
    if (advanceSlew)
        requested = slew.apply(configuration, channel, requested, nowMs);
    ProcessedProportionalChannel result;
    result.requestedPermille = requested;
    result.safePermille = safe;
    return result;
}

void processConfiguredProportionalOutputs(
    const ControlConfigurationSnapshot &configuration,
    CrsfChannelOutputModel &outputs, ChannelSlewRuntime &slew,
    uint32_t nowMs, bool advanceSlew, uint16_t reservedChannels)
{
    const uint16_t channels = static_cast<uint16_t>(
        configuration.proportionalOutputMask & ~reservedChannels);
    for (uint8_t channel = 1; channel <= CRSF_CHANNEL_COUNT; ++channel) {
        const uint16_t bit = static_cast<uint16_t>(1U << (channel - 1));
        if ((channels & bit) == 0) continue;
        const int16_t raw = permilleFromCrsfValue(outputs.requested(channel));
        const int16_t safeRaw = permilleFromCrsfValue(outputs.safe(channel));
        const ProcessedProportionalChannel processed =
            processConfiguredProportionalChannel(
                configuration, channel, raw, safeRaw, slew, nowMs,
                advanceSlew);
        outputs.setRequested(
            channel, crsfValueFromPermille(processed.requestedPermille));
        outputs.setSafe(channel,
                        crsfValueFromPermille(processed.safePermille));
    }
}


namespace {
// Channel sources are logical signals, before mechanical endpoint/reverse/subtrim.
int16_t mixShapedInput(const ControlConfigurationSnapshot &c,uint8_t ch,int16_t value) {
    if(ch<1||ch>16)return 0;
    auto profile=c.channelProfiles[ch-1];
    profile.negativeEndpointPercent=profile.positiveEndpointPercent=100;
    profile.reversed=false;profile.subtrimPercent=0;
    return applyChannelResponse(profile,value);
}
struct MixEvaluation {
    const ControlConfigurationSnapshot &c;
    MixInputSnapshot inputs;
    const CrsfChannelOutputModel &outputs;
    ChannelMixRuntime &runtime;
    uint32_t now;bool live;
    uint16_t done=0;
    int16_t raw[16]={},rates[16]={};
    bool valid[16]={};
    MixEvaluation(const ControlConfigurationSnapshot &config,const MixInputSnapshot &in,
                  const CrsfChannelOutputModel &out,ChannelMixRuntime &rt,uint32_t time,bool running)
        :c(config),inputs(in),outputs(out),runtime(rt),now(time),live(running) {
        for(int role=0;role<2;++role) {
            const uint8_t ch=role?c.throttleChannel:c.steeringChannel;
            const bool enabled=role?c.throttleEnabled:c.steeringEnabled;
            const int source=kMixConfiguredSource+role;
            if(ch>=1&&ch<=16&&enabled&&mixInputValid(in,role)) {
                inputs.values[source]=mixShapedInput(c,ch,in.values[role]);
                inputs.trims[source]=c.channelParameters[ch-1].trimPermille;
                inputs.valid|=uint64_t{1}<<source;
            }
        }
    }
    bool evaluate(uint8_t index) {
        if(done&(1U<<index))return valid[index];
        const auto &m=c.mixing[index];const uint8_t ch=index+1;
        const bool steer=ch==c.steeringChannel,throttle=ch==c.throttleChannel;
        const int16_t base=steer?inputs.values[0]:throttle?inputs.values[1]:
            outputs.enabled(ch)?permilleFromCrsfValue(outputs.requested(ch)):0;
        bool baseNeeded=m.includeBase;
        // An unconditional replacement discards the base, including its validity requirement.
        for(const auto &r:m.rules)if(r.enabled&&r.replace&&mixConditionActive(r,inputs,c.driveMode==DriveMode::Walk))baseNeeded=false;
        bool baseValid=steer?(c.steeringEnabled&&mixInputValid(inputs,0)):
            throttle?(c.throttleEnabled&&mixInputValid(inputs,1)):true;
        valid[index]=(!baseNeeded||baseValid)&&channelMixInputsValid(m,inputs,c.driveMode==DriveMode::Walk);
        rates[index]=1000;
        raw[index]=runtime.apply(ch,m,inputs,c.driveMode==DriveMode::Walk,base,now,live,&rates[index]);
        const int source=kMixChannelSource+index;
        inputs.values[source]=mixShapedInput(c,ch,raw[index])*rates[index]/1000;
        inputs.trims[source]=c.channelParameters[index].trimPermille;
        if(valid[index])inputs.valid|=uint64_t{1}<<source;
        done|=1U<<index;return valid[index];
    }
};
}
bool validConfiguredMixing(const ControlConfigurationSnapshot &c) {
    if (!validChannel(c.steeringChannel) || !validChannel(c.throttleChannel) ||
        c.steeringChannel == c.throttleChannel) return false;
    if(!validChannelMixGraph(c.mixing))return false;
    for(uint8_t ch=1;ch<=16;++ch)if(ch==c.steeringChannel||ch==c.throttleChannel){
        const auto &m=c.mixing[ch-1];
        if(m.cutOverride||m.outputMinimum>0||m.outputMaximum<0||channelMixUsesThreshold(c.mixing,ch-1))return false;
    }
    return true;
}
namespace {
void appendMixOrder(const ChannelMixSettings *all, uint8_t n,
                    uint16_t &done, CompiledMixPlan &plan) {
    if (done & (1U << n)) return;
    done |= 1U << n;
    for (const auto &r : all[n].rules)
        if (r.enabled && r.source >= kMixChannelSource && r.source < kMixConfiguredSource)
            appendMixOrder(all, r.source - kMixChannelSource, done, plan);
    plan.order[plan.count++] = n;
}
void evaluateMixPlan(MixEvaluation &evaluation, const CompiledMixPlan &plan) {
    for (uint8_t n = 0; n < plan.count; ++n) evaluation.evaluate(plan.order[n]);
}
}
CompiledMixPlan compileConfiguredMixing(const ControlConfigurationSnapshot &c) {
    CompiledMixPlan plan;
    if (!validConfiguredMixing(c)) return plan;
    uint16_t done = 0;
    for (uint8_t n = 0; n < CRSF_CHANNEL_COUNT; ++n) {
        appendMixOrder(c.mixing, n, done, plan);
        const bool role = (n+1 == c.steeringChannel && c.steeringEnabled) ||
                          (n+1 == c.throttleChannel && c.throttleEnabled);
        if (role || channelHasMix(c.mixing[n])) plan.requiredMask |= 1U << n;
    }
    plan.valid = true;
    return plan;
}
ConfiguredMixInspection inspectConfiguredMixInputs(const ControlConfigurationSnapshot &c,
    const MixInputSnapshot &inputs, const CrsfChannelOutputModel &bindings,
    const CompiledMixPlan &plan) {
    ConfiguredMixInspection result;
    if (!plan.valid) return result;
    ChannelMixRuntime runtime;
    MixEvaluation e(c, inputs, bindings, runtime, 0, false);
    evaluateMixPlan(e, plan);
    result.inputsValid = true;
    for (uint8_t n = 0; n < CRSF_CHANNEL_COUNT; ++n)
        if ((plan.requiredMask & (1U << n)) && !e.valid[n]) result.inputsValid = false;
    if (c.throttleChannel < 1 || c.throttleChannel > CRSF_CHANNEL_COUNT) return result;
    const auto &m = c.mixing[c.throttleChannel-1];
    const uint8_t throttleIndex = c.throttleChannel-1;
    result.throttleHasMix = m.constantPercent || !m.includeBase ||
                           m.rules[0].enabled || m.rules[1].enabled;
    result.throttlePermille = e.raw[throttleIndex];
    result.throttleValid = e.valid[throttleIndex];
    result.throttleNeutral = result.throttleValid &&
        (!result.throttleHasMix || (result.throttlePermille >= -50 && result.throttlePermille <= 50));
    return result;
}
bool configuredMixInputsValid(const ControlConfigurationSnapshot &c, const MixInputSnapshot &inputs,
                              const CrsfChannelOutputModel *bindings) {
    const CrsfChannelOutputModel empty;
    return inspectConfiguredMixInputs(c, inputs, bindings ? *bindings : empty,
                                      compileConfiguredMixing(c)).inputsValid;
}
void processConfiguredMixedOutputs(const ControlConfigurationSnapshot &c,
    CrsfChannelOutputModel &outputs,const MixInputSnapshot &inputs,
    ChannelMixRuntime &mix,ChannelSlewRuntime &slew,uint32_t now,bool live,
    const CompiledMixPlan *compiled)
{
    CompiledMixPlan fallback;
    if (!compiled) { fallback = compileConfiguredMixing(c); compiled = &fallback; }
    if (!compiled->valid) return; // safety validity gate prevents transmission
    const CrsfChannelOutputModel original=outputs;
    MixEvaluation e(c,inputs,original,mix,now,live);
    evaluateMixPlan(e, *compiled);
    for(uint8_t ch=1;ch<=16;++ch) {
        const bool steer=ch==c.steeringChannel, throttle=ch==c.throttleChannel;
        const auto &m=c.mixing[ch-1];const bool modified=channelHasMix(m);
        if(!(original.enabled(ch)||steer||throttle||modified||(c.proportionalOutputMask&(1U<<(ch-1)))))continue;
        const bool valid = e.evaluate(ch-1);
        const int16_t safeBase=(steer||throttle)?0:permilleFromCrsfValue(original.safe(ch));
        auto processed=processConfiguredProportionalChannel(c,ch,e.raw[ch-1],safeBase,slew,now,live,e.rates[ch-1]);
        // Final command bounds follow response, trims, reverse and slew.
        // Role channels must retain a neutral-containing range, even if a
        // previously configured auxiliary channel is reassigned to throttle.
        if(!(steer||throttle)|| (m.outputMinimum<=0&&m.outputMaximum>=0))
            processed.requestedPermille=limitMixedOutput(m,processed.requestedPermille);
        if(m.cutOverride&&!steer&&!throttle)processed.safePermille=int16_t(m.cutPercent)*10;
        if (!valid) processed.requestedPermille = processed.safePermille;
        outputs.configure(ch,crsfValueFromPermille(processed.requestedPermille),
            crsfValueFromPermille(processed.safePermille),
            steer?c.steeringEnabled:throttle?c.throttleEnabled:(original.enabled(ch)||modified));
    }
}
bool configuredMixedThrottleNeutral(const ControlConfigurationSnapshot &c,
    const MixInputSnapshot &inputs, const CrsfChannelOutputModel *bindings) {
    const CrsfChannelOutputModel empty;
    return inspectConfiguredMixInputs(c, inputs, bindings ? *bindings : empty,
                                      compileConfiguredMixing(c)).throttleNeutral;
}

ChannelMixPreview previewChannelMix(const ControlConfigurationSnapshot &configuration,
    uint8_t channel,const ChannelMixSettings &settings,int16_t base,
    const int16_t sources[2],bool switchHigh,bool walk)
{
    ChannelMixPreview result;
    if(channel<1||channel>16||!validChannelMix(settings))return result;
    MixInputSnapshot input;input.valid=(uint64_t{1}<<kMixSourceCount)-1;
    for(int n=0;n<2;++n) {
        const auto &r=settings.rules[n];input.values[r.control]=switchHigh?1000:-1000;
    }
    input.values[settings.rateControl]=switchHigh?1000:-1000;
    // Separate synthetic slots allow testing both contributions even when they
    // refer to the same source; callers keep those sliders in sync.
    auto test=settings;
    for(int n=0;n<2;++n) {
        const auto &r=settings.rules[n];auto &t=test.rules[n];
        uint8_t ch=r.source>=19&&r.source<35?r.source-18:
            r.source==35?configuration.steeringChannel:r.source==36?configuration.throttleChannel:0;
        t.source=30+n;input.values[t.source]=sources[n];
        input.trims[t.source]=ch>=1&&ch<=16?configuration.channelParameters[ch-1].trimPermille:0;
    }
    ChannelMixRuntime runtime;int16_t rate=1000;
    result.mixed=runtime.apply(channel,test,input,walk,base,0,false,&rate,result.contributions);
    ChannelSlewRuntime slew;auto c=configuration;c.driveMode=walk?DriveMode::Walk:DriveMode::Normal;
    result.output=limitMixedOutput(settings,processConfiguredProportionalChannel(c,channel,result.mixed,0,slew,0,false,rate).requestedPermille);
    return result;
}

void previewChannelMixCurve(const ControlConfigurationSnapshot &c,uint8_t channel,
    const ChannelMixSettings &m,int16_t base,const int16_t sources[2],
    bool high,bool walk,uint8_t sweep,int16_t output[kMixCurvePoints])
{
    for(unsigned n=0;n<kMixCurvePoints;++n){
        const int16_t x=-1000+n*2000/(kMixCurvePoints-1);
        int16_t values[]={sources[0],sources[1]};
        if(sweep==1||sweep==2){
            values[sweep-1]=x;
            if(m.rules[0].source==m.rules[1].source)values[2-sweep]=x;
        }
        output[n]=previewChannelMix(c,channel,m,sweep==0?x:base,values,high,walk).output;
    }
}

ChannelMixMonitor monitorChannelMix(const ControlConfigurationSnapshot &configuration,
    const MixInputSnapshot &inputs,const CrsfChannelOutputModel &bindings,
    uint8_t channel,const ChannelMixSettings &draft,int8_t sweep)
{
    ChannelMixMonitor monitor;
    if(channel<1 || channel>16)return monitor;
    auto c=configuration;c.mixing[channel-1]=draft;
    const auto plan=compileConfiguredMixing(c);if(!plan.valid)return monitor;
    ChannelMixRuntime runtime;
    MixEvaluation e(c,inputs,bindings,runtime,0,false);evaluateMixPlan(e,plan);
    if(!e.valid[channel-1])return monitor;
    monitor.valid=true;
    monitor.base=channel==c.steeringChannel?inputs.values[0]:channel==c.throttleChannel?inputs.values[1]:
        bindings.enabled(channel)?permilleFromCrsfValue(bindings.requested(channel)):0;
    for(unsigned n=0;n<2;++n)monitor.sources[n]=e.inputs.values[draft.rules[n].source];
    auto calculate=[&](const MixInputSnapshot &samples,int16_t base){
        ChannelMixPreview result;ChannelMixRuntime settled;int16_t rate=1000;
        result.mixed=settled.apply(channel,draft,samples,c.driveMode==DriveMode::Walk,base,0,false,&rate,result.contributions);
        ChannelSlewRuntime slew;
        result.output=limitMixedOutput(draft,processConfiguredProportionalChannel(c,channel,result.mixed,0,slew,0,false,rate).requestedPermille);
        return result;
    };
    monitor.result=calculate(e.inputs,monitor.base);
    if(sweep>=0 && sweep<=2)for(unsigned n=0;n<kMixCurvePoints;++n){
        const int16_t value=-1000+n*2000/(kMixCurvePoints-1);
        auto samples=e.inputs;
        if(sweep>0)samples.values[draft.rules[sweep-1].source]=value;
        monitor.curve[n]=calculate(samples,sweep==0?value:monitor.base).output;
    }
    return monitor;
}
