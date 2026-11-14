// Include the actual production sampler/dispatch implementation. FreeRTOS,
// GPIO/I2C and the UI boundary are deterministic host seams; debounce, binding
// resolution, bounded queues, epoch checks and the safety mailbox are real.
#define JR_R1_XIAO_PCB 1
#include "bounded_event_queue.h"
#include "input_pipeline_policy.h"
#include "physical_input_runtime.h"
#include "binding_resolver.h"
#include "control_output_mailbox.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <vector>

using TickType_t=uint32_t;
using TaskHandle_t=void *;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) (ms)
constexpr unsigned MCP23017_POLL_PERIOD_MS=5;
constexpr bool STARTUP_IGNORE_HELD_INPUTS=false;
uint32_t nowMs=100;
uint32_t millis(){return nowMs;}
uint32_t micros(){return nowMs*1000;}
TickType_t xTaskGetTickCount(){return nowMs;}
struct EndSample{};
void vTaskDelayUntil(TickType_t *,TickType_t){throw EndSample{};}
struct Console{template<class... Args>void printf(const char *,Args...){}}Serial;
struct Chip{
    bool available=true;uint16_t levels=0;
    bool online()const{return available;}
    bool begin(){return available;}
    bool read(uint16_t &out){out=levels;return available;}
}pcbKeypad,pcbControls;
std::atomic<uint32_t> inputBindingEpoch{1},inputSafetyEpoch{1},inputLastSampleMs{0};
portMUX_TYPE bindingResolverMux=0;
PhysicalInputRuntime physicalInputs;
PhysicalInputEventBuffer physicalInputEvents;
BindingResolver bindingResolver;
ControlOutputMailbox mailbox;
std::vector<ControlBinding> uiBindings;
void requestPhysicalControlCut(){++inputSafetyEpoch;mailbox.requestPhysicalCut();}
void requestControlReset(){mailbox.requestReset();}
BindingLoadResult installUiBindings(){
    const auto result=bindingResolver.setBindings(uiBindings.data(),uiBindings.size());++inputBindingEpoch;return result;
}
struct ControlState{int16_t rawPermille=0,steeringRawPermille=0;bool armed=false;}control;
ControlState readControlState(){return control;}
uint64_t readDirectGpioLevels(){return 0;}
void(*afterSampling)()=nullptr;
void publishMixInputs(){if(afterSampling){auto fn=afterSampling;afterSampling=nullptr;fn();}}
struct Ui {
    bool arm=false,bindingsChanged=false,eventsPending=false;
    unsigned armCalls=0,cuts=0,parameterCalls=0,pressed=0,released=0;
    uint8_t activeControlContext()const{return ContextDashboard;}
    void requestPhysicalInputCut(){arm=false;++cuts;++inputSafetyEpoch;mailbox.requireConfiguration();}
    bool consumeBindingsChanged(){const bool result=bindingsChanged;bindingsChanged=false;return result;}
    void setPhysicalInputEventsPending(bool pending){eventsPending=pending;}
    bool handleKeypadEvent(PhysicalControlId,InputEvent event,uint32_t){
        pressed+=event==InputEvent::Pressed;released+=event==InputEvent::Released;return true;
    }
    void applyChannelParameterAction(uint16_t,int16_t){++parameterCalls;}
    void applyTransmitterFunctionAction(uint16_t id,int16_t value){
        if(id==unsigned(TransmitterFunction::ArmCut)){arm=value>0;++armCalls;}
    }
    void applyDriveModeAction(uint16_t id,int16_t value){
        if(id==unsigned(DriveModeSelection::CutArm)){arm=value>0;++armCalls;}
    }
}transmitterUi;
#include "../src/main_input_pipeline.inc"

