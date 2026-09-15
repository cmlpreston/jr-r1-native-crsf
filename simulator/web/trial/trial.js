'use strict';
// A standalone interaction prototype. It neither imports nor writes firmware state.
const $ = id => document.getElementById(id);
const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
const storageKey = 'jr-r1-interface-trial-v2';
const parameters = {
  rate: {label:'Rate',unit:'%',min:0,max:100,initial:100,step:1,coarse:10},
  expo: {label:'Expo',unit:'%',min:-100,max:100,initial:0,step:1,coarse:10},
  travel: {label:'Travel',unit:'%',min:0,max:120,initial:100,step:1,coarse:10},
  trim: {label:'Trim',unit:'%',min:-30,max:30,initial:0,step:1,coarse:5},
  speed: {label:'Transition time',unit:'ms',min:0,max:500,initial:0,step:10,coarse:50}
};
const controlNames = {'grip-a-plus':'Grip A +','grip-a-minus':'Grip A −','grip-b-plus':'Grip B +','grip-b-minus':'Grip B −','trim-plus':'Steer trim +','trim-minus':'Steer trim −',timer:'Timer button','mode-switch':'Drive mode switch',steering:'Steering wheel',throttle:'Throttle trigger',auxDial:'AUX dial'};
const functionNames = {steeringSource:'Steering input',rateUp:'Steering rate +',rateDown:'Steering rate −',timer:'Run timer',mode:'Drive mode'};
const modes = ['Normal','Walk','Race'];
const roleName = role => ({steering:'Steering',throttle:'Throttle',brake:'Brake'})[role];
const parameterKeys = r => r==='brake' ? ['rate','expo','speed'] : Object.keys(parameters);
const signed = value => (value > 0 ? '+' : '') + Math.round(value);
const defaultBindings={steeringSource:'steering',rateUp:'grip-a-plus',rateDown:'grip-a-minus',timer:'timer',mode:'mode-switch'};
const defaults = () => ({theme:'day',orientation:'landscape',modelId:0,
  models:[
    {name:'Hyper 7',description:'1:8 electric buggy',mode:'Normal',steering:{rate:95,expo:20,travel:100,trim:0,speed:0},throttle:{rate:100,expo:10,travel:100,trim:0,speed:0},rf:{power:100,rate:250,telemetry:'1:32'}},
    {name:'Trail crawler',description:'1:10 electric crawler',mode:'Walk',steering:{rate:100,expo:10,travel:100,trim:0,speed:0},throttle:{rate:70,expo:35,travel:100,trim:0,speed:150},rf:{power:100,rate:250,telemetry:'1:32'}}
  ].map(m=>({...m,brake:{rate:100,expo:0,travel:100,trim:0,speed:0},aux:auxDefaults(),bindings:{...defaultBindings}}))});
