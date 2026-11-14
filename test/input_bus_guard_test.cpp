#include "input_bus_guard.h"
#include "mcp23017_input.h"
#include "Wire.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

FakeWire Wire;
void delay(uint32_t) {}
void delayMicroseconds(uint32_t) {}

int main()
{
    Mcp23017Input keypad(0x20),controls(0x21);
    uint16_t ports=0x1234;
    // A missing lock must not silently touch Wire or mutate driver state.
    assert(!keypad.begin()&&!keypad.configure(0xffff,0xffff,0));
    assert(!keypad.read(ports)&&ports==0x1234);
    assert(!keypad.setOutput(0,true)&&!keypad.setInput(0,true)&&!keypad.writeOutput(0,true));
    assert(!keypad.online()&&keypad.readErrors()==UINT32_MAX);
    assert(Wire.tx.empty()&&Wire.writes.empty());
    assert(InputBus::begin());
    assert(keypad.configure(0xffff,0xffff,0)); // configure -> begin recursively owns the same mutex
    assert(controls.configure(0xffff,0xffff,0));
    {
        InputBus::Guard outer;assert(outer);
        InputBus::Guard inner;assert(inner);
        assert(keypad.online()&&keypad.readErrors()==0);
    }
    // A GT-style register-select/repeated-start/read sequence must stay atomic
    // against an MCP operation on the same Wire object. Exercise the actual
    // MCP public method and shared guard with a real competing host thread.
    for(unsigned round=0;round<32;++round){
        std::atomic<bool> selected{false},attempted{false},finished{false};
        std::thread reader([&](){
            InputBus::Guard bus;assert(bus);
            Wire.beginTransmission(0x20);Wire.write(0x12);
            assert(Wire.endTransmission(false)==0);selected.store(true);
            while(!attempted.load())std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            assert(!finished.load());
            assert(Wire.requestFrom(0x20,2)==2);
            const auto value=uint16_t(Wire.read())|uint16_t(Wire.read())<<8;
            assert(value==0xffff);
        });
        std::thread writer([&](){
            while(!selected.load())std::this_thread::yield();
            attempted.store(true);
            assert(controls.writeOutput(8,(round&1)!=0));
            finished.store(true);
        });
        reader.join();writer.join();assert(finished.load());
    }
    assert(keypad.read(ports)&&ports==0xffff);
    assert(keypad.online()&&controls.online());
    puts("Shared input bus: fail-closed startup, recursive ownership, complete repeated-start reads and competing MCP operations PASS");
}
