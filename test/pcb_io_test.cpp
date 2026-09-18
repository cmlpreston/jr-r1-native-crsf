#include <cassert>
#include <cstdio>
#include "Wire.h"
#include "pcb_io.h"
#include "physical_input_runtime.h"
#include "r1_input_map.h"
#include "config.h"
FakeWire Wire;
unsigned startupWaits = 0;
uint8_t recoverAddress = 0;
void delay(uint32_t ms) {
    if (ms == 100) {
        ++startupWaits;
        if (recoverAddress) Wire.missing[recoverAddress] = false;
    }
}
void delayMicroseconds(uint32_t) {}

int main()
{
    // A write ACK alone is insufficient: retry a failed read, without
    // reconfiguring the other healthy chip.
    Wire.failedReadsRemaining[0x21] = 1;
    assert(pcbIoBegin());
    assert(startupWaits == 1);
    unsigned k1Writes=0, k2Writes=0;
    for (auto w : Wire.writes) (w.address==0x20 ? k1Writes : k2Writes)++;
    assert(k1Writes==6 && k2Writes==12);
    // Either chip can come online late; a healthy one is retained.
    for (uint8_t address : {uint8_t(0x20), uint8_t(0x21)}) {
        Wire = FakeWire{};startupWaits=0;recoverAddress=address;
        Wire.missing[address]=true;
        assert(pcbIoBegin());assert(startupWaits==1);
    }
    // Permanently missing devices exhaust exactly five attempts/four waits.
    Wire = FakeWire{};startupWaits=0;recoverAddress=0;
    Wire.missing[0x20]=Wire.missing[0x21]=true;
    unsigned attempts=0;
    static unsigned reportedAttempts=0;
    assert(!pcbIoBegin([](uint8_t n,bool a,bool b){
        reportedAttempts=n;assert(!a && !b);
    }));
    attempts=reportedAttempts;
    assert(attempts==5 && startupWaits==4);
    assert(!pcbKeypad.online() && !pcbControls.online());
    Wire = FakeWire{};startupWaits=0;
    assert(pcbIoBegin());assert(startupWaits==0);
    assert(pcbResetTouch());
    assert(pcbResetDisplay());
    // Reset lines remain outputs high; INT returns to input with pull-up.
    assert((Wire.regs[0x21][1] & 9)==0);
    assert((Wire.regs[0x21][0x15] & 0x89)==0x89);
    assert(Wire.regs[0x21][1] & (1<<4));
    assert(Wire.regs[0x21][0x0d] & (1<<4));
    assert(!pcbTouchIrqActive());
    Wire.heldLow[0x21]=1<<12;
    assert(pcbTouchIrqActive());
    Wire.heldLow[0x21]=0;

    // No handset switches connected: internal pull-ups alone must release all
    // controls. Touch IRQ can assert without becoming a startup control fault.
    {
        PhysicalInputRuntime runtime;
        PhysicalInputEventBuffer events;
        assert(runtime.begin(kR1InputMap,kR1InputMapCount));
        Wire.heldLow[0x21]=1<<12;
        McpInputSnapshot samples[]={{0x20,0xffff,true},{0x21,0xffff,true}};
        assert(pcbKeypad.read(samples[0].levels));
        assert(pcbControls.read(samples[1].levels));
        assert(((~samples[0].levels)&0x7f1f)==0);
        assert(((~samples[1].levels)&0x661f)==0);
        runtime.update(samples,2,0,0,events);
        runtime.update(samples,2,0,100,events);
        assert(runtime.startupUnsafeMask()==0);
        samples[0].available=false;
        runtime.update(samples,2,0,200,events);
        assert(runtime.startupUnsafeMask()!=0);
        samples[0].available=true;
        runtime.update(samples,2,0,300,events);
        assert(runtime.startupUnsafeMask()==0);
        Wire.heldLow[0x21]=0;
    }

    // Poll/reconnect must preserve reset levels, IRQ direction and LED off.
    uint16_t ports;
    assert(pcbControls.read(ports));
    Wire.missing[0x21]=true;
    assert(!pcbControls.read(ports));
    assert(!pcbControls.online());
    Wire.missing[0x21]=false;
    Wire.writes.clear();
    assert(pcbControls.begin());
    for (auto write : Wire.writes) {
        if (write.reg==0x15) assert((write.value & 0x89)==0x89);
        if (write.reg==1) assert((write.value & 9)==0);
    }

    // Verify each physical keypad contact, including PAGE on port A, with
    // a floating-input model: released keys only read high if GPPU is set.
    const PhysicalControlId keys[] = {PhysicalControlId::KeyMode,
        PhysicalControlId::KeyUp, PhysicalControlId::KeyPlus,
        PhysicalControlId::KeySelect, PhysicalControlId::KeyMinus,
        PhysicalControlId::KeyDown, PhysicalControlId::KeyClear,
        PhysicalControlId::KeyPage};
    const uint8_t pins[]={1,13,11,12,9,10,8,3};
    for (unsigned key=0;key<8;++key) {
        Wire.heldLow[0x20]=0;
        PhysicalInputRuntime runtime;
        PhysicalInputEventBuffer events;
        assert(runtime.begin(kR1InputMap,kR1InputMapCount));
        McpInputSnapshot samples[]={{0x21,0xffff,true},{0x20,0xffff,true}};
        assert(pcbKeypad.read(samples[1].levels));
        assert(pcbControls.read(samples[0].levels));
        runtime.update(samples,2,0,0,events);
        assert(events.size()==0);
        Wire.heldLow[0x20]=1<<pins[key];
        assert(pcbKeypad.read(samples[1].levels));
        runtime.update(samples,2,0,10,events);
        runtime.update(samples,2,0,40,events);
        assert(events.size()==1);
        assert(events[0].input==keys[key]);
        assert(events[0].event==InputEvent::Pressed);
        assert(!runtime.state(PhysicalControlId::TimerButton).stablePrimary);
        // K2 failure faults its inputs while healthy K1 remains usable.
        samples[0].available=false;
        samples[0].levels=0; // deliberately stale/pressed data
        events.clear();
        runtime.update(samples,2,0,50,events);
        runtime.update(samples,2,0,80,events);
        assert(runtime.state(PhysicalControlId::DriveModeSwitch).faulted);
        assert(!runtime.state(keys[key]).faulted);
        assert(!runtime.state(PhysicalControlId::TimerButton).stablePrimary);
        assert(runtime.startupUnsafeMask()!=0);
        samples[0].available=true; samples[0].levels=0xffff;
        runtime.update(samples,2,0,100,events);
        assert(!runtime.state(PhysicalControlId::DriveModeSwitch).faulted);
    }
    // Check every trim contact including pairs split between ports A and B.
    struct Trim { PhysicalControlId id; uint8_t chip, minus, plus; };
    const Trim trims[] = {
        {PhysicalControlId::ThrottleTrim, 0x20, 2, 0},
        {PhysicalControlId::SteeringTrim, 0x20, 4, 14},
        {PhysicalControlId::GripTrimA, 0x21, 0, 2},
        {PhysicalControlId::GripTrimB, 0x21, 3, 13},
        {PhysicalControlId::Aux3Trim, 0x21, 9, 10},
    };
    for (auto trim : trims) for (bool plus : {false, true}) {
        Wire.heldLow[0x20]=Wire.heldLow[0x21]=0;
        PhysicalInputRuntime runtime;
        PhysicalInputEventBuffer events;
        assert(runtime.begin(kR1InputMap,kR1InputMapCount));
        McpInputSnapshot samples[]={{0x20,0xffff,true},{0x21,0xffff,true}};
        assert(pcbKeypad.read(samples[0].levels));
        assert(pcbControls.read(samples[1].levels));
        runtime.update(samples,2,0,0,events);
        Wire.heldLow[trim.chip]=1 << (plus ? trim.plus : trim.minus);
        assert(pcbKeypad.read(samples[0].levels));
        assert(pcbControls.read(samples[1].levels));
        runtime.update(samples,2,0,10,events);
        runtime.update(samples,2,0,40,events);
        assert(events.size()==1 && events[0].input==trim.id);
        assert(events[0].event==(plus ? InputEvent::PositivePress : InputEvent::NegativePress));
    }
    const PhysicalControlId switches[]={PhysicalControlId::TimerButton,
        PhysicalControlId::DriveModeSwitch,PhysicalControlId::IdleUpSwitch};
    const uint8_t switchPins[]={1,4,14};
    for (unsigned i=0;i<3;++i) {
        Wire.heldLow[0x20]=Wire.heldLow[0x21]=0;
        PhysicalInputRuntime runtime;
        PhysicalInputEventBuffer events;
        assert(runtime.begin(kR1InputMap,kR1InputMapCount));
        McpInputSnapshot samples[]={{0x20,0xffff,true},{0x21,0xffff,true}};
        assert(pcbKeypad.read(samples[0].levels));
        assert(pcbControls.read(samples[1].levels));
        runtime.update(samples,2,0,0,events);
        Wire.heldLow[0x21]=1 << switchPins[i];
        assert(pcbControls.read(samples[1].levels));
        runtime.update(samples,2,0,10,events);
        runtime.update(samples,2,0,40,events);
        assert(events.size()==1 && events[0].input==switches[i]);
        assert(events[0].event==InputEvent::Pressed);
    }
    // All eleven header signals must be distinct and avoid USB GPIO19/20.
    const int gpios[]={THROTTLE_ADC_PIN,STEERING_ADC_PIN,TX_BATTERY_ADC_PIN,
        CRSF_SIGNAL_PIN,BUZZER_PIN,TFT_DC_PIN,TFT_SCK_PIN,TFT_MOSI_PIN,
        TFT_BACKLIGHT_PIN,TOUCH_SDA_PIN,TOUCH_SCL_PIN};
    for (unsigned i=0;i<11;++i) {
        assert(gpios[i]!=19 && gpios[i]!=20);
        for (unsigned j=0;j<i;++j) assert(gpios[i]!=gpios[j]);
    }
    assert(BUZZER_PIN==43 && TFT_BACKLIGHT_PIN==3);
    assert(TOUCH_SDA_PIN==1 && TOUCH_SCL_PIN==2);
    assert(TFT_SCK_PIN==5 && TFT_MOSI_PIN==6 && TFT_DC_PIN==4);
    // Critical reset write errors must abort startup, never report success.
    Wire.writes.clear(); Wire.failAtWrite=1;
    assert(!pcbResetTouch());
    Wire.failAtWrite=0;
    Wire.missing[0x20]=true;
    assert(!pcbIoBegin());
    assert(!pcbKeypad.online());
    assert(THROTTLE_ADC_PIN==8 && STEERING_ADC_PIN==7);
    assert(CRSF_SIGNAL_PIN==44 && TX_BATTERY_ADC_PIN==9);
    assert(TFT_RESET_PIN==-1 && TOUCH_IRQ_PIN==-1);
    puts("pcb_io_test: PASS");
}
