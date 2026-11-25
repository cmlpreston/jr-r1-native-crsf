const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(__dirname+'/../include/setup_portal_page.h','utf8');
let now=10000,timers=[],posts=[],urls=[],paint=[],response,previewResolve;
const renderer={createImageData:(w,h)=>({width:w,height:h,data:new Uint8ClampedArray(w*h*4)}),putImageData:(image,x,y)=>paint.push({image,x,y})};
const elements={livecanvas:{style:{},getContext:()=>renderer},livestatus:{},livestart:{}};
const ctx={$:id=>elements[id],Date:{now:()=>now},setTimeout:(fn,ms)=>{timers.push({fn,ms});return timers.length},clearTimeout:()=>{},AbortController,URLSearchParams,document:{hidden:false},mapBusy:false,pinLocked:false,webWritable:true,setRemoteMode:()=>{},fetch:url=>{urls.push(url);return response},request:async(url,data)=>{if(url==='/api/access')return {json:async()=>({controlAllowed:true})};posts.push(Object.fromEntries(data));}};
vm.createContext(ctx);
vm.runInContext(source.slice(source.indexOf('let live=false,'),source.indexOf('\nlcd.onclick=')),ctx);
function set(code){vm.runInContext(code,ctx)}
const unchanged={ok:true,status:204,headers:{has:()=>false}};
function strip(batch,epoch,y,rows,next){
 const bytes=new ArrayBuffer(22+4*rows*2),v=new DataView(bytes);
 v.setUint32(0,0x52315352);v.setUint32(4,batch,true);v.setUint32(8,epoch,true);
 [4,8,y,rows,next].forEach((n,i)=>v.setUint16(12+i*2,n,true));
 for(let i=22;i<bytes.byteLength;i+=2)v.setUint16(i,0xffff,true);
 return {...unchanged,status:200,arrayBuffer:async()=>bytes};
}
(async()=>{
 set('live=true;liveReady=true;liveEpoch=7;lastLiveAt=Date.now();liveActiveUntil=Date.now()+2000');
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
 set('live=true;liveReady=true;liveEpoch=9;lastLiveAt=Date.now();remoteWake=true');ctx.webWritable=false;ctx.pinLocked=true;
 await ctx.liveInput(0,20,30);assert.equal(posts.at(-1).action,'7');
 set('remoteWake=false');const count=posts.length;await ctx.liveInput(1,20,30);assert.equal(posts.length,count);
 ctx.pinLocked=false;await ctx.liveInput(0,20,30);assert.equal(posts.at(-2).action,'9');assert.equal(posts.at(-1).action,'0');
 // A failed CUT handshake must never dispatch the requested UI control.
 ctx.webWritable=false;const originalRequest=ctx.request;ctx.request=async(url,data)=>{if(url==='/api/access')throw Error('Connection lost');return originalRequest(url,data)};
 const before=posts.length;await ctx.liveInput(0,50,60);assert.equal(posts.length,before+1);assert.equal(posts.at(-1).action,'9');
 // A new page is assembled offscreen before pointer/keypad input can resume.
 ctx.webWritable=true;ctx.request=originalRequest;
 const requestCount=posts.length;
 response=Promise.resolve(strip(21,10,0,4,4));await ctx.pollLive();
 assert.equal(timers.at(-1).ms,0);assert.equal(paint.length,0);
 assert.equal(vm.runInContext('liveRevision',ctx),0);
 await ctx.liveInput(0,1,1);assert.equal(posts.length,requestCount);
 // Nonblocking write aborted mid-response: retain the exact batch cursor.
 response=Promise.resolve({...unchanged,status:200,arrayBuffer:async()=>{throw new TypeError('truncated')}});
 await ctx.pollLive();const failed=urls.at(-1);
 assert.match(failed,/since=0&epoch=10&scan=21&row=4$/);assert.equal(timers.at(-1).ms,100);
 response=Promise.resolve(strip(21,10,4,4,0));await ctx.pollLive();
 assert.equal(urls.at(-1),failed);assert.equal(paint.length,1);
 assert.equal(paint[0].image.height,8);assert(paint[0].image.data.every(x=>x===255));
 assert.equal(vm.runInContext('liveRevision',ctx),21);
 assert.equal(vm.runInContext('liveReady',ctx),true);
 // A newer page supersedes even a partially received delta batch.
 response=Promise.resolve(strip(22,10,0,1,7));await ctx.pollLive();
 assert.equal(vm.runInContext('liveRevision',ctx),21);
 response=Promise.resolve(strip(23,11,0,4,4));await ctx.pollLive();
 assert.equal(vm.runInContext('liveReady',ctx),false);
 response=Promise.resolve(strip(23,11,4,4,0));await ctx.pollLive();
 assert.equal(vm.runInContext('liveRevision',ctx),23);
 assert.equal(vm.runInContext('liveEpoch',ctx),11);
 assert.equal(paint.length,3);
 console.log('Live screen: cadence, queued tap and staleness, safe epoch staging, strip retry and completed revision PASS');
})().catch(e=>{console.error(e);process.exitCode=1});
