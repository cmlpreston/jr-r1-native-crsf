'use strict';
// Extra-channel and telemetry design studies. All values are local simulations.
const auxFunctions=['Auxiliary','Lights','Winch','Gear','Diff lock','Fan'];
const auxModes=['Toggle','3-position','Proportional'];
const auxDefaults=()=>Array.from({length:14},(_,i)=>({name:i===0?'Lights':i===1?'Winch':'Auxiliary',source:null,enabled:false,mode:i===1?'Proportional':'Toggle',reverse:false,low:-100,high:100,failsafe:i===0?-100:0}));
let channelBank=0,auxIndex=0,auxTab='control',auxLearning=null;
let auxPositions=Array(14).fill(0);
const auxChannel=()=>model().aux[auxIndex];
function restoreAux(m,old){
  m.aux=auxDefaults();
  m.aux.forEach((a,i)=>{
    const v=old?.aux?.[i];if(!v)return;
    if(auxFunctions.includes(v.name))a.name=v.name;
    if(auxModes.includes(v.mode))a.mode=v.mode;
    if(v.source===null||controlNames[v.source])a.source=v.source;
    a.enabled=v.enabled===true;a.reverse=v.reverse===true;
    for(const k of ['low','high','failsafe'])if(Number.isFinite(v[k]))a[k]=clamp(Math.round(v[k]),-100,100);
    a.low=clamp(a.low,-100,0);a.high=clamp(a.high,0,100);
    a.failsafe=clamp(a.failsafe,a.low,a.high);
  });
}
function auxRaw(i){
  const a=model().aux[i];
  if(!a.enabled||!a.source)return 0;
  return a.mode==='Proportional'?(input[a.source]??0):a.mode==='Toggle'?(auxPositions[i]===100?100:-100):auxPositions[i];
}
function auxOutput(i){
  const a=model().aux[i];
  if(!a.enabled)return 0;
  if(!armed||!link)return a.failsafe;
  const v=auxRaw(i)*(a.reverse?-1:1);
  return v<0?-v/100*a.low:v/100*a.high;
}
function channelLabel(n){return n===0?'Steering':n===1?'Throttle / brake':model().aux[n-2].name;}
function bankFooter(){return `<div class="bank-footer"><button data-bank="-1" aria-label="Previous channels" ${channelBank===0?'disabled':''}>‹</button><span>CH ${channelBank*4+1}–${channelBank*4+4} of 16</span><button data-bank="1" aria-label="Next channels" ${channelBank===3?'disabled':''}>›</button></div>`;}
function channelsView(){
  return head('Channels','<span class="scope">MODEL OUTPUTS</span>','setup')+`<div class="rows channel-list">${Array.from({length:4},(_,i)=>{const n=channelBank*4+i,a=model().aux[n-2];return row(`CH${n+1} · ${channelLabel(n)}`,n<2?'Tune':a.enabled?'On':'Off',`data-channel="${n}"`);}).join('')}</div>`+bankFooter();
}
function auxView(){
  const a=auxChannel();
  return head(`CH${auxIndex+3}`,`<div class="segmented"><button data-aux-tab="control" class="${auxTab==='control'?'selected':''}">Control</button><button data-aux-tab="output" class="${auxTab==='output'?'selected':''}">Output</button></div>`,'channels')+`<div class="rows aux-rows">${auxTab==='control'?
    row('Function',a.name,'data-aux-change="name"')+row('Assigned control',controlNames[a.source]||'Unassigned','data-aux-learn="true"')+row('Behaviour',a.mode,'data-aux-change="mode"')+row('Channel enabled',a.enabled?'On':'Off','data-aux-change="enabled"'):
    row('Low endpoint',a.low+'%','data-aux-edit="low"')+row('High endpoint',a.high+'%','data-aux-edit="high"')+row('Reverse',a.reverse?'On':'Off','data-aux-change="reverse"')+row('Failsafe',a.failsafe+'%','data-aux-edit="failsafe"')
    }</div><div class="aux-summary">${a.name} · <span id="aux-summary-value">${signed(auxOutput(auxIndex))}%</span> output · ${armed?'ARMED':'CUT'}</div>`;
}
function auxSpec(key){
  const a=auxChannel();
  return {label:{low:'Low endpoint',high:'High endpoint',failsafe:'Failsafe'}[key],unit:'%',min:key==='high'?0:key==='failsafe'?a.low:-100,max:key==='low'?0:key==='failsafe'?a.high:100,initial:key==='low'?-100:key==='high'?100:clamp(0,a.low,a.high),step:1,coarse:10};
}
function editAux(key){armed=false;editor={key,auxId:auxIndex,draft:auxChannel()[key],returnPage:'aux'};page='editor';render();}
function auxPreview(){return `<div class="aux-preview"><div class="micro-label">OUTPUT RANGE</div><div class="monitor-bar"><i id="aux-draft-marker" style="left:${50+editor.draft/2}%"></i></div><div class="graph-ends"><span>−100</span><span>0</span><span>+100</span></div><p>${editor.key==='failsafe'?'This position is used on link loss or CUT in this trial.':'Endpoints limit this channel. Direction is set separately with Reverse.'}</p><div class="micro-label">CH${auxIndex+3} · ${auxChannel().name.toUpperCase()}</div></div>`;}
function buttonConflicts(control,exceptAux=-1){
  if(['steering','throttle','auxDial'].includes(control))return [];
  return [...Object.entries(bindings()).filter(([k,v])=>k!=='steeringSource'&&v===control).map(([k])=>functionNames[k]),...model().aux.flatMap((a,i)=>i!==exceptAux&&a.source===control?[`CH${i+3} ${a.name}`]:[])];
}
function clearButtonAssignments(control,exceptAux=-1){
  if(['steering','throttle','auxDial'].includes(control))return;
  for(const k of Object.keys(bindings()))if(k!=='steeringSource'&&bindings()[k]===control)bindings()[k]=null;
  model().aux.forEach((a,i)=>{if(i!==exceptAux&&a.source===control){a.source=null;a.enabled=false;auxPositions[i]=0;}});
}
function auxLearnView(){
  const detected=auxLearning?.detected,conflicts=detected?buttonConflicts(detected,auxIndex):[];
  return head(`CH${auxIndex+3} · Assign`,'<span class="scope">CONTROL</span>')+`<div class="learn-body"><div class="micro-label">${detected?'CONTROL DETECTED':'WAITING FOR INPUT'}</div><h3>${detected?controlNames[detected]:'Move a dial or<br>press a button'}</h3><p>${detected?(conflicts.length?'Replaces: '+conflicts.join(', ')+'.':'Assigning enables this channel.'): 'Use the AUX dial or handset buttons beside the display.'}</p></div><div class="learn-actions"><button data-go="aux">Cancel</button><button class="primary" data-aux-assign="true" ${detected?'':'disabled'}>Assign</button></div>`;
}
function learnAuxControl(control){if(!auxLearning)return false;auxLearning.detected=control;render();return true;}
function triggerAux(control){
  const i=model().aux.findIndex(a=>a.enabled&&a.source===control&&a.mode!=='Proportional');
  if(i<0)return false;
  const a=model().aux[i];auxPositions[i]=a.mode==='Toggle'?(auxPositions[i]===100?-100:100):auxPositions[i]===100?-100:auxPositions[i]+100;
  refreshReadings();toast(`CH${i+3} ${a.name} ${signed(auxPositions[i])}%`,armed&&link?'Output follows this control':'Requested position · output at failsafe');return true;
}
function dataHead(){return head('Data',`<div class="segmented">${[['telemetry','Sensors'],['gps','GPS'],['monitor','Channels']].map(([p,label])=>`<button data-go="${p}" class="${page===p?'selected':''}">${label}</button>`).join('')}</div>`);}
function allMonitorView(){
  return dataHead()+`<div class="all-channels">${Array.from({length:4},(_,i)=>{const n=channelBank*4+i;return `<div class="channel-meter"><div class="monitor-label"><span>CH${n+1} ${channelLabel(n)}</span><b id="channel-out-${n}">0%</b></div><div class="monitor-bar"><i id="channel-dot-${n}"></i></div><div class="monitor-foot"><span id="channel-in-${n}">Input 0%</span><span>${n<2?'Output':model().aux[n-2].enabled?'Output':'Disabled'}</span></div></div>`;}).join('')}</div>`+bankFooter();
}
function refreshExtras(){
  for(let n=channelBank*4;n<channelBank*4+4;n++){
    if(!$('channel-out-'+n))continue;
    const r=n===0?'steering':'throttle';
    const v=n<2?(armed&&link?motion[r]*(r==='throttle'&&motion[r]>0&&model().mode==='Walk'?.5:1):0):auxOutput(n-2);
    $('channel-out-'+n).textContent=signed(v)+'%';$('channel-dot-'+n).style.left=(50+v/2)+'%';
    $('channel-in-'+n).textContent='Input '+signed(n<2?rawFor(r):auxRaw(n-2))+'%';
  }
  if($('aux-summary-value'))$('aux-summary-value').textContent=signed(auxOutput(auxIndex))+'%';
  if($('aux-draft-marker'))$('aux-draft-marker').style.left=(50+editor.draft/2)+'%';
}

