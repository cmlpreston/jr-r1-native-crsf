#include "pcb_io.h"

#ifdef JR_R1_XIAO_PCB
Mcp23017Input pcbKeypad(0x20);
Mcp23017Input pcbControls(0x21);

bool pcbIoBegin()
{
    // K1: keypad A1/A3/B0..B5, trims A0/A2/A4/B6. A7/B7 are output-only.
    const bool keypad = pcbKeypad.configure(0x7f7f, 0x7f1f, 0);
    // K2: B3 LCD reset, B0 touch reset, B7 active-low RUN LED.
    // Start both resets asserted, RUN off. B4 is temporarily driven during
    // address selection, then returned to an input with a pull-up.
    const bool controls = pcbControls.configure(0x767f, 0x761f, 0x8000);
    return keypad && controls;
}

bool pcbResetTouch()
{
    // GT911 address 0x5d: INT low before RESET rises. Preload each output
    // latch before enabling its driver, and propagate every I2C failure.
    if (!pcbControls.setOutput(PCB_TOUCH_RESET_PORT, false) ||
        !pcbControls.setOutput(PCB_TOUCH_IRQ_PORT, false)) return false;
    delay(11);
    delayMicroseconds(110);
    if (!pcbControls.writeOutput(PCB_TOUCH_RESET_PORT, true)) return false;
    delay(6);
    delay(55);
    if (!pcbControls.setInput(PCB_TOUCH_IRQ_PORT, true)) return false;
    delay(100);
    return true;
}

bool pcbResetDisplay()
{
    if (!pcbControls.setOutput(PCB_LCD_RESET_PORT, false)) return false;
    delay(20);
    if (!pcbControls.writeOutput(PCB_LCD_RESET_PORT, true)) return false;
    delay(120);
    return true;
}

bool pcbTouchIrqActive()
{
    uint16_t ports = 0xffff;
    return pcbControls.online() && pcbControls.read(ports) &&
           (ports & (1U << PCB_TOUCH_IRQ_PORT)) == 0;
}
#endif
