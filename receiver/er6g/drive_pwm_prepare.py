"""Keep ER6G output 5 released until its first valid servo-PWM command.

Applied only to the selectable single-receiver candidate, never stock/listener
profiles. Pinned upstream functions remain the source exercised by host tests.
"""
from pathlib import Path

def apply(source):
    path=Path(source)
    text=path.read_text()
    def replace(old,new):
        nonlocal text
        if text.count(old)!=1:raise ValueError('Drive PWM integration anchor changed: '+old[:70])
        text=text.replace(old,new,1)
    replace('static bool initialized = false;', '''static bool initialized = false;
// Only a complete, model-matched channels update may allocate the ESC PWM pin.
// A connection event or pre-input failsafe must not pull the shared wire LOW.
static bool er6gEscPwmFrameReady = false;
static bool er6gDeferredEscPwm(uint8_t ch)
{
    return GPIO_PIN_PWM_OUTPUTS[ch] == 2 && config.GetPwmChannel(ch)->val.mode <= som400Hz;
}''')
    replace('    const eServoOutputMode chMode = (eServoOutputMode)chConfig->val.mode;\n    if (chMode == somDShot', '''    const eServoOutputMode chMode = (eServoOutputMode)chConfig->val.mode;
    if (er6gDeferredEscPwm(ch) && pwmChannels[ch] == -1)
    {
        if (servoPins[ch] == UNDEF_PIN || !er6gEscPwmFrameReady || us == 0)
            return;
        // Allocate and apply the first pulse in this same call, never seconds apart.
        // LEDC avoids reinitialising a shared MCPWM timer after other servos started.
        pwmChannels[ch] = PWM.allocateLedc(servoPins[ch], servoOutputModeToFrequency(chMode), us);
        if (pwmChannels[ch] == -1)
        {
            pinMode(servoPins[ch], INPUT_PULLUP);
            return; // No write to an invalid PWM channel; retry on a later valid frame.
        }
        pwmChannelValues[ch] = UINT16_MAX;
    }
    if (chMode == somDShot''')
    replace('    if (connectionState != connected || !connectionHasModelMatch || !teamraceHasModelMatch)',
            '    if (connectionState != connected || !connectionHasModelMatch)')
    replace('        servoCalcAllChannels(&servoWrite);', '''        er6gEscPwmFrameReady = true;
        servoCalcAllChannels(&servoWrite);
        er6gEscPwmFrameReady = false;''')
    replace('        servoPins[ch] = pin;\n        // Initialize all servos to low ASAP', '''        servoPins[ch] = pin;
        if (pin != UNDEF_PIN && er6gDeferredEscPwm(ch))
        {
            pinMode(pin, INPUT_PULLUP);
            continue; // Keep ESC data idle/released until the first valid PWM value.
        }
        // Initialize all other servos to low ASAP''')
    replace('            if (frequency && servoPins[ch] != UNDEF_PIN)',
            '            if (frequency && servoPins[ch] != UNDEF_PIN && !er6gDeferredEscPwm(ch))')
    replace('            servoPins[ch] = UNDEF_PIN;\n        }\n        return DURATION_NEVER;', '''            if (er6gDeferredEscPwm(ch) && !er6gEscListenOwnsPin(GPIO_PIN_PWM_OUTPUTS[ch]))
                pinMode(GPIO_PIN_PWM_OUTPUTS[ch], INPUT_PULLUP);
            servoPins[ch] = UNDEF_PIN;
        }
        return DURATION_NEVER;''')
    path.write_text(text)

    header=path.parents[1]/'PWM/PWM.h'
    h=header.read_text();anchor='    pwm_channel_t allocate(uint8_t pin, uint32_t frequency);'
    if h.count(anchor)!=1:raise ValueError('PWM API anchor changed')
    header.write_text(h.replace(anchor,anchor+"\n#if defined(PLATFORM_ESP32)\n    // Dedicated deferred ESC output: avoid resetting another servo's MCPWM timer.\n    pwm_channel_t allocateLedc(uint8_t pin, uint32_t frequency, uint16_t initialMicroseconds);\n#endif",1))
    driver=path.parents[1]/'PWM/PWM_ESP32.cpp';d=driver.read_text()
    anchor='pwm_channel_t PWMController::allocate(uint8_t pin, uint32_t frequency)\n{'
    if d.count(anchor)!=1:raise ValueError('PWM allocator anchor changed')
    d=d.replace(anchor,'static pwm_channel_t allocatePwm(uint8_t pin, uint32_t frequency, bool allowMcpwm, uint16_t initialMicroseconds)\n{',1)
    anchor='    // 1a. see if theres a MCPWM already using this frequency we can piggy-back on'
    if d.count(anchor)!=1:raise ValueError('MCPWM allocator start changed')
    d=d.replace(anchor,'    if (allowMcpwm) {\n'+anchor,1)
    anchor='    // 2. try for a LEDC channel'
    if d.count(anchor)!=1:raise ValueError('MCPWM allocator end changed')
    d=d.replace('#endif\n'+anchor,'    }\n#endif\n'+anchor,1)
    anchor='void PWMController::release(pwm_channel_t channel)'
    if d.count(anchor)!=1:raise ValueError('PWM release anchor changed')
    d=d.replace(anchor,"pwm_channel_t PWMController::allocate(uint8_t pin, uint32_t frequency)\n{\n    return allocatePwm(pin, frequency, true, 0);\n}\n\npwm_channel_t PWMController::allocateLedc(uint8_t pin, uint32_t frequency, uint16_t initialMicroseconds)\n{\n    return allocatePwm(pin, frequency, false, initialMicroseconds);\n}\n\n"+anchor,1)
    anchor='static void ledcAttachPinEx(uint8_t pin, uint8_t chan, ledc_timer_t timer)'
    if d.count(anchor)!=1:raise ValueError('LEDC attach signature changed')
    d=d.replace(anchor,anchor[:-1]+', uint32_t initialDuty = 0)',1)
    if d.count('        .duty = 0,')!=1:raise ValueError('LEDC initial duty anchor changed')
    d=d.replace('        .duty = 0,','        .duty = initialDuty,',1)
    anchor='                    ledcAttachPinEx(pin, ch, (ledc_timer_t)timer_idx);'
    if d.count(anchor)!=1:raise ValueError('LEDC allocation attach changed')
    d=d.replace(anchor,"                    const uint32_t initialDuty = uint64_t(initialMicroseconds) * ((1U << bits) - 1) / (1000000U / frequency);\n                    ledcAttachPinEx(pin, ch, (ledc_timer_t)timer_idx, initialDuty);",1)
    driver.write_text(d)
