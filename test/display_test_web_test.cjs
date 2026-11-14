const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(__dirname+'/../include/setup_portal_page.h','utf8');
const elements={},$=id=>elements[id]??={disabled:false,textContent:''};
let calls=[],fail=false,pendingResolve;
let snapshot={supported:true,mode:'standard',pixels:153600,bytes:307200,pixelCalls:20,
 backendCalls:4800,scratchPixels:0,elapsedUs:40000,maxWriteUs:2200,
 runtime:{ready:true,controlMaximumGapUs:4100,radioMaximumGapUs:4200,
  inputMaximumGapUs:5100,inputHealthy:true,inputAgeMs:2,inputDropped:0,diagnosticDropped:3}};
const ctx={$,URLSearchParams,webWritable:true,pinLocked:false,request:async(path,body)=>{
 assert.equal(path,'/api/display-test');const action=body?Object.fromEntries(body):null;calls.push(action);
 if(pendingResolve!==undefined)await new Promise(resolve=>{pendingResolve=resolve});
 if(fail)throw Error('connection lost');
 if(action?.action==='mode')snapshot={...snapshot,mode:action.mode};
 if(action?.action==='reset')snapshot={...snapshot,pixels:0,bytes:0,pixelCalls:0,backendCalls:0,elapsedUs:0};
 return{json:async()=>snapshot};
}};
vm.createContext(ctx);
vm.runInContext(source.slice(source.indexOf('// Display comparison uses'),source.indexOf('// End display comparison.')),ctx);
(async()=>{
 // An unsolicited write cannot precede a successful capability read.
 await $('displaytrial').onclick();assert.equal(calls.length,0);
 await ctx.loadDisplayTest();assert.deepEqual(calls,[null]);
 assert.match($('displaystatus').textContent,/Standard/);
 assert.match($('displaytotals').textContent,/40\.00 ms/);
 assert.match($('displaytotals').textContent,/20 writes \/ 4,800 API calls/);
 assert.match($('displaygaps').textContent,/physical inputs 5\.10 ms/);
 assert.match($('displaygaps').textContent,/diagnostic events 3/);
 assert.equal($('displaytrial').disabled,false);

 // Read access stays available in LIVE; both visual and direct-event writes stop.
 ctx.webWritable=false;ctx.updateDisplayTestControls();
 assert.equal($('displaytrial').disabled,true);assert.equal($('displayrefresh').disabled,false);
 const initial=calls.length;
 await $('displaytrial').onclick();await $('displayreset').onclick();assert.equal(calls.length,initial);
 await ctx.loadDisplayTest();assert.equal(calls.length,initial+1);assert.equal(calls.at(-1),null);
 ctx.webWritable=true;ctx.pinLocked=true;ctx.updateDisplayTestControls();
 await $('displaystandard').onclick();assert.equal(calls.length,initial+1);
 ctx.pinLocked=false;ctx.updateDisplayTestControls();

 // Mode changes are explicit, and reset addresses only display counters.
 await $('displaytrial').onclick();assert.deepEqual(calls.at(-1),{action:'mode',mode:'trial'});
 assert.match($('displaystatus').textContent,/Trial/);
 await $('displayreset').onclick();assert.deepEqual(calls.at(-1),{action:'reset'});
 assert.match($('displaytotals').textContent,/0 pixels/);assert.match($('displaystatus').textContent,/Trial/);
 await $('displaystandard').onclick();assert.deepEqual(calls.at(-1),{action:'mode',mode:'standard'});

 // One in-flight request, even if a button is clicked repeatedly or refresh fires.
 pendingResolve=null;const changing=$('displaytrial').onclick();await Promise.resolve();
 const during=calls.length;assert.equal($('displaytrial').disabled,true);
 await $('displaytrial').onclick();await ctx.loadDisplayTest();assert.equal(calls.length,during);
 const finish=pendingResolve;pendingResolve=undefined;finish();await changing;
 assert.equal($('displaytrial').disabled,false);

 // Ambiguous failures disable further writes until a fresh successful read.
 fail=true;await $('displaystandard').onclick();
 assert.equal($('displaytrial').disabled,true);assert.match($('displaystatus').textContent,/refresh before another change/);
 const failed=calls.length;await $('displayreset').onclick();assert.equal(calls.length,failed);
 fail=false;await ctx.loadDisplayTest();assert.equal($('displaytrial').disabled,false);

 // Unsupported builds expose a useful read-only result and never submit a change.
 snapshot={...snapshot,supported:false,runtime:{ready:false}};await ctx.loadDisplayTest();
 assert.match($('displaystatus').textContent,/unavailable/);assert.equal($('displaytotals').textContent,'');
 assert.equal($('displaygaps').textContent,'');const unsupported=calls.length;
 await $('displaytrial').onclick();await $('displayreset').onclick();assert.equal(calls.length,unsupported);
 console.log('Display comparison: capability/read-only/PIN guards, explicit mode/reset, one pending request and failure recovery PASS');
})().catch(e=>{console.error(e);process.exitCode=1});
