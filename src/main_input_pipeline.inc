// The sampler owns physicalInputs after startup. LVGL, model parameters and page
// navigation remain UI-owned; only bounded immutable messages cross that boundary.
struct InputDispatch {
    PhysicalInputEvent event;
    ResolvedBindingAction action;
    uint32_t bindingEpoch=0,safetyEpoch=0;
    bool keypad=false;
};
struct InputSampleSnapshot {
    PhysicalInputStateStore states;
    McpInputSnapshot chips[2]={{0x20,0xffff,false},{0x21,0xffff,false}};
    uint32_t sampledMs=0,unsafeMask=0,maximumGapUs=0;
    bool ready=false,available=false;
};
BoundedEventQueue<InputDispatch,64> inputDispatch;
InputSampleSnapshot latestInputSample;
portMUX_TYPE inputDispatchMux=portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t inputTaskHandle=nullptr;
std::atomic<uint8_t> inputUiContext{ContextDashboard};
std::atomic<bool> pendingPhysicalCut{false};

bool inputArmAction(const ResolvedBindingAction &a) {
    return (a.target==AssignmentTarget::DriveMode&&a.targetId==unsigned(DriveModeSelection::CutArm))||
        (a.target==AssignmentTarget::TransmitterFunction&&a.targetId==unsigned(TransmitterFunction::ArmCut));
}
void immediatePhysicalCut(){
    pendingPhysicalCut.store(true);
    requestPhysicalControlCut(); // Epoch-acknowledged veto, including a racing UI publication.
}
void queueInputDispatch(const InputDispatch &item) {
    portENTER_CRITICAL(&inputDispatchMux);
    const bool ok=inputDispatch.push(item);
    portEXIT_CRITICAL(&inputDispatchMux);
    if(!ok)immediatePhysicalCut(); // Overflow cannot silently lose a CUT or release.
}
InputSampleSnapshot readInputSample(){
    portENTER_CRITICAL(&inputDispatchMux);
    auto sample=latestInputSample;
    portEXIT_CRITICAL(&inputDispatchMux);
    return sample;
}
PhysicalInputStateStore currentPhysicalInputStates(){
    return inputTaskHandle?readInputSample().states:physicalInputs.states();
}
uint32_t currentStartupUnsafeMask(){
    return inputTaskHandle?readInputSample().unsafeMask:physicalInputs.startupUnsafeMask(STARTUP_IGNORE_HELD_INPUTS);
}
void inputTask(void *) {
    TickType_t nextWake=xTaskGetTickCount();uint32_t lastUs=micros(),maxGap=0,nextProbe=0;
    bool previousAvailable=true;
    for(;;){
        const uint32_t safetyEpoch=inputSafetyEpoch.load(); // Before sampling or resolving any event.
        const uint32_t frameBindingEpoch=inputBindingEpoch.load();
        const uint32_t now=millis(),nowUs=micros();
        const uint32_t gap=nowUs-lastUs;lastUs=nowUs;if(gap>maxGap)maxGap=gap;
        InputSampleSnapshot sample;sample.sampledMs=now;sample.maximumGapUs=maxGap;
#ifdef JR_R1_XIAO_PCB
        if(int32_t(now-nextProbe)>=0){
            nextProbe=now+1000;
            if(!pcbKeypad.online())pcbKeypad.begin();
            if(!pcbControls.online())pcbControls.begin();
        }
        sample.chips[0].available=pcbKeypad.online()&&pcbKeypad.read(sample.chips[0].levels);
        sample.chips[1].available=pcbControls.online()&&pcbControls.read(sample.chips[1].levels);
        sample.available=sample.chips[0].available&&sample.chips[1].available;
        physicalInputEvents.clear();
        physicalInputs.update(sample.chips,2,readDirectGpioLevels(),now,physicalInputEvents);
#else
        if(!inputExpander.online()&&int32_t(now-nextProbe)>=0){nextProbe=now+1000;inputExpander.begin();}
        uint16_t ports=0xffff;sample.available=inputExpander.online()&&inputExpander.read(ports);
        physicalInputEvents.clear();physicalInputs.update(ports,readDirectGpioLevels(),now,physicalInputEvents,sample.available);
#endif
        publishMixInputs();
        const auto control=readControlState();
        physicalInputs.setProportionalValue(PhysicalControlId::ThrottleTrigger,control.rawPermille,now);
        physicalInputs.setProportionalValue(PhysicalControlId::SteeringWheel,control.steeringRawPermille,now);
        sample.states=physicalInputs.states();sample.unsafeMask=physicalInputs.startupUnsafeMask(STARTUP_IGNORE_HELD_INPUTS);
        sample.ready=true;
        portENTER_CRITICAL(&inputDispatchMux);latestInputSample=sample;portEXIT_CRITICAL(&inputDispatchMux);
        inputLastSampleMs.store(sample.available?now:0); // Health includes live MCP availability, not just task liveness.
        if((previousAvailable&&!sample.available)||physicalInputEvents.overflowed())immediatePhysicalCut();
        previousAvailable=sample.available;
        for(size_t i=0;i<physicalInputEvents.size();++i){
            const auto &event=physicalInputEvents[i];
            InputDispatch item;item.event=event;
            if(event.input>=PhysicalControlId::KeyMode&&event.input<=PhysicalControlId::KeyMinus){
                item.keypad=true;item.bindingEpoch=frameBindingEpoch;item.safetyEpoch=safetyEpoch;
                queueInputDispatch(item);continue;
            }
            ResolvedBindingActionBuffer actions;
            portENTER_CRITICAL(&bindingResolverMux);
            const auto epoch=inputBindingEpoch.load();
            if(epoch!=frameBindingEpoch){portEXIT_CRITICAL(&bindingResolverMux);continue;}
            bindingResolver.processEvent(event,uint8_t(ContextAlways|inputUiContext.load()|
                (control.armed?ContextArmed:ContextCut)),actions);
            portEXIT_CRITICAL(&bindingResolverMux);
            if(actions.overflowed())immediatePhysicalCut();
            for(size_t n=0;n<actions.size();++n){
                const auto &action=actions[n];
                if(action.target==AssignmentTarget::CrsfChannel)continue; // Resolver already updated; no UI owner required.
                if(inputArmAction(action)&&action.value<=0)immediatePhysicalCut();
                item.action=action;item.bindingEpoch=epoch;item.safetyEpoch=safetyEpoch;
                queueInputDispatch(item);
            }
        }
        vTaskDelayUntil(&nextWake,pdMS_TO_TICKS(MCP23017_POLL_PERIOD_MS));
        // No catch-up burst following an I2C timeout.
        if(int32_t(xTaskGetTickCount()-nextWake)>int32_t(pdMS_TO_TICKS(MCP23017_POLL_PERIOD_MS)))nextWake=xTaskGetTickCount();
    }
}
void resetQueuedKeypadState(uint32_t now){
    for(unsigned i=unsigned(PhysicalControlId::KeyMode);i<=unsigned(PhysicalControlId::KeyMinus);++i)
        transmitterUi.handleKeypadEvent(PhysicalControlId(i),InputEvent::InvalidState,now);
}
void synchronizeUiBindings(){
    if(!transmitterUi.consumeBindingsChanged())return;
    requestControlReset();const auto result=installUiBindings();
    resetQueuedKeypadState(millis());
    Serial.printf("BINDINGS: TFT profile reload %s\n",result.ok()?"ready":result.reason?result.reason:"FAILED");
}
void dispatchQueuedInputs(uint32_t now){
    static uint32_t reportedDrops=0,lastSafetyEpoch=0;
    inputUiContext.store(transmitterUi.activeControlContext());
    uint32_t drops;
    portENTER_CRITICAL(&inputDispatchMux);drops=inputDispatch.dropped();portEXIT_CRITICAL(&inputDispatchMux);
    if(drops!=reportedDrops){
        portENTER_CRITICAL(&inputDispatchMux);inputDispatch.clear();portEXIT_CRITICAL(&inputDispatchMux);
        resetQueuedKeypadState(now);reportedDrops=drops;
        Serial.printf("INPUT: queue overflow; outputs CUT, dropped=%lu\n",(unsigned long)drops);
    }
    if(pendingPhysicalCut.exchange(false))
        transmitterUi.requestPhysicalInputCut();
    synchronizeUiBindings();
    for(unsigned n=0;n<32;++n){
        InputDispatch item;portENTER_CRITICAL(&inputDispatchMux);
        const bool available=inputDispatch.pop(item);portEXIT_CRITICAL(&inputDispatchMux);
        if(!available)break;
        const auto safetyEpoch=inputSafetyEpoch.load();
        if(lastSafetyEpoch!=safetyEpoch){resetQueuedKeypadState(now);lastSafetyEpoch=safetyEpoch;}
        if(!inputDispatchCurrent(item.bindingEpoch,item.safetyEpoch,inputBindingEpoch.load(),safetyEpoch))continue;
        if(item.keypad)transmitterUi.handleKeypadEvent(item.event.input,item.event.event,item.event.timestampMs);
        else{
            // A CUT observed while UI was busy invalidates every older arm request.
            if(inputArmAction(item.action)&&item.action.value>0&&item.safetyEpoch!=inputSafetyEpoch.load())continue;
            const auto &a=item.action;
            if(a.target==AssignmentTarget::ModelParameter)transmitterUi.applyChannelParameterAction(a.targetId,a.value);
            else if(a.target==AssignmentTarget::TransmitterFunction)transmitterUi.applyTransmitterFunctionAction(a.targetId,a.value);
            else if(a.target==AssignmentTarget::DriveMode)transmitterUi.applyDriveModeAction(a.targetId,a.value);
        }
        synchronizeUiBindings(); // Drop remaining old-model actions before they can touch the new model.
        inputUiContext.store(transmitterUi.activeControlContext());
    }
    portENTER_CRITICAL(&inputDispatchMux);const bool pending=inputDispatch.size()!=0;portEXIT_CRITICAL(&inputDispatchMux);
    transmitterUi.setPhysicalInputEventsPending(pending);
    // Catch a CUT that arrived during an expensive callback above, before publication.
    if(pendingPhysicalCut.exchange(false))
        transmitterUi.requestPhysicalInputCut();
}