function loadPreferences(){
  const clean=defaults();
  try{
    const saved=JSON.parse(localStorage.getItem(storageKey));
    if(!saved)return clean;
    if(['day','night'].includes(saved.theme))clean.theme=saved.theme;
    if(['landscape','portrait'].includes(saved.orientation))clean.orientation=saved.orientation;
    if([0,1].includes(saved.modelId))clean.modelId=saved.modelId;
    clean.models.forEach((model,index)=>{
      const old=saved.models?.[index];restoreAux(model,old);if(!old)return;
      for(const role of ['steering','throttle','brake'])for(const key of parameterKeys(role)){
        const param=parameters[key];
        const value=old[role]?.[key];if(Number.isFinite(value))model[role][key]=clamp(Math.round(value),param.min,param.max);
      }
      if(modes.includes(old.mode))model.mode=old.mode;
      if([25,100,250].includes(old.rf?.power))model.rf.power=old.rf.power;
      if([100,250,500].includes(old.rf?.rate))model.rf.rate=old.rf.rate;
      if(['1:32','1:64','Off'].includes(old.rf?.telemetry))model.rf.telemetry=old.rf.telemetry;
    });
    clean.models.forEach((model,index)=>{for(const key of Object.keys(defaultBindings)){
      const value=saved.models?.[index]?.bindings?.[key];
      if(key==='steeringSource'&&['steering','throttle'].includes(value))model.bindings[key]=value;
      else if(key!=='steeringSource'&&(value===null || (controlNames[value]&&!['steering','throttle'].includes(value))))model.bindings[key]=value;
    }});
  }catch(e){/* Defaults are usable if storage is blocked or the trial data is invalid. */}
  return clean;
}
let prefs=loadPreferences();
let page='drive',role='steering',setupScope='model',editor=null,learning=null,focusIndex=0;
let armed=false,link=true,toastTimeout=null,timerRunning=false,timerStart=0,timerElapsed=0;
const input={steering:0,throttle:0,auxDial:0};
const motion={steering:0,throttle:0};
let lastMotionAt=performance.now();
const model=()=>prefs.models[prefs.modelId];
const bindings=()=>model().bindings;
function save(){try{localStorage.setItem(storageKey,JSON.stringify(prefs));}catch(e){toast('Settings are session-only','Browser storage is unavailable.');}}
function toast(message,detail=''){
  const box=$('toast');box.replaceChildren(document.createTextNode(message));
  if(detail){const small=document.createElement('small');small.textContent=detail;box.append(small);}
  box.hidden=false;clearTimeout(toastTimeout);toastTimeout=setTimeout(()=>{box.hidden=true;},2600);
}
function status(){
  $('active-model').innerHTML=modelIcon(prefs.modelId)+`<span>${model().name.toUpperCase()}</span>`;
  $('lq').textContent=link?'100 LQ':'NO LINK';
  $('link-status').parentElement.classList.toggle('no-link',!link);
  $('output-state').textContent=!link?'CUT':armed?'ARMED':'CUT';
  $('output-state').className=!link?'lost':armed?'armed':'cut-state';
}
function head(title,extra='',back=''){
  return `<div class="page-head"><div class="heading-group">${back?`<button class="back" data-go="${back}" aria-label="Back">‹</button>`:''}<h2>${title}</h2></div>${extra}</div>`;
}
function row(label,value,attributes,small=''){
  return `<button class="setting-row" ${attributes}><span>${label}${small?`<small>${small}</small>`:''}</span><b>${value}</b><span class="chevron">›</span></button>`;
}
function graph(){
  const brake=(editor?.role||role)==='brake';
  const ends=brake?['0','BRAKE TRAVEL','100']:['−100','NEUTRAL','+100'];
  return `<div class="graph-panel"><div class="micro-label">RESPONSE PREVIEW</div><svg class="response-graph" viewBox="0 0 180 140" preserveAspectRatio="none" role="img" aria-label="Response curve with a live control position"><path class="grid" d="M45 0V140M135 0V140M0 35H180M0 105H180"/><path class="axis" d="${brake?'M6 0V132H180':'M90 0V140M0 70H180'}"/><path class="reference" d="M6 132L174 8"/><path class="curve" id="curve"/><circle class="marker" id="marker" r="4.5" cx="90" cy="70"/></svg><div class="graph-ends">${ends.map(t=>`<span>${t}</span>`).join('')}</div><div class="graph-caption"><span id="graph-in">IN 0%</span><span id="graph-out">OUT 0%</span></div></div>`;
}
function trimControl(r){
  const value=model()[r].trim;
  return `<button class="trim-control" data-edit="trim" data-role="${r}" aria-label="${roleName(r)} trim ${signed(value)} percent"><span class="trim-label"><span>${roleName(r)} trim</span><b>${signed(value)}<small>%</small> <span class="chevron">›</span></b></span><span class="trim-track" aria-hidden="true"><i style="left:${50+value/30*46}%"></i></span></button>`;
}
function driveView(){
  return head('Drive',`<button class="mode-choice" data-go="modes">${model().mode.toUpperCase()} ›</button>`)+
    `<div class="drive-body"><div class="run-timer"><div class="micro-label">RUN TIMER</div><div class="clock" id="clock">00:00</div><button data-action="timer" id="timer-toggle">${timerRunning?'Stop':'Start'}</button></div><div class="volts"><div>TX <strong>9.9<span>V</span></strong></div><div>RX <strong id="drive-rx">${link?'6.6':'—'}<span>V</span></strong></div></div><div class="quick-values">${['steering','brake'].map(r=>`<button class="quick-row" data-edit="rate" data-role="${r}"><span>${roleName(r)} rate</span><strong>${model()[r].rate}<small>%</small></strong><span class="chevron">›</span></button>`).join('')}</div><div class="drive-trims">${trimControl('steering')}${trimControl('throttle')}</div></div>`;
}
function tuneView(){
  return head('Tune',`<div class="segmented">${['steering','throttle','brake'].map(r=>`<button data-role-select="${r}" class="${r===role?'selected':''}">${roleName(r)}</button>`).join('')}</div>`)+
    `<div class="tune-body"><div class="parameter-list">${parameterKeys(role).map(key=>{const p=parameters[key];return `<button class="param-row" data-edit="${key}" data-role="${role}"><span>${key==='speed'?'Speed':p.label}</span><span class="value">${model()[role][key]}${p.unit}</span><span class="chevron">›</span></button>`;}).join('')}${role==='brake'?'<p class="settings-footnote">Brake-side response. Forward throttle is unchanged.</p>':''}</div>${graph()}</div>`;
}
function editorView(){
  const p=editorSpec();
  return head(`${editor.auxId!==undefined?'CH'+(editor.auxId+3):roleName(editor.role)} · ${editor.key==='speed'?'Speed':p.label}`,'<span class="scope">PREVIEW</span>')+
    `<div class="editor-body"><div class="editor-main"><div class="micro-label">${p.min} TO ${p.max} ${p.unit.toUpperCase()}</div><div class="editor-value"><span id="draft-value">${editor.draft}</span><small>${p.unit}</small></div><div class="edit-steps">${[-p.coarse,-p.step,p.step,p.coarse].map(step=>`<button data-step="${step}" aria-label="${step<0?'Decrease':'Increase'} by ${Math.abs(step)}">${step>0?'+':'−'}${Math.abs(step)}</button>`).join('')}</div><div class="editor-note">Changes take effect when you Apply.</div></div>${editor.auxId!==undefined?auxPreview():graph()}</div><div class="editor-actions"><button data-action="reset-value">Reset</button><button data-action="cancel-edit">Cancel</button><button class="primary" data-action="apply-edit">Apply</button></div>`;
}
function setupView(){
  return head('Setup',`<div class="segmented"><button data-scope="model" class="${setupScope==='model'?'selected':''}">Model</button><button data-scope="handset" class="${setupScope==='handset'?'selected':''}">Handset</button></div>`)+`<div class="rows">${setupScope==='model'?
    row('Channels','CH1–16','data-go="channels"')+row('Control map','', 'data-go="mapping"')+row('RF module','ELRS','data-go="link"')+row('Drive mode',model().mode,'data-go="modes"'):
    row('Display palette',prefs.theme==='day'?'Day':'Night','data-action="palette"')+row('Screen layout',prefs.orientation==='landscape'?'Landscape':'Portrait','data-action="orientation"')+row('Reset trial settings','','data-go="reset"')}
    </div>`;
}
function mappingView(){
  return head('Control map',`<span class="scope">${model().name}</span>`,'setup')+`<div class="rows mapping-rows">${Object.entries(functionNames).map(([key,label])=>row(label,controlNames[bindings()[key]]||'Unassigned',`data-learn="${key}"`)).join('')}</div>`;
}
function learnView(){
  const analog=learning.key==='steeringSource';
  const conflicts=learning.detected&&!analog?buttonConflicts(learning.detected):[];
  return head(functionNames[learning.key],'<span class="scope">ASSIGN CONTROL</span>')+
    `<div class="learn-body"><div class="micro-label">${learning.detected?'CONTROL DETECTED':'WAITING FOR INPUT'}</div><h3>${learning.detected?`<span class="control-detected">${controlNames[learning.detected]}</span>`:analog?'Move the wheel<br>or trigger':'Press a handset<br>button'}</h3><p>${learning.detected?(conflicts.length?`Replaces: ${conflicts.join(', ')}.`:'Confirm to use this control.'):(analog?'Use either input slider beside the display.':'Use a grip, trim, timer or mode button beside the display.')}</p></div><div class="learn-actions"><button data-action="cancel-learn">Cancel</button><button class="primary" data-action="assign" ${learning.detected?'':'disabled'}>Assign</button></div>`;
}
function linkView(){
  const rf=model().rf;
  return head('RF module','<span class="scope">RANGER NANO</span>','setup')+`<div class="rows">${row('Packet rate',rf.rate+' Hz','data-rf="rate"')}${row('RF power',rf.power+' mW','data-rf="power"')}${row('Telemetry ratio',rf.telemetry,'data-rf="telemetry"')}<p class="settings-footnote">${link?'Connected · LQ 100% · −48 dBm':'No radio link'}<br>Configuration replies are simulated in this trial.</p></div>`;
}
function safetyView(){
  const state=!link?'Radio link lost':armed?'Output armed':'Output is cut';
  const reason=!link?'Outputs use their failsafe positions. Restore the link, then re-arm.':armed?'Enabled channels follow the simulated handset controls.':'Use the ARM button beside the display to try the output state. The trigger must be centred.';
  return head('Output status','','drive')+`<div class="safety-body"><h3 class="safety-state">${state}</h3><p>${reason}</p><p>Opening Tune or Setup returns this demo to CUT.</p></div>`;
}
function modelsView(){
  return head('Model profiles','','setup')+`<div class="rows">${prefs.models.map((m,i)=>`<button class="setting-row model-row ${prefs.modelId===i?'active':''}" data-model="${i}">${modelIcon(i)}<span>${m.name}<small>${m.description}</small></span><b>${prefs.modelId===i?'Active':'Select'}</b></button>`).join('')}<p class="settings-footnote">Each model keeps its own tuning and RF settings.</p></div>`;
}
function modesView(){
  const descriptions=['Full configured response','Throttle limited to 50%','Full configured response'];
  return head('Drive mode',`<span class="scope">${model().name}</span>`,'drive')+`<div class="rows">${modes.map((m,i)=>`<button class="setting-row mode-row ${model().mode===m?'active':''}" data-mode="${m}"><span>${m}<small>${descriptions[i]}</small></span><b>${model().mode===m?'Active':'Select'}</b></button>`).join('')}</div>`;
}
function render(){
  const views={drive:driveView,monitor:allMonitorView,telemetry:telemetryView,gps:gpsView,sensor:sensorView,channels:channelsView,aux:auxView,auxlearn:auxLearnView,tune:tuneView,editor:editorView,setup:setupView,mapping:mappingView,learn:learnView,link:linkView,safety:safetyView,models:modelsView,modes:modesView,
    reset:()=>head('Reset this trial','','setup')+'<div class="safety-body"><h3 class="safety-state">Start again?</h3><p>This clears tuning and control assignments for this design trial in your browser.</p></div><div class="learn-actions"><button data-go="setup">Cancel</button><button class="primary" data-action="reset-all">Reset trial</button></div>'};
  $('view').innerHTML=views[page]();
  $('radio-nav').hidden=['editor','learn','auxlearn','reset'].includes(page);
  const group=['monitor','telemetry','gps','sensor'].includes(page)?'telemetry':page==='drive'||page==='safety'||page==='modes'?'drive':page==='tune'||(page==='editor'&&editor?.auxId===undefined)?'tune':'setup';
  $('radio-nav').querySelectorAll('button').forEach(b=>{if(b.dataset.go===group)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current');});
  focusIndex=0;status();refreshReadings();updateData();
}
function go(next){
  if(!['drive','monitor','telemetry','gps','sensor','safety'].includes(next))armed=false;
  if(next!=='auxlearn')auxLearning=null;
  editor=null;learning=null;page=next;render();
}
function edit(key,editRole){
  armed=false;role=editRole;editor={key,role:editRole,draft:model()[editRole][key],returnPage:page};page='editor';render();
}
function editorSpec(){return editor.auxId!==undefined?auxSpec(editor.key):parameters[editor.key];}
function setDraft(value){const p=editorSpec();editor.draft=clamp(value,p.min,p.max);$('draft-value').textContent=editor.draft;refreshReadings();}
function endEdit(apply){
  const old=editor,p=editorSpec();if(apply){
    if(old.auxId!==undefined){const a=model().aux[old.auxId];a[old.key]=old.draft;a.failsafe=clamp(a.failsafe,a.low,a.high);}
    else model()[old.role][old.key]=old.draft;save();}
  page=old.returnPage;editor=null;render();
  if(apply)toast(`${old.auxId!==undefined?'CH'+(old.auxId+3):roleName(old.role)} ${p.label.toLowerCase()} ${old.draft}${p.unit}`,'Applied to '+model().name);
}
function action(name){
  if(name==='timer'){if(timerRunning){timerElapsed+=performance.now()-timerStart;timerRunning=false;}else{timerStart=performance.now();timerRunning=true;}refreshReadings();const button=$('timer-toggle');if(button)button.textContent=timerRunning?'Stop':'Start';}
  else if(name==='reset-value')setDraft(editorSpec().initial);
  else if(name==='cancel-edit')endEdit(false);
  else if(name==='apply-edit')endEdit(true);
  else if(name==='cancel-learn')go('mapping');
  else if(name==='assign'&&learning?.detected){
    const {key,detected}=learning;
    if(key!=='steeringSource')clearButtonAssignments(detected);
    if(key!=='steeringSource')for(const existing of Object.keys(bindings()))if(existing!==key&&bindings()[existing]===detected)bindings()[existing]=null;
    bindings()[key]=detected;save();go('mapping');toast(`${functionNames[key]} assigned`,controlNames[detected]);
  }else if(name==='palette'){prefs.theme=prefs.theme==='day'?'night':'day';appearance();render();}
  else if(name==='orientation'){prefs.orientation=prefs.orientation==='landscape'?'portrait':'landscape';appearance();render();}
  else if(name==='reset-all'){try{localStorage.removeItem(storageKey);}catch(e){}prefs=defaults();resetExtras();armed=false;timerRunning=false;timerElapsed=0;appearance();go('drive');}
}
function learnControl(control){
  if(!learning)return false;
  if(control==='auxDial')return true;
  const isAnalog=['steering','throttle'].includes(control);
  if((learning.key==='steeringSource')!==isAnalog)return true;
  if(learning.detected!==control){learning.detected=control;render();}
  return true;
}
function physical(control){
  if(learnAuxControl(control)||learnControl(control)||triggerAux(control))return;
  const fn=Object.keys(bindings()).find(key=>key!=='steeringSource'&&bindings()[key]===control);
  if(fn==='timer'){action('timer');toast(timerRunning?'Run timer started':'Run timer stopped');}
  else if(fn==='mode'){model().mode=modes[(modes.indexOf(model().mode)+1)%modes.length];save();if(page!=='editor')render();toast('Drive mode '+model().mode);}
  else if(fn==='rateUp'||fn==='rateDown')adjustSetting('rate',fn==='rateUp'?1:-1);
  else if(control==='trim-plus'||control==='trim-minus')adjustSetting('trim',control==='trim-plus'?1:-1);
  else toast(controlNames[control],'No function assigned. Use Setup → Control map.');
}
function adjustSetting(key,amount){
  const p=parameters[key];model().steering[key]=clamp(model().steering[key]+amount,p.min,p.max);save();
  if(page!=='editor')render();else refreshReadings();
  toast(`Steering ${key==='trim'?'trim':'rate'} ${key==='trim'?signed(model().steering[key]):model().steering[key]}${p.unit}`);
}
function setAxis(name,value){
  input[name]=clamp(Number(value),-100,100);$(name).value=input[name];$(name+'-value').textContent=signed(input[name])+'%';
  if(Math.abs(input[name])>=10&&!learnAuxControl(name))learnControl(name);
  refreshReadings();
}
function valuesFor(r){
  const values={...model()[r]};if(editor?.role===r)values[editor.key]=editor.draft;return values;
}
// Brake has its own curve and rate. It only affects negative trigger travel.
function curveValue(x,r){
  if(r==='throttle'&&x<0)return clamp(-curveValue(-x,'brake')+valuesFor('throttle').trim,-100,100);
  const p=valuesFor(r),v=x/100,e=p.expo/100;
  const shaped=e>=0?(1-e)*v+e*v*v*v:(1+e)*v-e*Math.sign(v)*(1-Math.pow(1-Math.abs(v),3));
  return clamp(shaped*p.rate*p.travel/100+p.trim,-100,100);
}
function rawFor(r){return r==='brake'?Math.max(0,-input.throttle):input[r==='steering'?bindings().steeringSource:'throttle'];}
function refreshReadings(){
  refreshExtras();
  const r=editor?.role||role,raw=rawFor(r),desired=curveValue(raw,r),p=valuesFor(r);
  const curve=$('curve');
  if(curve){
    const brake=r==='brake',start=brake?0:-100;
    const mapX=x=>brake?6+x*1.68:90+x*.84,mapY=y=>brake?132-y*1.24:70-y*.62;
    let path='';for(let x=start;x<=100;x+=2){path+=(x===start?'M':'L')+mapX(x).toFixed(1)+' '+mapY(curveValue(x,r)).toFixed(1)+' ';}
    const actual=brake?Math.max(0,-motion.throttle):motion[r];
    const preview=(r==='throttle'&&raw<0?valuesFor('brake').speed:p.speed)?actual:desired;
    curve.setAttribute('d',path);$('marker').setAttribute('cx',mapX(raw));$('marker').setAttribute('cy',mapY(preview));
    $('graph-in').textContent='IN '+signed(raw)+'%';$('graph-out').textContent='OUT '+signed(preview)+'%';
  }
  if($('clock')){
    const seconds=Math.floor((timerElapsed+(timerRunning?performance.now()-timerStart:0))/1000);
    $('clock').textContent=String(Math.floor(seconds/60)).padStart(2,'0')+':'+String(seconds%60).padStart(2,'0');
  }
  for(const channel of ['steering','throttle']){
    if(!$('monitor-'+channel))continue;
    const output=armed&&link?motion[channel]*(channel==='throttle'&&motion[channel]>0&&model().mode==='Walk'?.5:1):0;
    $('monitor-'+channel).style.left=(50+output*.5)+'%';$('monitor-'+channel+'-value').textContent=signed(output)+'%';$('raw-'+channel).textContent=signed(rawFor(channel))+'%';
  }
  if($('monitor-status')){
    $('monitor-status').textContent=!link?'Link lost · output held at neutral':armed?'Output follows controls · ARMED':'Output held at neutral · CUT';
    $('monitor-status').parentElement.dataset.state=!link?'lost':armed?'armed':'cut';
  }
}
function animate(now){
  const elapsed=Math.min(100,now-lastMotionAt);lastMotionAt=now;
  for(const r of ['steering','throttle']){
    const raw=rawFor(r),target=curveValue(raw,r),speed=valuesFor(r==='throttle'&&raw<0?'brake':r).speed;
    motion[r]=speed?motion[r]+clamp(target-motion[r],-elapsed*100/speed,elapsed*100/speed):target;
  }
  if(document.visibilityState!=='hidden')refreshReadings();requestAnimationFrame(animate);
}
function appearance(){
  $('lcd').dataset.theme=prefs.theme;$('lcd').dataset.orientation=prefs.orientation;
  $('palette').value=prefs.theme;$('orientation').value=prefs.orientation;save();fit();
}
function fit(){
  const width=prefs.orientation==='portrait'?320:480,height=prefs.orientation==='portrait'?480:320;
  const available=document.querySelector('.screen-column').clientWidth;
  const border=window.matchMedia('(max-width:690px)').matches?20:28;
  const maximum=$('native').checked?width+border:prefs.orientation==='portrait'?450:740;
  $('device').style.width=Math.min(available,maximum)+'px';
  const scale=$('screen-slot').clientWidth/width;
  $('lcd').style.transform=`scale(${scale})`;$('screen-slot').style.height=(height*scale+2)+'px';
  $('size-label').textContent=`${width} × ${height} · ${scale.toFixed(2)}× preview`;
}
function back(){if(auxLearning){go('aux');return;}if(['aux','channels','sensor'].includes(page)){go(page==='aux'?'channels':page==='channels'?'setup':'telemetry');return;}if(editor)endEdit(false);else if(learning)go('mapping');else go(['mapping','link','models','reset'].includes(page)?'setup':'drive');}
function keypad(key){
  if(key==='mode')go('setup');else if(key==='page')go(['drive','tune','telemetry','setup'][(['drive','tune','telemetry','setup'].indexOf(page)+1)%4]);
  else if(key==='home')go('drive');else if(key==='back')back();
  else if((key==='plus'||key==='minus')&&editor)setDraft(editor.draft+(key==='plus'?1:-1)*editorSpec().step);
  else{
    const choices=[...$('view').querySelectorAll('button:not(:disabled)'),...$('radio-nav').querySelectorAll('button')].filter(b=>b.getClientRects().length);
    if(!choices.length)return;
    if(key==='down'||key==='up'){focusIndex=(focusIndex+(key==='down'?1:-1)+choices.length)%choices.length;choices[focusIndex].focus({preventScroll:true});}
    else if(key==='select')choices[focusIndex]?.click();
    else if(key==='plus'||key==='minus'){
      const button=choices[focusIndex];if(button?.dataset.edit){edit(button.dataset.edit,button.dataset.role);setDraft(editor.draft+(key==='plus'?1:-1)*editorSpec().step);}
    }
  }
}
$('lcd').addEventListener('click',event=>{
  const b=event.target.closest('button');if(!b)return;
  if(b.dataset.go)go(b.dataset.go);
  else if(b.dataset.edit)edit(b.dataset.edit,b.dataset.role);
  else if(b.dataset.step)setDraft(editor.draft+Number(b.dataset.step));
  else if(b.dataset.action)action(b.dataset.action);
  else if(b.dataset.roleSelect){role=b.dataset.roleSelect;render();}
  else if(b.dataset.scope){setupScope=b.dataset.scope;render();}
  else if(b.dataset.learn){learning={key:b.dataset.learn,detected:null};page='learn';render();}
  else if(b.dataset.model){prefs.modelId=Number(b.dataset.model);resetExtras();timerRunning=false;timerElapsed=0;save();go('drive');toast(model().name+' selected','Output is CUT');}
  else if(b.dataset.mode){model().mode=b.dataset.mode;save();go('drive');toast('Drive mode '+model().mode);}
  else if(b.dataset.rf){const options={power:[25,100,250],rate:[100,250,500],telemetry:['1:32','1:64','Off']},key=b.dataset.rf;model().rf[key]=options[key][(options[key].indexOf(model().rf[key])+1)%3];save();render();toast('RF setting applied','Simulated module readback');}
});
document.querySelectorAll('[data-physical]').forEach(b=>b.addEventListener('click',()=>physical(b.dataset.physical)));
document.querySelectorAll('[data-key]').forEach(b=>b.addEventListener('click',()=>keypad(b.dataset.key)));
for(const axis of ['steering','throttle','auxDial']){
  $(axis).addEventListener('input',event=>setAxis(axis,event.target.value));
  $(axis).addEventListener('change',()=>{if(axis!=='auxDial'&&$('spring').checked)setAxis(axis,0);});
}
$('arm').addEventListener('click',()=>{
  if(!link){toast('Cannot arm','Restore the simulated radio link.');return;}
  if(!['drive','monitor','telemetry','gps','sensor'].includes(page)){toast('Return to Drive or Monitor to arm');return;}
  if(Math.abs(input.throttle)>3){toast('Centre the throttle to arm');return;}
  armed=true;status();refreshReadings();toast('Output armed');
});
$('cut').addEventListener('click',()=>{armed=false;status();if(page==='safety')render();refreshReadings();toast('Output cut');});
$('link').addEventListener('change',event=>{link=event.target.checked;armed=false;status();if(['drive','monitor','telemetry','gps','sensor','link','safety'].includes(page))render();toast(link?'Radio link restored':'Radio link lost',link?'Output remains CUT until you re-arm.':'Outputs set to their failsafe positions.');});
$('palette').addEventListener('change',event=>{prefs.theme=event.target.value;appearance();});
$('orientation').addEventListener('change',event=>{prefs.orientation=event.target.value;appearance();});
$('native').addEventListener('change',fit);new ResizeObserver(fit).observe(document.querySelector('.screen-column'));
const navKeys={m:'mode',p:'page',h:'home',Escape:'back',ArrowUp:'up',ArrowDown:'down',ArrowLeft:'minus',ArrowRight:'plus',Enter:'select'};
const driveKeys={a:['steering',-100],d:['steering',100],w:['throttle',100],s:['throttle',-100]};
document.addEventListener('keydown',event=>{
  if(event.target.matches('input,select')||event.metaKey||event.ctrlKey||event.altKey)return;
  const key=event.key.length===1?event.key.toLowerCase():event.key;
  if(key==='Enter'&&event.target.closest('button')&&$('lcd').contains(event.target))return;
  if(navKeys[key]){event.preventDefault();if(!event.repeat||['plus','minus'].includes(navKeys[key]))keypad(navKeys[key]);}
  else if(driveKeys[key]){event.preventDefault();if(!event.repeat)setAxis(...driveKeys[key]);}
});
document.addEventListener('keyup',event=>{const key=event.key.toLowerCase();if(driveKeys[key])setAxis(driveKeys[key][0],0);});
function releaseInputs(){setAxis('steering',0);setAxis('throttle',0);}
window.addEventListener('blur',releaseInputs);document.addEventListener('visibilitychange',()=>{if(document.hidden)releaseInputs();});
$('demo-stream').addEventListener('change',e=>{demoStream=e.target.value;updateData();});
$('gps-fix-toggle').addEventListener('change',e=>{gpsFix=e.target.checked;updateData();});
appearance();render();requestAnimationFrame(animate);setInterval(tickTelemetry,500);
