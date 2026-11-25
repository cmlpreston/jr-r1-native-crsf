#include "bounded_event_queue.h"
#include "input_pipeline_policy.h"
#include "physical_input_runtime.h"
#include "control_output_mailbox.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

int main(){
    // Real debounce/event producer keeps sampling during a simulated 350ms UI
    // stall; the UI later receives both edges of a short, valid press in order.
    const PhysicalInputHardwareBinding map[]={
        {PhysicalControlId::TimerButton,{InputSourceKind::DirectGpio,0,4,false},{}}
    };
    PhysicalInputRuntime runtime;assert(runtime.begin(map,1));
    BoundedEventQueue<PhysicalInputEvent,64> pending;
    PhysicalInputEventBuffer events;
    for(uint32_t now=100;now<=450;now+=5){
        const bool down=now>=140&&now<220;
        events.clear();runtime.update(0xffff,down?uint64_t{1}<<4:0,now,events);
        assert(!events.overflowed());
        for(size_t i=0;i<events.size();++i)assert(pending.push(events[i]));
        assert(physicalInputSampleFresh(now,now));
    }
    PhysicalInputEvent event;unsigned press=0,release=0;uint32_t pressed=0,released=0;
    while(pending.pop(event)){
        if(event.event==InputEvent::Pressed){++press;pressed=event.timestampMs;}
        if(event.event==InputEvent::Released){++release;released=event.timestampMs;}
    }
    assert(press==1&&release==1&&pressed<released);
    // Queue overflow is explicit, never overwrites old events, and clearing
    // delivery retains the lost-event counter for diagnosis.
    BoundedEventQueue<unsigned,3> bounded;
    for(unsigned i=0;i<3;++i)assert(bounded.push(i));
    assert(!bounded.push(99)&&bounded.dropped()==1);unsigned n;
    for(unsigned i=0;i<3;++i){assert(bounded.pop(n));assert(n==i);}
    assert(!bounded.pop(n));bounded.clear();assert(bounded.dropped()==1);
    // Neither a new model nor a CUT may inherit a queued press/arm from before it.
    assert(inputDispatchCurrent(2,7,2,7));
    assert(!inputDispatchCurrent(1,7,2,7));
    assert(!inputDispatchCurrent(2,6,2,7));
    assert(!physicalInputSampleFresh(500,0));
    assert(physicalInputSampleFresh(500,400));
    assert(!physicalInputSampleFresh(501,400));
    assert(physicalInputSampleFresh(20,0xfffffff0u));
    assert(!physicalInputSampleFresh(200,0xfffffff0u));
    // While UI is stalled, its old revision cannot undo sampler CUT. Only an
    // acknowledged new disarmed configuration can allow fresh publication.
    ControlOutputMailbox mailbox;CrsfChannelOutputModel output;
    output.configure(2,1811,992,true);mailbox.acceptConfiguration(3);
    assert(mailbox.publish(output,OutputSafetyStatus::Armed,100,0,3));
    mailbox.requireConfiguration();
    assert(mailbox.outputs().controlSafetyStatus==OutputSafetyStatus::Cut);
    assert(!mailbox.publish(output,OutputSafetyStatus::Armed,104,0,3));
    mailbox.acceptConfiguration(4);
    assert(!mailbox.publish(output,OutputSafetyStatus::Armed,108,0,3));
    assert(mailbox.publish(output,OutputSafetyStatus::Cut,112,mailbox.commands().resetSequence,4));
    // Inject CUT after UI has captured its candidate but before accept. An old
    // armed OR disarmed revision must fail until it observes the new CUT epoch.
    const auto staleEpoch=mailbox.commands().physicalCutSequence;
    mailbox.requestPhysicalCut();
    assert(!mailbox.acceptConfiguration(5,true,staleEpoch));
    assert(!mailbox.acceptConfiguration(5,false,staleEpoch));
    assert(mailbox.commands().configurationPending&&mailbox.commands().physicalCutPending);
    const auto cutEpoch=mailbox.commands().physicalCutSequence;
    assert(!mailbox.acceptConfiguration(5,true,cutEpoch));
    assert(mailbox.acceptConfiguration(5,false,cutEpoch));
    assert(!mailbox.commands().physicalCutPending);
    puts("Input pipeline: blocked-UI short press, debounce/FIFO/overflow, model/CUT epochs, heartbeat/wrap and mailbox veto PASS");
}