// Fictional track in local metres. No real location or online map service used.
let demoStream='live',gpsFix=true,demoTime=0,lastSample=performance.now(),mapZoom=1,showTrail=true;
let gpsHeading=180,gpsPoint={x:350,y:130},gpsTrail=[],sample={speed:40,temp:82,rpm:18200,rx:6.6,lq:100,sats:12};
let sensorSelected='speed';
const sensorInfo={speed:['Speed','km/h',0,100],temp:['Motor temp','°C',0,140],rpm:['Motor RPM','rpm',0,32000],rx:['Receiver','V',4,8],lq:['Link quality','%',0,100],sats:['Satellites','',0,20]};
const sensorHistory=Object.fromEntries(Object.keys(sensorInfo).map(k=>[k,[]]));
function freshTelemetry(){return demoStream==='live'&&link;}
function sensorAvailable(k){return demoStream!=='missing'&&link&&(!['speed','sats'].includes(k)||gpsFix);}
function sensorText(k){return sensorAvailable(k)?k==='rx'?sample[k].toFixed(1):Math.round(sample[k]).toLocaleString('en-AU'):'—';}
function streamLabel(){return !link?'LINK LOST':demoStream==='missing'?'NO DATA':demoStream==='paused'?`STALE · ${Math.floor((performance.now()-lastSample)/1000)}s`:'DEMO · LIVE';}
function telemetryView(){
  return dataHead()+`<div class="sensor-grid">${Object.entries(sensorInfo).map(([k,[label,unit]])=>`<button class="sensor-tile" data-sensor="${k}"><span>${label}</span><strong id="sensor-${k}">${sensorText(k)}</strong><small>${unit}</small></button>`).join('')}</div><div class="data-status"><span id="data-stream">${streamLabel()}</span><span>Simulated sensors</span></div>`;
}
function sensorView(){
  const [label,unit]=sensorInfo[sensorSelected];
  return head(label,'<span class="scope">DEMO SENSOR</span>','telemetry')+`<div class="sensor-detail"><div class="sensor-reading"><strong id="sensor-detail-value">${sensorText(sensorSelected)}</strong><span>${unit}</span></div><svg class="sensor-chart" viewBox="0 0 400 90" preserveAspectRatio="none" role="img" aria-label="Recent sensor readings"><path class="grid" d="M0 30H400M0 60H400"/><path id="sensor-history"/></svg><div class="sensor-range"><span id="sensor-min">MIN —</span><span>Last 30 samples</span><span id="sensor-max">MAX —</span></div></div><div class="data-status"><span id="data-stream">${streamLabel()}</span><span>Simulated sensors</span></div>`;
}
function gpsView(){
  return dataHead()+`<div class="gps-body"><div class="gps-strip"><b id="gps-fix">3D FIX · 12 SAT</b><span id="gps-speed">40 km/h</span></div><div class="map-panel"><svg id="track-map" viewBox="0 0 440 260" role="img" aria-label="Fictional test-track map with a simulated vehicle trail"><rect class="map-ground" width="440" height="260"/><path class="map-grid" d="M0 65H440M0 130H440M0 195H440M110 0V260M220 0V260M330 0V260"/><path class="map-road-edge" d="M350 130a130 75 0 1 1-260 0a130 75 0 1 1 260 0"/><path class="map-road" d="M350 130a130 75 0 1 1-260 0a130 75 0 1 1 260 0"/><path class="map-pit" d="M105 172L160 168L290 168L328 176"/><rect class="map-building" x="175" y="175" width="65" height="17"/><text class="map-label" x="179" y="165">PITS</text><text class="map-label" x="166" y="113">R1 TEST TRACK</text><path id="gps-trail"/><g transform="translate(350 130)"><circle class="home-dot" r="6"/><text class="map-label" x="12" y="5">H</text></g><g id="gps-marker"><circle class="vehicle-halo" r="12"/><path class="vehicle-arrow" d="M0-9L7 8L0 4L-7 8Z"/></g></svg><span class="map-north">↑ N</span><span class="map-scale" id="map-scale">50 m</span><span class="map-demo">FICTIONAL MAP</span></div><div class="gps-readings"><span id="gps-distance">Home 0 m</span><span id="gps-state">DEMO · LIVE</span></div><div class="map-actions"><button data-map="out" aria-label="Zoom out">−</button><button data-map="in" aria-label="Zoom in">+</button><button data-map="fit">Fit</button><button data-map="trail">Trail ${showTrail?'on':'off'}</button></div></div>`;
}
function updateData(){
  if($('drive-rx'))$('drive-rx').innerHTML=(freshTelemetry()?sensorText('rx'):'—')+'<span>V</span>';
  for(const k of Object.keys(sensorInfo))if($('sensor-'+k)){
    $('sensor-'+k).textContent=sensorText(k);$('sensor-'+k).parentElement.classList.toggle('stale',!freshTelemetry()||!sensorAvailable(k));
  }
  if($('data-stream'))$('data-stream').textContent=streamLabel();
  if($('sensor-detail-value')){
    const k=sensorSelected,h=sensorHistory[k],info=sensorInfo[k],range=info[3]-info[2];
    $('sensor-detail-value').textContent=sensorText(k);
    $('sensor-history').setAttribute('d',h.map((v,i)=>`${i?'L':'M'}${i*400/29} ${88-clamp((v-info[2])/range,0,1)*84}`).join(' '));
    $('sensor-min').textContent='MIN '+(h.length?Math.min(...h).toFixed(k==='rx'?1:0):'—');
    $('sensor-max').textContent='MAX '+(h.length?Math.max(...h).toFixed(k==='rx'?1:0):'—');
  }
  if($('track-map')){
    const valid=freshTelemetry()&&gpsFix;
    $('gps-fix').textContent=valid?`3D FIX · ${sample.sats} SAT`:gpsFix?'STALE POSITION':'NO GPS FIX';
    $('gps-speed').textContent=sensorText('speed')+' km/h';
    $('gps-state').textContent=streamLabel();
    $('gps-distance').textContent=(valid?'Home ':'Last fix · ')+Math.round(Math.hypot(gpsPoint.x-350,gpsPoint.y-130)*.5)+' m';
    const w=440/mapZoom,h=260/mapZoom;
    $('track-map').setAttribute('viewBox',mapZoom===1?'0 0 440 260':`${gpsPoint.x-w/2} ${gpsPoint.y-h/2} ${w} ${h}`);
    $('gps-marker').setAttribute('transform',`translate(${gpsPoint.x} ${gpsPoint.y}) rotate(${gpsHeading})`);
    $('gps-marker').classList.toggle('no-fix',!valid);
    $('gps-trail').setAttribute('d',showTrail?gpsTrail.map((v,i)=>`${i?'L':'M'}${v.x} ${v.y}`).join(' '):'');
    // Scale in map units: 100 units = 50 metres. SVG preserves aspect ratio.
    const map=$('track-map');const pxPerUnit=Math.min(map.clientWidth/w,map.clientHeight/h);
    const scaleMetres=mapZoom>=2?20:50;
    $('map-scale').textContent=scaleMetres+' m';$('map-scale').style.width=(scaleMetres*2*pxPerUnit)+'px';
  }
}
function tickTelemetry(){
  if(freshTelemetry()){
    demoTime+=.13;lastSample=performance.now();
    sample.speed=Math.hypot(130*Math.sin(demoTime),75*Math.cos(demoTime))*.26*.5*3.6;
    sample.temp=82+9*Math.sin(demoTime*.35);sample.rpm=8000+sample.speed*250;sample.rx=6.6-.1*Math.sin(demoTime*.4);sample.sats=12;
    if(gpsFix){gpsHeading=Math.atan2(-130*Math.sin(demoTime),-75*Math.cos(demoTime))*180/Math.PI;gpsPoint={x:220+130*Math.cos(demoTime),y:130+75*Math.sin(demoTime)};gpsTrail.push({...gpsPoint});if(gpsTrail.length>70)gpsTrail.shift();}
    for(const k of Object.keys(sensorInfo))if(sensorAvailable(k)){sensorHistory[k].push(sample[k]);if(sensorHistory[k].length>30)sensorHistory[k].shift();}
  }
  updateData();
}

