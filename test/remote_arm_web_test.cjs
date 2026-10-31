const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(__dirname+'/../include/setup_portal_page.h','utf8');
let now=10000,posts=[],nextId=1,failHeartbeat=false;
const el={livestart:{},livestatus:{},livecanvas:{}};
const ctx={$:id=>el[id],Date:{now:()=>now},document:{hidden:false},setTimeout:()=>1,clearTimeout:()=>{},AbortController,URLSearchParams,token:'test',pinLocked:false,webWritable:true,mapBusy:false,setRemoteMode:v=>{ctx.webWritable=v},fetch:async(url,options)=>{const body=Object.fromEntries(options.body);posts.push(body);if(failHeartbeat&&body.action==='6')throw Error('network lost');return{ok:true,text:async()=>body.action==='3'?String(nextId++):'Applied'}}};
vm.createContext(ctx);vm.runInContext(source.slice(source.indexOf('let live=false,'),source.indexOf('\nlcd.onclick=')),ctx);
const set=s=>vm.runInContext(s,ctx),hold=()=>set('webHold');
const settle=async()=>{await Promise.resolve();await Promise.resolve();await Promise.resolve()};
const reset=()=>{ctx.webWritable=true;ctx.pinLocked=false;ctx.document.hidden=false;set('live=true;liveBusy=false;liveEpoch=7;lastLiveAt=Date.now();remoteHoldZone=[0,280,120,40]')};
(async()=>{
 reset();await ctx.startWebHold(30,300);await settle();await ctx.finishWebHold();await settle();
 assert.equal(posts.filter(p=>p.action==='4').length,0);assert.ok(posts.some(p=>p.action==='5'));
 reset();await ctx.startWebHold(30,300);await settle();const h=hold();
 now+=850;await ctx.holdPulse(h);assert.equal(h.ready,true);await ctx.finishWebHold();
 assert.equal(posts.filter(p=>p.action==='4').length,1);assert.equal(posts.at(-1).hold,String(h.id));assert.equal(ctx.webWritable,false);
 reset();await ctx.startWebHold(30,300);await settle();ctx.document.hidden=true;await ctx.holdPulse(hold());await ctx.finishWebHold();assert.equal(posts.filter(p=>p.action==='4').length,1);
 reset();await ctx.startWebHold(30,300);await settle();set('liveGeneration++');await ctx.finishWebHold();assert.equal(posts.filter(p=>p.action==='4').length,1);
 reset();failHeartbeat=true;await ctx.startWebHold(30,300);await settle();await settle();assert.equal(hold(),null);await ctx.finishWebHold();assert.equal(posts.filter(p=>p.action==='4').length,1);
 reset();ctx.pinLocked=true;const before=posts.length;await ctx.startWebHold(30,300);assert.equal(posts.length,before);
 assert.equal(ctx.inHoldZone(20,300),true);assert.equal(ctx.inHoldZone(121,300),false);
 // Drive is also a navigation tab: a short tap navigates but cannot enable.
 reset();failHeartbeat=false;let taps=[];ctx.liveInput=async(...args)=>taps.push(args);
 const gesture={x:30,y:300,started:now,epoch:7,generation:set('liveGeneration'),zone:[0,280,120,40]};
 await ctx.startWebHold(30,300);await settle();now+=100;await ctx.releaseDriveGesture(gesture,30,300);assert.deepEqual(taps,[[0,30,300]]);assert.equal(posts.filter(p=>p.action==='4').length,1);
 taps=[];gesture.started=now;await ctx.startWebHold(30,300);await settle();now+=500;await ctx.releaseDriveGesture(gesture,30,300);assert.equal(taps.length,0);assert.equal(posts.filter(p=>p.action==='4').length,1);
 gesture.started=now;gesture.epoch=6;await ctx.releaseDriveGesture(gesture,30,300);assert.equal(taps.length,0);
 // A body swipe forwards its endpoints, while stale/wake gestures cannot page.
 ctx.lcd=el.livecanvas;Object.assign(ctx.lcd,{width:480,height:320,getBoundingClientRect:()=>({left:0,top:0,width:480,height:320}),setPointerCapture:()=>{}});
 vm.runInContext(source.slice(source.indexOf('lcd.onclick=null;'),source.indexOf('window.addEventListener(\'blur\'')),ctx);
 const pointer=(x,y)=>({button:0,pointerId:1,clientX:x,clientY:y,preventDefault(){}});
 reset();set('remoteWake=false');taps=[];
 ctx.lcd.onpointerdown(pointer(250,150));now+=400;ctx.lcd.onpointermove(pointer(100,154));ctx.lcd.onpointerup(pointer(100,154));
 assert.equal(taps.length,1);assert.deepEqual(taps[0].slice(0,3),[8,250,150]);
 assert.equal(taps[0][3].endX,100);assert.equal(taps[0][3].endY,154);assert.equal(taps[0][3].duration,400);
 taps=[];ctx.lcd.onpointerdown(pointer(250,150));set('liveEpoch++');ctx.lcd.onpointerup(pointer(100,154));assert.equal(taps.length,0);
 reset();set('remoteWake=true');ctx.lcd.onpointerdown(pointer(250,150));ctx.lcd.onpointerup(pointer(100,154));assert.equal(taps.length,0);
 console.log('Remote web hold: short release, ready release, nonce, PIN, visibility, generation and connection-loss cancellation PASS');
})().catch(e=>{console.error(e);process.exitCode=1});
