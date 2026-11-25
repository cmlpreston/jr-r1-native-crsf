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
 RemoteScreenLease lease;
 assert(!lease.active(0));
 lease.renew(UINT32_MAX-999);
 assert(lease.active(0)); // millis() wraps during the lease.
 assert(lease.active(RemoteScreenLease::TimeoutMs-1001));
 assert(!lease.active(RemoteScreenLease::TimeoutMs-1000));
 lease.renew(25000);assert(lease.active(25000));
 lease.clear();assert(!lease.active(25000));

 RemoteScreen screen;
 uint16_t data[32],frame[32]={};
 assert(!screen.strip(0,0).count);
 screen.attach(data,4,8);
 assert(!screen.strip(0,0).count);
 screen.capture(0,0,4,8,frame);
 auto strip=screen.strip(0,0);
 assert(strip.first==0&&strip.count==RemoteScreen::MaxRowsPerReply&&strip.next==strip.count);
 strip=screen.strip(0,strip.next);
 assert(strip.first==4&&strip.count==4&&strip.next==0);
 const auto baseline=screen.revision();
 assert(!screen.strip(baseline,0).count);
 screen.capture(0,0,4,8,frame);assert(screen.revision()==baseline);

 uint16_t pixel=99;
 screen.capture(1,0,1,1,&pixel);
 screen.capture(1,7,1,1,&pixel);
 const auto batchRevision=screen.revision();
 strip=screen.strip(baseline,0);
 assert(strip.first==0&&strip.count==1&&strip.next==7&&data[1]==99);
 // A row redrawn behind the cursor stays dirty for the following batch.
 pixel=100;screen.capture(1,0,1,1,&pixel);
 strip=screen.strip(baseline,strip.next);
 assert(strip.first==7&&strip.count==1&&strip.next==0&&data[29]==99);
 strip=screen.strip(batchRevision,0);
 assert(strip.first==0&&strip.count==1&&strip.next==0&&data[1]==100);

 const auto current=screen.revision();
 screen.capture(-1,0,1,1,&pixel);screen.capture(0,8,1,1,&pixel);
 assert(screen.revision()==current);
 assert(!screen.strip(0,-1).count&&!screen.strip(0,8).count);
 screen.attach(nullptr,0,0);assert(!screen.strip(0,0).count);
 puts("Remote screen bounded row strips, sparse updates, re-dirtied rows and viewer lease PASS");
}