const buggyImage="data:image/png;base64,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";
function modelIcon(index){return index===0?`<img class="model-icon" src="${buggyImage}" alt="Hyper 7 buggy">`:'<svg class="model-icon crawler-icon" viewBox="0 0 82 58" role="img" aria-label="Trail crawler"><path d="M13 35L17 19H48L62 31H73V45H10V35Z"/><path class="window" d="M23 24H44L53 32H21Z"/><circle cx="24" cy="44" r="10"/><circle cx="62" cy="44" r="10"/></svg>';}
function resetExtras(){channelBank=0;auxPositions.fill(0);gpsHeading=180;for(const values of Object.values(sensorHistory))values.length=0;gpsTrail=[];demoTime=0;gpsPoint={x:350,y:130};}
document.getElementById('lcd').addEventListener('click',event=>{
  const b=event.target.closest('button');if(!b)return;
  if(b.dataset.bank){channelBank=clamp(channelBank+Number(b.dataset.bank),0,3);render();}
  else if(b.dataset.channel!==undefined){const n=Number(b.dataset.channel);if(n<2){role=n===0?'steering':'throttle';go('tune');}else{auxIndex=n-2;auxTab='control';go('aux');}}
  else if(b.dataset.auxTab){auxTab=b.dataset.auxTab;render();}
  else if(b.dataset.auxEdit)editAux(b.dataset.auxEdit);
  else if(b.dataset.auxLearn){armed=false;auxLearning={detected:null};page='auxlearn';render();}
  else if(b.dataset.auxAssign&&auxLearning?.detected){
    const source=auxLearning.detected,a=auxChannel();clearButtonAssignments(source,auxIndex);
    a.source=source;a.enabled=true;a.mode=['steering','throttle','auxDial'].includes(source)?'Proportional':a.mode==='Proportional'?'Toggle':a.mode;
    auxPositions[auxIndex]=0;save();go('aux');toast(`CH${auxIndex+3} assigned`,controlNames[source]);
  }else if(b.dataset.auxChange){
    const a=auxChannel(),k=b.dataset.auxChange;
    if(k==='name')a.name=auxFunctions[(auxFunctions.indexOf(a.name)+1)%auxFunctions.length];
    else if(k==='mode'){
      const analog=['steering','throttle','auxDial'].includes(a.source);
      if(analog){toast('Proportional control','Assign a button to use switched positions.');return;}
      if(a.source)a.mode=a.mode==='Toggle'?'3-position':'Toggle';else a.mode=auxModes[(auxModes.indexOf(a.mode)+1)%3];
      auxPositions[auxIndex]=0;
    }else if(k==='enabled'&&!a.source){toast('Assign a control first');return;}else a[k]=!a[k];
    save();render();
  }else if(b.dataset.sensor){sensorSelected=b.dataset.sensor;go('sensor');}
  else if(b.dataset.map){const k=b.dataset.map;if(k==='in')mapZoom=Math.min(3,mapZoom+.5);if(k==='out')mapZoom=Math.max(1,mapZoom-.5);if(k==='fit')mapZoom=1;if(k==='trail'){showTrail=!showTrail;b.textContent='Trail '+(showTrail?'on':'off');}updateData();}
});
