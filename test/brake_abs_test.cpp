#include "brake_abs.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
    BrakeAbsSettings s;s.enabled=s.forwardBrakeVerified=true;
    uint8_t record[kBrakeAbsRecordSize];assert(encodeBrakeAbs(s,record,sizeof(record)));
    BrakeAbsSettings copy;assert(decodeBrakeAbs(record,sizeof(record),copy)&&equalBrakeAbs(s,copy));
    for(unsigned i=0;i<sizeof(record);++i){record[i]^=1;const auto old=copy;
        assert(!decodeBrakeAbs(record,sizeof(record),copy)&&equalBrakeAbs(old,copy));record[i]^=1;}
    auto bad=s;bad.forwardBrakeVerified=false;assert(!validBrakeAbs(bad));
    BrakeAbsRuntime r;
    assert(r.apply(s,-1000,-1000,0,false,100,true)==-1000);
    assert(r.apply(s,-1000,-1000,0,false,299,true)==-1000);
    assert(r.apply(s,-1000,-1000,0,false,300,true)==-1000);
    assert(r.apply(s,-1000,-1000,0,false,400,true)==-700);
    assert(r.apply(s,-1000,-1000,0,false,500,true)==-1000);
    assert(r.apply(s,-1000,-1000,0,false,600,true)==-700);
    // Threshold release, forward, unknown mode, CUT and opposite-side mixes reset.
    assert(r.apply(s,-299,-900,0,false,604,true)==-900);
    assert(r.apply(s,-1000,-1000,0,false,608,true)==-1000);
    assert(r.apply(s,1000,1000,0,false,700,true)==1000);
    assert(r.apply(s,-1000,100,0,false,704,true)==100);
    assert(r.apply(bad,-1000,-1000,0,false,708,true)==-1000);
    assert(r.apply(s,-1000,-1000,0,false,712,false)==-1000);
    // Reversed output and shifted neutral never cross neutral or deepen braking.
    for(bool rev:{false,true})for(int neutral:{-100,0,100})for(int magnitude:{100,500,800}){
        r.reset();s.delayMs=0;const int command=neutral+(rev?magnitude:-magnitude);
        assert(r.apply(s,-1000,command,neutral,rev,1000,true)==command);
        const int release=r.apply(s,-1000,command,neutral,rev,1100,true);
        const int baseline=(!rev&&neutral>0)||(rev&&neutral<0)?0:neutral;
        if((command-baseline)*(rev?1:-1)>0){
            assert((release-baseline)*(rev?1:-1)>=0);
            assert((release-baseline)*(rev?1:-1)<(command-baseline)*(rev?1:-1));
        }else assert(release==command);
        assert(release*(rev?1:-1)>=0);
    }
    // Frequency limits, phase and millisecond wrap, with no catch-up bursts.
    for(unsigned hz=2;hz<=10;++hz){s.frequencyHz=hz;r.reset();const uint32_t start=0xffffff80u;
        assert(r.apply(s,-1000,-1000,0,false,start,true)==-1000);
        const uint32_t off=(500+hz-1)/hz;
        assert(r.apply(s,-1000,-1000,0,false,start+off,true)==-700);
        const uint32_t on=(1000+hz-1)/hz;
        assert(r.apply(s,-1000,-1000,0,false,start+on,true)==-1000);
    }
    puts("Brake ABS: mode gate, delay, frequency, threshold, reversed/trimmed output, CUT, checksum and wrap PASS");
}
