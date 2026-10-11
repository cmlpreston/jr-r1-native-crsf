const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(__dirname+'/../include/setup_portal_page.h','utf8');
let now=10000,timers=[],posts=[],response,previewResolve;
const elements={livecanvas:{},livestatus:{},livestart:{}};
const ctx={$:id=>elements[id],Date:{now:()=>now},setTimeout:(fn,ms)=>{timers.push({fn,ms});return timers.length},clearTimeout:()=>{},AbortController,URLSearchParams,document:{hidden:false},mapBusy:false,pinLocked:false,webWritable:true,setRemoteMode:()=>{},fetch:()=>response,request:async(url,data)=>{if(url==='/api/access')return {json:async()=>({controlAllowed:true})};posts.push(Object.fromEntries(data));}};
vm.createContext(ctx);
vm.runInContext(source.slice(source.indexOf('let live=false,'),source.indexOf('\nlcd.onclick=')),ctx);
function set(code){vm.runInContext(code,ctx)}
const unchanged={ok:true,status:204,headers:{has:()=>false}};
(async()=>{
 set('live=true;liveEpoch=7;lastLiveAt=Date.now();liveActiveUntil=Date.now()+2000');
 response=Promise.resolve(unchanged);await ctx.pollLive();assert.equal(timers.at(-1).ms,200);
 now+=3000;await ctx.pollLive();assert.equal(timers.at(-1).ms,1000);
 response=new Promise(r=>previewResolve=r);const polling=ctx.pollLive();
 await ctx.liveInput(0,12,34);assert.equal(posts.length,0); // transfer owns connection
 previewResolve(unchanged);await polling;await Promise.resolve();
 assert.equal(posts.length,1);assert.equal(posts[0].epoch,'7');assert.equal(posts[0].x,'12');assert.equal(timers.at(-1).ms,0);
 // Never replay a queued coordinate onto a different page.
 response=new Promise(r=>previewResolve=r);const next=ctx.pollLive();await ctx.liveInput(0,20,30);set('liveEpoch=8');previewResolve(unchanged);await next;
 assert.equal(posts.length,1);
 // A slower remote connection must not be cut off at the old five-second limit.
 response=new Promise(r=>previewResolve=r);const slow=ctx.pollLive();
 assert.equal(timers.at(-1).ms,20000);
 now+=6000;previewResolve(unchanged);await slow;assert.equal(vm.runInContext('live',ctx),true);
 // A tap queued behind a long transfer must expire rather than execute late.
 response=new Promise(r=>previewResolve=r);const delayed=ctx.pollLive();await ctx.liveInput(0,40,50);
 now+=3000;previewResolve(unchanged);await delayed;assert.equal(posts.length,1);
 // Disconnect cancels queued input.
 response=new Promise(r=>previewResolve=r);const last=ctx.pollLive();await ctx.liveInput(0,20,30);ctx.stopLive();previewResolve(unchanged);await last;assert.equal(posts.length,1);
 // Wake-only taps remain available while armed/read-only and PIN locked.
 set('live=true;liveEpoch=9;lastLiveAt=Date.now();remoteWake=true');ctx.webWritable=false;ctx.pinLocked=true;
 await ctx.liveInput(0,20,30);assert.equal(posts.at(-1).action,'7');
 set('remoteWake=false');const count=posts.length;await ctx.liveInput(1,20,30);assert.equal(posts.length,count);
 ctx.pinLocked=false;await ctx.liveInput(0,20,30);assert.equal(posts.at(-2).action,'9');assert.equal(posts.at(-1).action,'0');
 // A failed CUT handshake must never dispatch the requested UI control.
 ctx.webWritable=false;const originalRequest=ctx.request;ctx.request=async(url,data)=>{if(url==='/api/access')throw Error('Connection lost');return originalRequest(url,data)};
 const before=posts.length;await ctx.liveInput(0,50,60);assert.equal(posts.length,before+1);assert.equal(posts.at(-1).action,'9');
 console.log('Live screen: active/idle cadence, queued tap, immediate refresh, epoch/disconnect cancellation, slow transfer and stale tap expiry PASS');
})().catch(e=>{console.error(e);process.exitCode=1});
