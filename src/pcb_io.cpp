#include "pcb_io.h"

#ifdef JR_R1_XIAO_PCB
Mcp23017Input pcbKeypad(0x20);
Mcp23017Input pcbControls(0x21);

bool pcbIoBegin(PcbStartupProgress progress)
{
    bool keypad = false;
    bool controls = false;
    for (uint8_t attempt = 1; attempt <= 5; ++attempt) {
        // Reconfigure only failed chips; keep both reset outputs asserted
        // until both MCPs have also passed a fresh GPIO register read.
        if (!keypad) keypad = pcbKeypad.configure(0x7f7f, 0x7f1f, 0);
        if (!controls) controls = pcbControls.configure(0x767f, 0x761f, 0x8000);
        uint16_t ports;
        if (keypad) keypad = pcbKeypad.read(ports);
        if (controls) controls = pcbControls.read(ports);
        if (progress) progress(attempt, keypad, controls);
        if (keypad && controls) return true;
        if (attempt < 5) delay(100);
    }
    return false;
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
