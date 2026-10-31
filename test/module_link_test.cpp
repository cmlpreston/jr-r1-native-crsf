#include "module_link.h"
#include <cassert>
#include <cstdio>
int main(){
    ModuleLink link;
    assert(!link.enabled()&&!link.enable()&&!link.prompt());
    link.update(true,100);link.update(true,349);assert(!link.present());
    link.update(false,350);link.update(true,400);link.update(true,650);
    assert(link.present()&&link.prompt()&&!link.enabled());
    link.dismiss();link.update(true,5000);assert(!link.prompt()&&!link.enabled());
    assert(link.enable()&&link.enabled());
    link.pause();assert(link.present()&&!link.enabled()&&!link.prompt());
    assert(link.enable());link.update(false,5001);
    assert(!link.present()&&!link.enabled()&&!link.enable());
    link.update(true,6000);link.update(true,6250);assert(link.prompt()&&!link.enabled());
    ModuleLink reboot;assert(!reboot.enabled()&&!reboot.enable());
    ModuleLink rollover;rollover.update(true,0xffffff80u);rollover.update(true,0x7au);
    assert(rollover.present()&&rollover.prompt()&&!rollover.enabled());
    puts("Module link: boot default, detection debounce, approval, cancel, pause, power loss and rollover PASS");
}
