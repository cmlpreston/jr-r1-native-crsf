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
