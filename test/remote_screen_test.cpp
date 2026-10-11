#include "remote_screen.h"
#include "browser_session.h"
#include "wifi_retry.h"
#include "network_hostname.h"
#include "remote_control_policy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 assert(browserSessionMatches("R1Session=abc","abc"));
 assert(browserSessionMatches("other=x; R1Session=abc; another=y","abc"));
 assert(browserSessionMatches(" R1Session=abc\t ","abc"));
 for(const char *bad:{"", "R1Session=", "R1Session=abcd", "R1Session=ab", "OtherR1Session=abc", "x=R1Session=abc", "R1Session=old"})
  assert(!browserSessionMatches(bad,"abc"));
 assert(!browserSessionMatches("R1Session=abc",""));
 assert(!browserSessionMatches(nullptr,"abc"));

 char hostname[33];
 assert(normalizeHostname("R1-EVO",hostname)&&strcmp(hostname,"r1-evo")==0);
 assert(normalizeHostname("a",hostname));
 assert(normalizeHostname("12345678901234567890123456789012",hostname));
 for(const char *bad:{"", "-r1", "r1-", "r1.local", "r1 evo", "r1_evo", "123456789012345678901234567890123", "évo"})assert(!normalizeHostname(bad,hostname));
 assert(!normalizeHostname(nullptr,hostname));

 WifiRetry retry;assert(retry.ready(0));
 assert(retry.failed(100)==2000);assert(!retry.ready(2099));assert(retry.ready(2100));
 assert(retry.failed(3000)==5000);assert(retry.failed(9000)==10000);
 assert(retry.failed(20000)==20000);assert(retry.failed(50000)==30000);
 assert(retry.failed(90000)==30000);retry.reset();assert(retry.ready(90000));
 assert(retry.failed(UINT32_MAX-999)==2000);assert(!retry.ready(999));assert(retry.ready(1000));

 for(int i=0;i<=int(OutputSafetyStatus::ChannelDisabled);++i){
  auto status=static_cast<OutputSafetyStatus>(i);
  assert(!remoteControlAllowed(true,status));
  assert(!wifiConnectionAllowed(true,status));
  assert(wifiConnectionAllowed(false,status)==(status!=OutputSafetyStatus::Armed));
  assert(remoteControlAllowed(false,status)==(status==OutputSafetyStatus::Cut||status==OutputSafetyStatus::Calibration));
 }
 RemoteScreen s;int y=0,n=0;uint16_t data[12],row[4]={1,2,3,4};
 assert(!s.region(0,y,n));s.attach(data,4,3);assert(!s.region(0,y,n));
 s.capture(0,1,4,1,row);assert(s.region(0,y,n)&&y==0&&n==3);
 auto version=s.revision();assert(!s.region(version,y,n));
 s.capture(0,1,4,1,row);assert(s.revision()==version);
 row[1]=99;s.capture(0,1,4,1,row);assert(s.region(version,y,n)&&y==1&&n==1&&data[5]==99);
 auto next=s.revision();s.capture(-1,0,4,1,row);assert(s.revision()==next);
 s.capture(0,3,4,1,row);assert(s.revision()==next);
 assert(s.region(next+10,y,n)&&y==0&&n==3);
 s.attach(nullptr,0,0);assert(!s.region(0,y,n));
 puts("Remote screen dirty rows, bounds, resync and release PASS");
}
