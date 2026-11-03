#include "rf_link_warning.h"
#include <cassert>
#include <cstdio>
int main(){
    assert(elrs24Sensitivity(27,4)==-108&&elrs24Sensitivity(7,3)==-108);
    assert(elrs24Sensitivity(21,4)==-115&&elrs24Sensitivity(2,3)==-115);
    assert(elrs24Sensitivity(29,4)==-105&&elrs24Sensitivity(9,3)==-105);
    assert(elrs24Sensitivity(7,4)==0&&elrs24Sensitivity(27,3)==0);
    assert(elrs24Sensitivity(255,4)==0&&elrs24Sensitivity(1,4)==0);
    RfWarningLatch warning;
    assert(!warning.update(true,true,false,100));
    assert(!warning.update(true,true,false,1099));
    assert(warning.update(true,true,false,1100)&&warning.active());
    assert(!warning.update(true,false,false,1500)&&warning.active()); // recovery dead band
    assert(warning.update(true,true,false,6100));
    assert(!warning.update(true,false,true,6200)&&!warning.active());
    assert(!warning.update(true,true,false,7000));
    assert(!warning.update(true,false,false,7500)); // short transient cancelled
    assert(!warning.update(true,true,false,8000));
    assert(!warning.update(false,true,false,9100)&&!warning.active()); // stale/disabled/unknown
    assert(!warning.update(true,true,false,10000));
    assert(warning.update(true,true,false,11000));
    puts("RF sensitivity version/band guards, dwell, hysteresis, repeat and stale suppression PASS");
}
