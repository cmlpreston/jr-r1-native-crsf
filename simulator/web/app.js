const simulatorBuildId='__R1_BUILD_ID__';

const screen=document.getElementById('screen'), display=document.getElementById('display'), offline=document.getElementById('offline'), statusLine=document.getElementById('status');
const orientation=new URLSearchParams(location.search).get('orientation')==='landscape'?'landscape':'portrait';
let simulator=null, touch=null;
const actions={menu:0,home:1,page:2,quick:4,wake:6,arm:9,cut:9,up:21,down:22,select:23,back:24,minus:25,plus:26};
const kinds={steering:0,throttle:1,link:2,rx:3,tx:4,vehicle:5};
function send(data){
 if(!simulator)return;
 if(data.kind==='touch')simulator._demoTouch(data.x,data.y,Number(data.down));
 else if(data.kind==='contact')simulator._demoContact(Number(data.input),Number(data.positive),Number(data.down));
 else if(data.kind==='action')simulator._demoAction(actions[data.name],data.name==='cut'?0:1);
 else if(data.kind in kinds)simulator._demoInput(kinds[data.kind],Number(data.value));
}
document.getElementById('orientation').value=orientation;
display.classList.toggle('landscape',orientation==='landscape');
document.getElementById('screen-label').textContent='Drive → Layout edits widgets. Hold CUT to request arming; tap to cut.';
async function start(){
 try {
  if(!('WebAssembly' in window))throw new Error('This demo needs a browser that supports WebAssembly.');
  await new Promise((resolve,reject)=>{const script=document.createElement('script');script.src='r1-'+orientation+'.js?v='+simulatorBuildId;script.onload=resolve;script.onerror=()=>reject(new Error('Could not download the simulator. Reload to try again.'));document.head.append(script);});
  simulator=await createR1Simulator({canvas:screen,
   locateFile:path=>path+'?v='+simulatorBuildId,
   storageWarning:()=>{statusLine.textContent='Browser storage is unavailable. Changes will last only for this session.';},
   onAbort:()=>{offline.hidden=false;offline.textContent='The simulator stopped. Reload this page to restart.';}
  });
  offline.hidden=true;
 } catch(e){offline.hidden=false;offline.textContent=e.message;console.error(e);}
}
function xy(e){const rect=screen.getBoundingClientRect(),w=orientation==='portrait'?320:480,h=orientation==='portrait'?480:320;return{x:Math.max(0,Math.min(w-1,Math.floor((e.clientX-rect.left)*w/rect.width))),y:Math.max(0,Math.min(h-1,Math.floor((e.clientY-rect.top)*h/rect.height)))};}
screen.addEventListener('pointerdown',e=>{e.preventDefault();screen.setPointerCapture(e.pointerId);touch=xy(e);send({kind:'touch',...touch,down:true});});
screen.addEventListener('pointermove',e=>{if(touch){touch=xy(e);send({kind:'touch',...touch,down:true});}});
function releaseTouch(e){if(touch){send({kind:'touch',...touch,down:false});touch=null;}if(e&&screen.hasPointerCapture(e.pointerId))screen.releasePointerCapture(e.pointerId);}
screen.addEventListener('pointerup',releaseTouch);screen.addEventListener('pointercancel',releaseTouch);
function axis(name,value){const input=document.getElementById(name);input.value=value;document.getElementById(name+'-value').textContent=value+'%';send({kind:name,value:Math.round(Number(value)*10)});}
for(const name of ['steering','throttle']){const input=document.getElementById(name);input.addEventListener('input',()=>axis(name,input.value));input.addEventListener('change',()=>{if(document.getElementById('spring').checked)axis(name,0);});}
document.getElementById('centre').addEventListener('click',()=>{axis('steering',0);axis('throttle',0);});
document.querySelectorAll('[data-action]').forEach(b=>b.addEventListener('click',()=>send({kind:'action',name:b.dataset.action})));
// Send contact levels; the firmware supplies debounce, repeats and gestures.
const heldContacts=new Map();
const feedback=document.getElementById('contact-feedback');
function contact(button,down){
 send({kind:'contact',input:button.dataset.contact,positive:button.dataset.positive,down});
 button.classList.toggle('held',down);
 feedback.textContent=button.getAttribute('aria-label')+(down?' · held':' · released');
}
function pressContact(button){
 if(heldContacts.has(button)||!simulator)return;
 heldContacts.set(button,{started:performance.now(),timer:null});contact(button,true);
}
function releaseContact(button,immediate=false){
 const held=heldContacts.get(button);if(!held)return;
 clearTimeout(held.timer);
 const finish=()=>{contact(button,false);heldContacts.delete(button);};
 // Brief mouse/accessibility clicks must survive the firmware's 20 ms debounce.
 const remaining=80-(performance.now()-held.started);
 if(immediate||remaining<=0)finish();else held.timer=setTimeout(finish,remaining);
}
function releaseContacts(){for(const button of heldContacts.keys())releaseContact(button,true);}
document.querySelectorAll('[data-contact]').forEach(button=>{
 button.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.preventDefault();button.focus();button.setPointerCapture(e.pointerId);pressContact(button);});
 button.addEventListener('pointerup',()=>releaseContact(button));
 button.addEventListener('pointercancel',()=>releaseContact(button,true));
 button.addEventListener('lostpointercapture',()=>releaseContact(button));
 button.addEventListener('keydown',e=>{if(e.key===' '||e.key==='Enter'){e.preventDefault();if(!e.repeat)pressContact(button);}});
 button.addEventListener('keyup',e=>{if(e.key===' '||e.key==='Enter'){e.preventDefault();releaseContact(button);}});
 button.addEventListener('blur',()=>releaseContact(button,true));
 // Assistive technology may activate a button without pointer/key events.
 button.addEventListener('click',e=>{if(e.detail===0&&!heldContacts.has(button)){pressContact(button);releaseContact(button);}});
});
document.querySelectorAll('[data-switch]').forEach(button=>button.addEventListener('click',()=>{
 if(!simulator)return;
 const on=button.getAttribute('aria-pressed')!=='true';
 button.setAttribute('aria-pressed',String(on));button.textContent=on?'ON':'OFF';
 send({kind:'contact',input:button.dataset.switch,positive:0,down:on});
 feedback.textContent=button.getAttribute('aria-label')+(on?' · ON':' · OFF');
}));
document.getElementById('vehicle').addEventListener('change',e=>send({kind:'vehicle',value:e.target.checked}));
document.getElementById('link').addEventListener('change',e=>send({kind:'link',value:e.target.checked}));
for(const kind of ['rx','tx'])document.getElementById(kind).addEventListener('change',e=>{const v=Number(e.target.value);if(Number.isFinite(v)&&v>=0&&v<=20)send({kind,value:Math.round(v*10)});});
document.getElementById('orientation').addEventListener('change',e=>{releaseTouch();location.href='?orientation='+e.target.value;});
document.getElementById('reset-demo').addEventListener('click',()=>{
 if(!confirm('Reset the demo settings saved in this browser?'))return;
 try{for(const key of Object.keys(localStorage))if(key.startsWith('jr-r1-demo-v1:'))localStorage.removeItem(key);location.reload();}
 catch(e){statusLine.textContent='Browser storage is unavailable.';}
});
const keys={m:'menu',h:'home',p:'page',Escape:'back',ArrowUp:'up',ArrowDown:'down',ArrowLeft:'minus',ArrowRight:'plus',Enter:'select'};
const driveKeys={w:['throttle',100],s:['throttle',-100],a:['steering',-100],d:['steering',100]};
document.addEventListener('keydown',e=>{if(e.target.closest('[data-contact]')||e.target.matches('input,select')||(e.key==='Enter'&&e.target.closest('button'))||e.metaKey||e.ctrlKey||e.altKey)return;const k=e.key.length===1?e.key.toLowerCase():e.key;if(keys[k]){e.preventDefault();if(!e.repeat)send({kind:'action',name:keys[k]});}else if(driveKeys[k]){e.preventDefault();if(!e.repeat)axis(...driveKeys[k]);}});
document.addEventListener('keyup',e=>{const k=e.key.toLowerCase();if(driveKeys[k])axis(driveKeys[k][0],0);});
window.addEventListener('blur',()=>{releaseContacts();releaseTouch();axis('steering',0);axis('throttle',0);});
document.addEventListener('visibilitychange',()=>{if(document.hidden){releaseContacts();releaseTouch();axis('steering',0);axis('throttle',0);}});
start();