void sample(uint32_t now){nowMs=now;try{inputTask(nullptr);}catch(const EndSample &){};}
void reset(){
    inputDispatch={};pendingPhysicalCut=false;latestInputSample={};transmitterUi={};
    mailbox={};control={};afterSampling=nullptr;pcbKeypad={};pcbControls={};
    inputBindingEpoch=1;inputSafetyEpoch=1;inputLastSampleMs=0;bindingResolver.clear();
    const PhysicalInputHardwareBinding hardware[]={
        {PhysicalControlId::KeyPlus,{InputSourceKind::Mcp23017,0x20,0,false},{}},
        {PhysicalControlId::IdleUpSwitch,{InputSourceKind::Mcp23017,0x20,1,false},{}}
    };
    assert(physicalInputs.begin(hardware,2));sample(100);inputDispatch.clear();
    dispatchQueuedInputs(100); // Synchronise persistent diagnostic counters.
    transmitterUi={};
}
ControlBinding armBinding(){
    ControlBinding b;b.input=PhysicalControlId::IdleUpSwitch;b.event=InputEvent::Pressed;
    b.target=AssignmentTarget::DriveMode;b.targetId=unsigned(DriveModeSelection::CutArm);
    b.behaviour=ControlBehaviour::TwoPosition;b.minimum=0;b.maximum=1000;b.initial=b.failsafe=0;
    return b;
}
void samplerDuringUiStall(){
    reset();
    for(uint32_t now=105;now<=450;now+=5){
        pcbKeypad.levels=now>=140&&now<220?1:0;sample(now);
    }
    assert(physicalInputSampleFresh(450,inputLastSampleMs.load()));
    dispatchQueuedInputs(450);
    assert(transmitterUi.pressed==1&&transmitterUi.released==1);
    pcbControls.available=false;sample(455);
    assert(!readInputSample().available&&inputLastSampleMs.load()==0);
    assert(!physicalInputSampleFresh(455,inputLastSampleMs.load())&&mailbox.commands().physicalCutPending);
    dispatchQueuedInputs(455);assert(!transmitterUi.arm&&transmitterUi.cuts);
    pcbControls.available=true;sample(460);
    assert(physicalInputSampleFresh(460,inputLastSampleMs.load()));
    assert(!physicalInputSampleFresh(561,inputLastSampleMs.load()));
}
void sampledEpochs(){
    reset();auto b=armBinding();assert(bindingResolver.setBindings(&b,1).ok());
    pcbKeypad.levels=2;sample(120);
    const auto sampledSafety=inputSafetyEpoch.load();
    afterSampling=[](){requestPhysicalControlCut();};sample(150);
    std::vector<InputDispatch> observed;InputDispatch item;bool foundArm=false;
    while(inputDispatch.pop(item)){
        observed.push_back(item);
        if(inputArmAction(item.action)&&item.action.value>0){foundArm=true;assert(item.safetyEpoch==sampledSafety);}
    }
    assert(foundArm);
    for(const auto &event:observed)assert(inputDispatch.push(event));
    dispatchQueuedInputs(150);
    assert(!transmitterUi.arm&&transmitterUi.armCalls==0); // Old edge cannot inherit new CUT epoch.
    reset();b=armBinding();b.target=AssignmentTarget::CrsfChannel;b.crsfChannel=4;
    assert(bindingResolver.setBindings(&b,1).ok());pcbKeypad.levels=2;sample(120);
    afterSampling=[](){++inputBindingEpoch;};sample(150);
    assert(bindingResolver.value(0)==0); // Old edge never resolves against replacement bindings.
}
void staleArmAndOverflow(){
    reset();InputDispatch item;item.bindingEpoch=inputBindingEpoch.load();item.safetyEpoch=inputSafetyEpoch.load();
    item.action.target=AssignmentTarget::DriveMode;item.action.targetId=unsigned(DriveModeSelection::CutArm);item.action.value=1000;
    queueInputDispatch(item);immediatePhysicalCut();dispatchQueuedInputs(200);
    assert(!transmitterUi.arm&&transmitterUi.armCalls==0);
    // A stale UI candidate captured before CUT cannot acknowledge the new veto.
    const auto old=mailbox.commands();immediatePhysicalCut();
    assert(!mailbox.acceptConfiguration(10,true,old.physicalCutSequence));
    assert(!mailbox.acceptConfiguration(11,false,old.physicalCutSequence));
    const auto fresh=mailbox.commands();
    assert(!mailbox.acceptConfiguration(12,true,fresh.physicalCutSequence));
    assert(mailbox.acceptConfiguration(13,false,fresh.physicalCutSequence));
    reset();item={};item.keypad=true;item.event.input=PhysicalControlId::KeyPlus;item.event.event=InputEvent::Pressed;
    item.bindingEpoch=inputBindingEpoch.load();item.safetyEpoch=inputSafetyEpoch.load();
    for(unsigned n=0;n<40;++n)queueInputDispatch(item);
    dispatchQueuedInputs(300);assert(transmitterUi.eventsPending&&transmitterUi.pressed==32);
    dispatchQueuedInputs(305);assert(!transmitterUi.eventsPending&&transmitterUi.pressed==40);
    for(unsigned n=0;n<65;++n)queueInputDispatch(item);
    assert(inputDispatch.dropped()==1&&mailbox.commands().physicalCutPending);
    dispatchQueuedInputs(310);assert(inputDispatch.size()==0&&!transmitterUi.arm);
}
int main(){samplerDuringUiStall();sampledEpochs();staleArmAndOverflow();
    puts("Production input pipeline: stalled UI edges, sampler health, frame epochs, stale arm, publication race, backlog and overflow PASS");}
