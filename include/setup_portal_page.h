#pragma once
// All UI code is local. Optional map browsing uses OSM, Photon and Overpass.
static const char setupPortalPage[] PROGMEM = R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>R-1 EVO setup</title><style>
[hidden]{display:none!important}:root{color-scheme:light;font:16px system-ui;color:#1b3037;background:#e6ebeb}*{box-sizing:border-box}body{margin:0}header{background:#192d36;color:#fff;padding:20px 5vw;display:flex;justify-content:space-between;align-items:center}.jr{color:#ef3039;font-style:italic;font-weight:900}main{max-width:900px;margin:auto;padding:24px}section{background:#f7f9f8;border:1px solid #bdc9cd;padding:22px;margin-bottom:20px}h1{font-size:22px;margin:0}h2{font-size:20px;margin:0 0 16px}p{line-height:1.5}label{display:block;margin:12px 0 4px}input,select,button{font:inherit;padding:11px;border:1px solid #9caeb5;border-radius:3px;max-width:100%}input,select{width:100%;background:white}button{background:#00717c;color:white;cursor:pointer;margin:12px 8px 0 0}button:disabled{opacity:.5}.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 20px}.cut{color:#ffd078}small{color:#50636a}#notice{position:sticky;top:0;z-index:10;padding:12px;background:#d6eeee;min-height:42px}pre{white-space:pre-wrap;font-size:12px}canvas{width:100%;background:#e9e5dc;border:1px solid #bdc9cd}footer{padding:20px;text-align:center;color:#50636a}@media(max-width:500px){main{padding:12px}section{padding:16px}.grid{gap:8px}}
.control-layout{display:grid;grid-template-columns:1.1fr 1fr;gap:24px}.handset{background:#1b3037;color:#fff;padding:16px;border-radius:16px}.handset h3{font-size:13px;text-transform:uppercase;color:#b8ccce}.control-pair,.keypad{display:grid;grid-template-columns:1fr 1fr;gap:8px}.keypad{grid-template-columns:repeat(3,1fr)}#keycontrols [data-control="11"]{grid-area:1/1}#keycontrols [data-control="12"]{grid-area:1/2}#keycontrols [data-control="13"]{grid-area:1/3}#keycontrols [data-control="15"]{grid-area:2/1}#keycontrols [data-control="14"]{grid-area:2/3}#keycontrols [data-control="16"]{grid-area:3/1}#keycontrols [data-control="17"]{grid-area:3/2}#keycontrols [data-control="18"]{grid-area:3/3}.handset button{margin:0;background:#304950;text-align:left;padding:12px 8px;min-height:64px;border-color:#658089;font-size:12px}.handset button small{display:block;color:#bed0d2;font-size:11px;margin-top:5px}.handset button[aria-pressed=true]{background:#00717c;outline:2px solid #74d9dc}.searchrow{display:flex;gap:8px}.searchrow button{margin:0}#places button{display:block;text-align:left}#slippy{height:360px;position:relative;overflow:hidden;background:#ccd8da;touch-action:none;cursor:grab}#tiles img{position:absolute;width:256px;height:256px;max-width:none}#mapframe{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);border:3px solid #00717c;box-shadow:0 0 0 120px #192d3625;pointer-events:none}#mapframe span{background:#00717c;color:white;font-size:11px;padding:4px}.maptoolbar{display:flex;gap:8px;align-items:center}.maptoolbar button{min-width:44px}#mapprogress{font-size:14px}summary{cursor:pointer;padding:12px 0}@media(max-width:650px){.control-layout{grid-template-columns:1fr}.searchrow{flex-wrap:wrap}#slippy{height:300px}}
</style><header><h1><span class="jr">JR</span> R-1 EVO</h1><span class="cut"><span id="webmode">Checking transmitter state…</span></span></header><div id="notice" role="status">Connecting to handset…</div><main><section><h2>Live handset screen</h2><p>View the actual LCD at any time. Interacting with the mirrored UI enters CUT before applying the touch. This stops live outputs or cancels Link wait. Simply viewing the screen does not change output state.</p><button id="livestart">Connect live screen</button><button id="livehome">Browse handset menus</button><button id="livesetup">Wi-Fi setup screen</button><p id="livestatus" role="status">Not connected</p><canvas id="livecanvas" width="320" height="480" hidden aria-label="Live handset touchscreen" style="display:block;max-width:100%;width:320px;height:auto;touch-action:none;image-rendering:pixelated"></canvas><small>Tap controls as on the LCD. Close returns to the dashboard without disconnecting. Network/password changes require the physical LCD. Hold Drive until “Release to enable” appears, then release. Configuration becomes read-only when enabling is requested.</small></section>
<section><h2>Current model</h2><small id="connection"></small><form id="model"><label>Model name (12 characters)</label><input name="name" maxlength="12" required><button>Save name</button></form><form id="appearance"><label for="modelimage">Model image</label><select id="modelimage" name="image"><option value="0">Hyper 7</option><option value="1">Buggy</option><option value="2">Truggy</option><option value="3">Short-course truck</option><option value="4">Monster truck</option><option value="5">Crawler</option><option value="6">Touring car</option><option value="7">Formula / F1</option><option value="8">None</option></select><label><input id="originalcolour" type="checkbox" style="width:auto" checked> Original primary colour</label><label for="bodycolour">Primary paint</label><input id="bodycolour" type="color" value="#e53935" style="height:48px"><label><input id="originalaccent" type="checkbox" style="width:auto" checked> Original accent colour</label><label for="accentcolour">Accent paint</label><input id="accentcolour" type="color" value="#ffffff" style="height:48px"><label><input id="originalbase" type="checkbox" style="width:auto" checked> Original base colour</label><label for="basecolour">Base paint</label><input id="basecolour" type="color" value="#303030" style="height:48px"><button id="resetpaint" type="button">Original colours</button><p>Primary, accent and base paint include painted wings. The surrounding image stays transparent; tyres and mechanical parts retain their colours. View the result on the live LCD after saving.</p><button>Save appearance</button></form></section>
<section id="controlsection"><h2>Handset controls</h2><p>Select a control to assign its channel or function. Assignments belong to the current model.</p><div class="control-layout"><div class="handset"><h3>Wheel &amp; trigger</h3><div id="primarycontrols" class="control-pair"></div><h3>Trims &amp; switches</h3><div id="othercontrols" class="control-pair"></div><h3>R-1 keypad</h3><div id="keycontrols" class="keypad"></div></div><form id="binding"><h3 id="controlname">Select a control</h3><p id="controlhint"></p><label for="target">Assignment</label><select id="target"></select><label for="selection">Channel / function</label><select id="selection"></select><div id="behaviourfield"><label for="behaviour">Behaviour</label><select id="behaviour"></select></div><div id="contextfield"><label for="context">Active on</label><select id="context"><option value="1">Always</option><option value="2">Dashboard</option><option value="4">Menus</option><option value="8">CUT only</option><option value="16">Armed only</option></select></div><div id="bindingvalues"><h4>Control range (not servo endpoints)</h4><div class="grid"><label>Minimum / switch low (%)<input id="bindingminimum" type="number" step="0.1" required></label><label>Maximum / switch high (%)<input id="bindingmaximum" type="number" step="0.1" required></label><label>Step for stepped controls (%)<input id="bindingstep" type="number" step="0.1" required></label><label>Starting value (%)<input id="bindinginitial" type="number" step="0.1" required></label><label>Input fault value (%)<input id="bindingfailsafe" type="number" step="0.1" required></label></div><label><input id="bindinginverted" type="checkbox" style="width:auto"> Reverse control</label><p><small>Start and fault values must fit the range. Channel trims retain their saved value. This fault value does not configure the receiver’s RF-loss failsafe.</small></p></div><label><input id="bindingenabled" type="checkbox" style="width:auto"> Assignment enabled</label><button id="savebinding">Save assignment</button><p><small>Keypad bindings override their usual action in the selected context. Disabled assignments fall back to the normal handset action. Primary inputs disabled here output neutral.</small></p></form></div></section>

<section><h2>Channel parameters</h2><label>Channel</label><select id="channel"></select><form id="settings"><div class="grid" id="fields"></div><label><input name="reversed" type="checkbox" style="width:auto"> Reverse channel</label><button>Save channel</button></form><small>Negative and positive sides are independent. Trim values use ‰ (10‰ = 1%). Changes use the handset’s existing model storage.</small><p><button id="backup">Download parameter snapshot</button></p><h3>Mixing</h3><p>Choose the existing channel input and up to two ordered contributions. Channel sources include their rate/expo and mixes, before servo endpoints, reversal and sub-trim. Configured inputs use steering/throttle rate and expo; raw controls bypass those settings. Add combines; Replace discards earlier contributions. For CH3 = 60% CH1 + 40% CH2, disable the existing input and add those two sources. Save applies to this model only.</p><form id="mixsettings"><div id="mixfields"></div><div id="mixpreview"><h4>Bench preview</h4><p>Test values at each source contribution input, after source rate/expo. No test value is transmitted. Rates are shown settled; ramp timing is not simulated. Replace discards the preceding total.</p><div id="mixtestinputs" class="grid"></div><canvas id="mixcurve" width="600" height="280" aria-label="Mix input versus final channel output"></canvas><small>Horizontal: selected input %. Vertical: final channel output %. Dot: current test position. Other inputs stay fixed.</small><div id="mixbars"></div><p id="mixpreviewstatus"></p></div><button id="resetmixing" type="button">Reset mixing</button><button id="savemixing" disabled>Save mixing and rates</button><p id="mixstatus"></p></form></section>
<section><h2>Offline driving map</h2><p>Search for a driving location, then drag and zoom the map. Choose Street or Satellite. The outlined area becomes an offline image for the handset. Your browser needs internet access for search and map downloads.</p>
<small id="wifitiming"></small><br><small id="mapstatus"></small><div><button id="prepare">Prepare map storage</button><button id="download">Download handset map</button></div><form id="placesearch"><label for="address">Address or place</label><div class="searchrow"><input id="address" placeholder="Track, park, suburb or street" required maxlength="160"><button id="searchbutton">Search</button></div></form><div id="places" aria-live="polite"></div>
<label for="mapstyle">Map style</label><select id="mapstyle"><option value="street">Street · OpenStreetMap</option><option value="satellite">Satellite · Esri World Imagery</option></select><p><small id="mapstylehint">Street maps include roads, buildings, parks and labels.</small></p><button id="openmap">Open online map</button><div id="mapbrowser" hidden><div id="slippy" tabindex="0" role="application" aria-label="Driving map. Drag to pan, use arrow keys or zoom buttons."><div id="tiles"></div><div id="mapframe"><span>DOWNLOAD AREA</span></div></div><div class="maptoolbar"><button id="zoomout" aria-label="Zoom out">−</button><button id="zoomin" aria-label="Zoom in">+</button><small id="bounds"></small></div><small><span id="mapcredits">© OpenStreetMap contributors</span> · Search by Photon</small><p><button id="fetcharea">Download selected area</button><span id="mapprogress" role="status"></span></p></div>
<details><summary>Import a map file instead</summary><p>Use GeoJSON lines or polygon outlines, or an exported R-1 map package.</p>
<label>Map / track file</label><input type="file" id="mapfile" accept=".r1map,.json,.geojson,application/json,application/octet-stream"></details><div class="grid"><div><label>Name</label><input id="mapname" maxlength="40"></div><div><label>Attribution / source</label><input id="attribution" maxlength="80" placeholder="e.g. © OpenStreetMap contributors" required></div></div><p><canvas id="preview" width="600" height="300"></canvas></p><small id="mapdetails">North is up. Full-colour image: up to 512 × 512 pixels.</small><div><button id="savemap" disabled>Save map to handset</button><button id="exportmap" disabled>Export map package</button></div><p id="savestatus" role="status" aria-live="polite"></p></section>
<p>Home Wi-Fi provides automatic web access. Armed operation is read-only. The setup hotspot can be enabled manually from the LCD.</p></main><footer><span class="jr">JR</span> R-1 · LOCAL SETUP</footer>
<section id="memorysection"><h2>Memory diagnostics</h2><p>Runtime heap pools, not the build's static RAM figure. DMA memory overlaps internal RAM. LVGL's pool is already reserved in static RAM.</p><button id="memoryrefresh" type="button">Refresh memory</button><label><input id="memoryauto" type="checkbox" style="width:auto"> Refresh every 5 seconds</label><p id="memorystatus" role="status">Not sampled yet.</p><div style="overflow-x:auto"><table id="memorytable" style="width:100%;text-align:left;border-spacing:12px"><thead><tr><th>Heap pool</th><th>Free now</th><th>Lowest free</th><th>Largest block</th><th>Heap total</th></tr></thead><tbody></tbody></table></div><p id="memorylvgl"></p><p id="memorystacks"></p><small>Lowest heap and stack margins are since boot; LVGL peak is the largest sampled usage. Stack values are minimum unused bytes, not current usage. Heap minimums across multiple regions need not occur at the same instant. Snapshots are sampled every 2 seconds; short-lived largest-block changes may be missed. Try navigating pages, opening the live LCD view and uploading a map, then refresh. Serial command: MEMORY.</small></section><section><h2>Handset</h2><h3>PIN</h3><form id="pinform"><label>Current PIN<input name="current" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" required></label><label>New PIN<input name="next" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" required></label><label>Confirm new PIN<input name="confirm" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" required></label><button>Change PIN</button></form><p>Leaving Walk mode requires the PIN again. Forgotten PIN: connect USB, open the serial monitor and send <code>PINRESET 1234</code> while outputs are CUT. This sets PIN 1234 without erasing models, Wi-Fi or Walk mode.</p><form id="hostnameform"><label for="hostname">Local hostname</label><input id="hostname" name="hostname" maxlength="32" pattern="[A-Za-z0-9]([A-Za-z0-9-]{0,30}[A-Za-z0-9])?" required aria-describedby="hostnamehelp"><small id="hostnamehelp">Letters, digits and hyphens; omit .local. Restart to apply.</small><p id="hostnameaddress"></p><button>Save hostname</button></form><p>Restart without erasing saved models, maps or Wi-Fi. Outputs must be CUT.</p><button id="restarthandset">Restart handset</button></section><script>
let state,token='',map,webWritable=false,pinLocked=false,mapBusy=false;const $=id=>document.getElementById(id);const notify=t=>$('notice').textContent=t;
function setRemoteMode(allowed){webWritable=allowed;$('webmode').textContent=allowed?(pinLocked?'CUT · CONFIGURATION AVAILABLE · HANDSET PIN-LOCKED':'CUT · REMOTE CONTROL AVAILABLE'):'OUTPUTS ENABLED OR PENDING · REMOTE TOUCH ENTERS CUT';for(const id of ['model','appearance','settings','mixsettings','binding','hostnameform','pinform']){const f=$(id);if(!f)continue;let box=f.querySelector('fieldset');if(!box){box=document.createElement('fieldset');box.style.cssText='border:0;padding:0;margin:0;min-width:0';while(f.firstChild)box.append(f.firstChild);f.append(box)}box.disabled=!allowed}$('livehome').disabled=$('livesetup').disabled=pinLocked;}

const spec=[['negativeEndpoint','Negative endpoint (%)',50,100],['positiveEndpoint','Positive endpoint (%)',50,100],['negativeRate','Negative rate (%)',0,100],['positiveRate','Positive rate (%)',0,100],['negativeExpo','Negative expo (%)',-100,100],['positiveExpo','Positive expo (%)',-100,100],['negativeRamp','Negative ramp (ms)',0,2000],['positiveRamp','Positive ramp (ms)',0,2000],['trim','Trim (‰)',-200,200],['subtrim','Channel sub-trim (‰)',-200,200],['profileSubtrim','Response sub-trim (%)',-20,20]];
for(const [key,label,min,max] of spec){const d=document.createElement('div'),l=document.createElement('label'),i=document.createElement('input');l.textContent=label;i.name=key;i.type='number';i.min=min;i.max=max;i.step=1;i.required=true;d.append(l,i);$('fields').append(d)}
for(let i=1;i<=16;i++){let o=new Option('CH'+i,i);$('channel').add(o)}
async function request(path,body){let r=await fetch(path,body?{method:'POST',headers:{'X-R1-Token':token},body}:{});if(r.headers.has('X-R1-Unlocked'))pinLocked=r.headers.get('X-R1-Unlocked')!=='1';if(r.headers.has('X-R1-Control'))setRemoteMode(r.headers.get('X-R1-Control')==='1');if(!r.ok)throw Error(await r.text());return r}
$('pinform').onsubmit=async e=>{e.preventDefault();try{notify(await (await request('/api/pin',new URLSearchParams(new FormData(e.target)))).text());e.target.reset()}catch(err){notify(err.message)}};
function showChannel(){const c=state.channels[+$('channel').value-1];for(const [key] of spec)$('settings').elements[key].value=c[key];$('settings').elements.reversed.checked=c.reversed;loadMixingForm()}

const mixControlNames=['Steering wheel','Throttle trigger','Timer button','Drive mode switch','Grip trim A','Grip trim B','Throttle trim','Steering trim','Aux 3 trim','Idle up switch','Display switch','Key Mode','Key Up','Key +','Key Select','Key Page','Key Clear','Key Down','Key -'];
let mixLoadSequence=0,mixLoadedChannel=0;
function mixingField(name,label,options,min,max){
 const wrap=document.createElement('div'),text=document.createElement('label');text.textContent=label;text.htmlFor='mix-'+name;
 const input=document.createElement(options?'select':'input');input.name=name;input.id='mix-'+name;
 if(options)for(const [value,label] of options){const o=document.createElement('option');o.value=value;o.textContent=label;input.append(o)}
 else{input.type='number';input.min=min;input.max=max;input.required=true;input.step=1}
 wrap.append(text,input);return wrap;
}
function buildMixingForm(){
 const root=$('mixfields'),switches=[3,9,10].map(id=>[id,mixControlNames[id]]),onOff=[[0,'Off'],[1,'On']];
 root.append(mixingField('includeBase','Existing channel input',[[1,'Include at 100%'],[0,'None — contributions only']]));
 const baselineTitle=document.createElement('h4');baselineTitle.textContent='Baseline and final limits';root.append(baselineTitle);
 const limits=document.createElement('div');limits.className='grid';
 for(const [key,label,options,min,max] of [
  ['constantPercent','Constant contribution (%)',null,-100,100],
  ['outputMinimum','Final output minimum (%)',null,-100,100],['outputMaximum','Final output maximum (%)',null,-100,100],
  ['cutOverride','Auxiliary CUT override',onOff],['cutPercent','Auxiliary CUT value (%)',null,-100,100]])limits.append(mixingField(key,label,options,min,max));
 root.append(limits);
 const limitNote=document.createElement('p');limitNote.textContent='Constant is added before the rules; Replace discards earlier contributions. Final limits apply after endpoints, reversal, trim and slew. The optional auxiliary CUT value is a separate final command, unaffected by these limits. It applies after model settings load, including transmitter CUT/stale states, but does not configure receiver RF-loss failsafe. Steering/throttle cannot use CUT override and their limits must include zero.';root.append(limitNote);
 const sources=[...mixControlNames.map((n,id)=>[id,'Raw: '+n]),...Array.from({length:16},(_,i)=>[19+i,'Channel '+(i+1)]),[35,'Configured steering'],[36,'Configured throttle']];
 for(let i=0;i<2;i++){
  const heading=document.createElement('h4');heading.textContent='Mix '+(i+1);root.append(heading);
  const grid=document.createElement('div');grid.className='grid';
  for(const [key,label,options,min,max] of [
   ['enabled','Enabled',onOff],['source','Source',sources],
   ['sourceMode','Source mode',[[0,'Proportional'],[1,'Switch above'],[2,'Switch below']]],
   ['thresholdPercent','Trigger point (%)',null,-100,100],['hysteresisPercent','Hysteresis (%)',null,0,100],
   ['weight','Positive weight (%)',null,-100,100],['negativeWeight','Negative weight (%)',null,-100,100],['followTrim','Follow source trim',[[0,'No'],[1,'Yes']]],['replace','Operation',[[0,'Add'],[1,'Replace']]],
   ['condition','Active when',[[0,'Always'],[1,'Switch low'],[2,'Switch high'],[3,'Walk mode'],[4,'Normal mode']]],
   ['control','Activation switch',switches]])grid.append(mixingField('mix'+i+key,label,options,min,max));
  root.append(grid);
  const thresholdNote=document.createElement('p');thresholdNote.textContent='Threshold switches contribute 0% or +100%; weights apply only to Proportional mode. Above turns on strictly above the trigger and off at trigger minus hysteresis; Below turns on strictly below and off at trigger plus hysteresis. Release point must remain within -100% to +100%. Threshold switches are for auxiliary channels and cannot feed steering/throttle. Preview starts OFF and does not retain hysteresis history.';root.append(thresholdNote);
 }
 const heading=document.createElement('h4');heading.textContent='Optional switched rates';root.append(heading);
 const note=document.createElement('p');note.textContent='Low / high are released / active switch positions. Set both negative percentages to 100% to retain full braking while limiting forward travel. Endpoints stay unchanged. Transition is milliseconds per 100 percentage points.';root.append(note);
 const grid=document.createElement('div');grid.className='grid';
 for(const [key,label,options,min,max] of [
  ['rateEnabled','Enabled',onOff],['rateControl','Rate switch',switches],
  ['lowNegative','Low: negative (%)',null,0,100],['lowPositive','Low: positive (%)',null,0,100],
  ['highNegative','High: negative (%)',null,0,100],['highPositive','High: positive (%)',null,0,100],
  ['transitionMs','Transition (ms)',null,0,2000]])grid.append(mixingField(key,label,options,min,max));
 root.append(grid);
}
async function loadMixingForm(){
 const sequence=++mixLoadSequence,ch=+$('channel').value;mixLoadedChannel=0;$('savemixing').disabled=true;
 try{
  const m=await(await request('/api/mixing?channel='+ch)).json();if(sequence!==mixLoadSequence)return;
  for(let i=0;i<2;i++)for(const [key,value] of Object.entries(m.rules[i]))$('mixsettings').elements['mix'+i+key].value=value;
  for(const [key,value] of Object.entries(m))if(key!=='rules')$('mixsettings').elements[key].value=value;
  mixLoadedChannel=ch;previewMixing();$('savemixing').disabled=false;$('mixstatus').textContent='CH'+ch+' settings loaded.';
 }catch(e){if(sequence===mixLoadSequence)$('mixstatus').textContent=e.message}
}
buildMixingForm();
for(const [name,label] of [['testBase','Existing input'],['testA','Source 1 signal'],['testB','Source 2 signal']]){
 const wrap=document.createElement('label');wrap.textContent=label;
 const input=document.createElement('input');input.type='range';input.name=name;input.min=-1000;input.max=1000;input.step=10;input.value=0;
 const value=document.createElement('output');value.textContent='0%';input.oninput=()=>value.textContent=(input.value/10).toFixed(0)+'%';
 wrap.append(input,value);$('mixtestinputs').append(wrap);
}
$('mixtestinputs').append(mixingField('testHigh','Test switches',[[0,'Low'],[1,'High']]));
$('mixtestinputs').append(mixingField('testWalk','Test drive mode',[[0,'Normal'],[1,'Walk']]));
$('mixtestinputs').append(mixingField('graphSource','Graph input',[[1,'Source 1'],[2,'Source 2'],[0,'Existing input']]));
function drawMixCurve(result){
 const c=$('mixcurve'),g=c.getContext('2d'),left=52,top=20,w=524,h=218;
 g.fillStyle='#f7f9f8';g.fillRect(0,0,c.width,c.height);
 g.font='13px system-ui';g.textAlign='right';g.textBaseline='middle';
 for(const v of [-100,0,100]){
  const y=top+(100-v)*h/200;
  g.strokeStyle=v===0?'#81969c':'#d4dfe1';g.beginPath();g.moveTo(left,y);g.lineTo(left+w,y);g.stroke();
  g.fillStyle='#193039';g.fillText(v+'%',left-7,y);
  const x=left+(v+100)*w/200;
  g.beginPath();g.moveTo(x,top);g.lineTo(x,top+h);g.stroke();
  g.textAlign='center';g.fillText(v+'%',x,top+h+18);g.textAlign='right';
 }
 g.strokeStyle='#007681';g.lineWidth=3;g.beginPath();
 result.curve.forEach((v,i)=>{const x=left+i*w/(result.curve.length-1),y=top+(1000-v)*h/2000;i?g.lineTo(x,y):g.moveTo(x,y)});
 g.stroke();g.lineWidth=1;
 g.fillStyle='#aa6500';g.beginPath();g.arc(left+(result.graphPosition+1000)*w/2000,top+(1000-result.values[4])*h/2000,5,0,Math.PI*2);g.fill();
}
let mixPreviewTimer=0,mixPreviewSequence=0;
async function previewMixing(){
 if(!mixLoadedChannel)return;
 const sequence=++mixPreviewSequence;
 const form=$('mixsettings'),data=new URLSearchParams(new FormData(form));
 data.set('channel',mixLoadedChannel);data.set('preview','1');
 try{
  const result=await(await request('/api/mixing',data)).json();if(sequence!==mixPreviewSequence)return;
  if(result.curve)drawMixCurve(result);
  $('mixbars').replaceChildren();
  result.values.forEach((value,i)=>{
   const row=document.createElement('div'),label=document.createElement('div'),track=document.createElement('div'),bar=document.createElement('div');
   label.textContent=['Existing input + constant','Mix 1 contribution','Mix 2 contribution','Combined (limited to ±100%)','Final output (including final limits)'][i]+': '+(value/10).toFixed(1)+'%';
   track.style.cssText='position:relative;height:16px;background:#d8e1e3;margin:6px 0 12px;border-left:1px solid #8b9ca0';
   const width=Math.min(50,Math.abs(value)/20);bar.style.cssText='position:absolute;height:100%;background:#087988;width:'+width+'%;left:'+(value<0?50-width:50)+'%';
   const zero=document.createElement('span');zero.style.cssText='position:absolute;left:50%;height:100%;border-left:1px solid #172e37';
   track.append(bar,zero);row.append(label,track);$('mixbars').append(row);
  });$('mixpreviewstatus').textContent='Preview only — draft has not been saved.';
 }catch(e){if(sequence===mixPreviewSequence)$('mixpreviewstatus').textContent=e.message}
}
$('mixsettings').addEventListener('input',e=>{
 const f=$('mixsettings').elements;
 if(['testA','testB','mix0source','mix1source'].includes(e.target.name)&&f.mix0source.value===f.mix1source.value){
  const changed=f[e.target.name==='testB'?'testB':'testA'];const other=f[e.target.name==='testB'?'testA':'testB'];other.value=changed.value;other.nextElementSibling.textContent=(other.value/10).toFixed(0)+'%';
 }
 clearTimeout(mixPreviewTimer);mixPreviewTimer=setTimeout(previewMixing,180);
});
function resetMixingDraft(){
 const f=$('mixsettings').elements;
 const defaults={includeBase:1,constantPercent:0,outputMinimum:-100,outputMaximum:100,cutOverride:0,cutPercent:0,rateEnabled:0,rateControl:3,lowNegative:50,lowPositive:50,highNegative:100,highPositive:100,transitionMs:0};
 for(const [key,value] of Object.entries(defaults))f[key].value=value;
 for(let i=0;i<2;i++)for(const [key,value] of Object.entries({enabled:0,source:0,weight:100,negativeWeight:100,sourceMode:0,thresholdPercent:0,hysteresisPercent:5,followTrim:0,replace:0,condition:0,control:3}))f['mix'+i+key].value=value;
 clearTimeout(mixPreviewTimer);previewMixing();
 $('mixstatus').textContent='Mixing defaults restored in draft. Select Save mixing and rates to apply.';
}
$('resetmixing').onclick=()=>{
 if(!webWritable||mixLoadedChannel!==+$('channel').value)return;
 if(confirm('Reset all mixing, baseline, limits, CUT override and switched rates for CH'+mixLoadedChannel+'? Channel endpoints, trims and control assignments stay unchanged. Save afterward to apply.'))resetMixingDraft();
};
$('mixsettings').onsubmit=async e=>{
 e.preventDefault();if(mixLoadedChannel!==+$('channel').value)return;
 const data=new URLSearchParams(new FormData(e.target));data.set('channel',mixLoadedChannel);$('savemixing').disabled=true;
 try{$('mixstatus').textContent=await(await request('/api/mixing',data)).text();notify($('mixstatus').textContent)}
 catch(e){$('mixstatus').textContent=e.message}finally{$('savemixing').disabled=mixLoadedChannel!==+$('channel').value}
};
let memoryPending=false;
function memorySize(bytes){return Number.isFinite(bytes)&&bytes>=0?(bytes/1024).toFixed(1)+' KiB':'Unavailable'}
async function loadMemory(){
 if(memoryPending)return;memoryPending=true;$('memoryrefresh').disabled=true;
 try{
  const m=await(await request('/api/memory')).json();
  const rows=document.createDocumentFragment();
  for(const [key,label] of [['internal','Internal RAM'],['psram','PSRAM'],['dma','DMA (internal subset)']]){
   const p=m[key],row=document.createElement('tr');
   for(const value of [label,...['free','minimum','largest','total'].map(k=>memorySize(p[k]))]){
    const cell=document.createElement('td');cell.textContent=value;row.append(cell);
   }rows.append(row);
  }$('memorytable').querySelector('tbody').replaceChildren(rows);
  $('memorylvgl').textContent=m.lvgl.ready?`LVGL: ${memorySize(m.lvgl.used)} used / ${memorySize(m.lvgl.total)} pool · Sampled peak ${memorySize(m.lvgl.peak)} · Largest free block ${memorySize(m.lvgl.largest)} · Fragmentation ${m.lvgl.fragmentation}%`:'LVGL: not initialized';
  $('memorystacks').textContent='Minimum unused stack: '+[['ui','UI / web'],['control','Control'],['crsf','CRSF']].map(([key,name])=>name+' '+(m.stacks[key]<0?'not started':m.stacks[key]+' bytes')).join(' · ');
  $('memorystatus').textContent=`Snapshot at ${(m.sampledMs/1000).toFixed(1)} seconds after boot. Updated ${new Date().toLocaleTimeString()}.`;
 }catch(e){$('memorystatus').textContent='Memory refresh failed: '+e.message+' (any values shown are from the previous successful sample).'}
 finally{memoryPending=false;$('memoryrefresh').disabled=false;}
}
$('memoryrefresh').onclick=loadMemory;
$('memoryauto').onchange=()=>{if($('memoryauto').checked)loadMemory()};
setInterval(()=>{if($('memoryauto').checked&&!document.hidden)loadMemory()},5000);
async function load(){state=await(await request('/api/state')).json();token=state.token;$('hostname').value=state.hostname||'r1evo';$('hostnameaddress').textContent=`Current address: http://${state.activeHostname||'r1evo'}.local`+(state.hostname&&state.hostname!==state.activeHostname?` · After restart: http://${state.hostname}.local`:'');$('wifitiming').textContent=Number.isFinite(state.webReadyMs)?`Network diagnostics: IP ready ${(state.ipReadyMs/1000).toFixed(1)}s after boot · Web ready ${(state.webReadyMs/1000).toFixed(1)}s · Attempts ${state.wifiAttempts} · Last disconnect reason ${state.lastDisconnectReason}`:'';setRemoteMode(state.controlAllowed===true);$('model').elements.name.value=state.model;$('modelimage').value=state.modelImage??0;$('originalcolour').checked=(state.modelColour??16777216)===16777216;$('bodycolour').value='#'+((state.modelColour??16777216)&16777215).toString(16).padStart(6,'0');for(const [key,picker,check] of [['modelAccent','accentcolour','originalaccent'],['modelBase','basecolour','originalbase']]){const c=state[key]??16777216;$(check).checked=c===16777216;$(picker).value='#'+(c&16777215).toString(16).padStart(6,'0');}$('connection').textContent=`Model ${state.slot} · ${state.ssid||'Setup access point'} · ${state.ip||'192.168.4.1'}`;$('mapstatus').textContent=state.map?'Installed: '+state.map:'No uploaded map; built-in map remains available';$('prepare').hidden=state.storage;showChannel();loadMemory()}
$('hostnameform').onsubmit=async e=>{e.preventDefault();try{const result=await(await request('/api/hostname',new URLSearchParams({hostname:$('hostname').value}))).text();await load();notify(result)}catch(err){notify(err.message)}};
$('restarthandset').onclick=async()=>{if(!webWritable){notify('Keep outputs CUT before restarting');return}if(!confirm('Restart the handset now? Wi-Fi will briefly disconnect. Saved settings will be kept.'))return;try{$('restarthandset').disabled=true;notify(await(await request('/api/restart',new URLSearchParams({confirm:'restart'}))).text());stopLive()}catch(e){notify(e.message)}finally{$('restarthandset').disabled=false}};
$('channel').onchange=showChannel;
$('bodycolour').oninput=()=>{$('originalcolour').checked=false};
$('accentcolour').oninput=()=>{$('originalaccent').checked=false};$('basecolour').oninput=()=>{$('originalbase').checked=false};$('resetpaint').onclick=()=>{for(const id of ['originalcolour','originalaccent','originalbase'])$(id).checked=true};
$('appearance').onsubmit=async e=>{e.preventDefault();try{notify(await(await request('/api/model-appearance',new URLSearchParams({image:$('modelimage').value,colour:$('originalcolour').checked?'original':$('bodycolour').value,accent:$('originalaccent').checked?'original':$('accentcolour').value,base:$('originalbase').checked?'original':$('basecolour').value}))).text());await load()}catch(e){notify(e.message)}};
$('model').onsubmit=async e=>{e.preventDefault();try{notify(await(await request('/api/model',new URLSearchParams(new FormData(e.target)))).text());await load()}catch(e){notify(e.message)}};
$('settings').onsubmit=async e=>{e.preventDefault();let data=new URLSearchParams(new FormData(e.target));data.set('channel',$('channel').value);data.set('reversed',e.target.elements.reversed.checked?'1':'0');try{notify(await(await request('/api/channel',data)).text());await load()}catch(e){notify(e.message)}};
function download(data,name){const a=document.createElement('a'),url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'}));a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000)}
$('backup').onclick=()=>download({version:1,model:state.model,slot:state.slot,channels:state.channels,controls:catalogue?.controls},'r1-parameters.json');
$('prepare').onclick=async()=>{if(!confirm('Initialise the asset filesystem? This erases its contents, but not model settings. Only use for uninitialised storage.'))return;try{notify(await(await request('/api/storage',new URLSearchParams({confirm:'prepare'}))).text());await load()}catch(e){notify(e.message)}};
$('download').onclick=async()=>{try{downloadBlob(await(await request('/api/map')).blob(),'r1-map.r1map')}catch(e){notify(e.message)}};
function collect(g,paths){if(!g)return;if(g.type==='FeatureCollection')g.features.forEach(f=>collect(f,paths));else if(g.type==='Feature')collect(g.geometry,paths);else if(g.type==='LineString')paths.push(g.coordinates);else if(g.type==='MultiLineString'||g.type==='Polygon')paths.push(...g.coordinates);else if(g.type==='MultiPolygon')g.coordinates.forEach(p=>paths.push(...p));else if(g.type==='GeometryCollection')g.geometries.forEach(p=>collect(p,paths))}
function validate(m){if(m.version!==1||!Array.isArray(m.paths)||!m.paths.length||m.paths.length>32)throw Error('Use 1–32 line paths');let p=m.paths.flat();if(p.length>512||m.paths.some(p=>p.length<2))throw Error('Maximum 512 points; simplify the GeoJSON first');if(p.some(v=>!Array.isArray(v)||v.length!==2||!v.every(Number.isFinite)||Math.abs(v[0])>180||Math.abs(v[1])>85))throw Error('Expected [longitude, latitude] coordinates');const xs=p.map(p=>p[0]),ys=p.map(p=>p[1]);if(Math.max(...xs)-Math.min(...xs)>.25||Math.max(...ys)-Math.min(...ys)>.25)throw Error('Select a local area smaller than 0.25 degrees');return p}
function preview(){if(map.version===2){$('preview').width=map.width;$('preview').height=map.height;$('preview').getContext('2d').drawImage(map.canvas,0,0);$('mapdetails').textContent=`${map.sourceLabel||'Image map'} · ${map.width} × ${map.height} pixels · ${Math.ceil(map.pixels.length/1024)} KiB · North ↑ · GPS trace overlays this image`;$('savemap').disabled=$('exportmap').disabled=false;return}$('preview').width=600;$('preview').height=300;const p=validate(map),ctx=$('preview').getContext('2d'),xs=p.map(p=>p[0]),ys=p.map(p=>p[1]),minX=Math.min(...xs),maxY=Math.max(...ys),cos=Math.cos(maxY*Math.PI/180),scale=Math.min(560/Math.max((Math.max(...xs)-minX)*cos,1e-7),260/Math.max(maxY-Math.min(...ys),1e-7));ctx.clearRect(0,0,600,300);ctx.strokeStyle='#00717c';ctx.lineWidth=2;for(const line of map.paths){ctx.beginPath();line.forEach((p,i)=>{const x=20+(p[0]-minX)*cos*scale,y=20+(maxY-p[1])*scale;i?ctx.lineTo(x,y):ctx.moveTo(x,y)});ctx.stroke()}$('mapdetails').textContent=`${map.paths.length} paths · ${p.length} points · North ↑`;$('savemap').disabled=$('exportmap').disabled=false}
$('mapfile').onchange=async e=>{try{map=null;$('savemap').disabled=$('exportmap').disabled=true;const f=e.target.files[0];if(!f)return;if(f.size>526344)throw Error('Map file is too large');const buffer=await f.arrayBuffer();if(new Uint8Array(buffer)[0]===82){map=readImagePackage(buffer);$('mapname').value=map.name;$('attribution').value=map.attribution;preview();notify('Image map ready to review');return}const d=JSON.parse(new TextDecoder().decode(buffer));if(d.version===1)map=d;else{let paths=[];collect(d,paths);map={version:1,name:f.name.replace(/\.[^.]+$/,'').slice(0,40),attribution:'',paths}}$('mapname').value=map.name||'';$('attribution').value=map.attribution||'';preview();notify('Map ready to review')}catch(e){map=null;notify(e.message)}};
function packageMap(){map.name=$('mapname').value.trim();map.attribution=$('attribution').value.trim();if(map.version!==2)validate(map);if(new TextEncoder().encode(map.name).length>40||new TextEncoder().encode(map.attribution).length>80)throw Error('Name or attribution too long');if(!map.name||!map.attribution)throw Error('Enter map name and attribution');if(map.version!==2&&JSON.stringify(map).length>48000)throw Error('Map package exceeds 48 KB');return map}
function mapStatus(text){$('savestatus').textContent=text;notify(text)}
function uploadMap(data){return new Promise((resolve,reject)=>{const xhr=new XMLHttpRequest();xhr.open('POST','/api/map');xhr.timeout=60000;xhr.setRequestHeader('X-R1-Token',token);xhr.upload.onprogress=e=>mapStatus(e.lengthComputable?`Uploading map: ${Math.round(e.loaded/e.total*100)}%`:'Uploading map…');xhr.upload.onload=()=>mapStatus('Upload sent. Waiting for handset to validate and save…');xhr.onload=()=>{const mode=xhr.getResponseHeader('X-R1-Control');if(mode!==null)setRemoteMode(mode==='1');if(xhr.status>=200&&xhr.status<300)resolve(xhr.responseText);else {const error=Error(xhr.responseText||`Map upload failed (${xhr.status})`);error.sessionExpired=xhr.status===403&&xhr.responseText.trim()==='Invalid setup session';reject(error)}};xhr.onerror=()=>reject(Error('Connection lost. Check the handset connection before retrying.'));xhr.ontimeout=()=>reject(Error('No confirmation after 60 seconds. The save result is unknown; reconnect and check the installed map before retrying.'));xhr.send(data)})}
async function refreshMapSession(){const fresh=await(await request('/api/state')).json();if(typeof fresh.token!=='string'||!fresh.token)throw Error('Could not refresh the handset session; your prepared map is still available to export');token=fresh.token;setRemoteMode(fresh.controlAllowed===true);if(fresh.controlAllowed!==true)throw Error('Keep the transmitter in CUT to save a map');if(!fresh.storage)throw Error('Use Prepare map storage before saving a map');}
async function saveMapWithSession(data){await refreshMapSession();try{return await uploadMap(data)}catch(e){if(!e.sessionExpired)throw e;mapStatus('Handset session changed; reconnecting and retrying map once…');await refreshMapSession();return await uploadMap(data)}}
$('savemap').onclick=async()=>{if(mapBusy)return;try{const data=new FormData(),m=packageMap();data.append('map',m.version===2?imageBlob(m):new Blob([JSON.stringify(m)],{type:'application/json'}),'map.r1map');mapBusy=true;$('savemap').disabled=true;mapStatus('Checking handset session…');mapStatus(await saveMapWithSession(data));try{await load()}catch(e){mapStatus('Map saved, but status refresh failed: '+e.message)}}catch(e){mapStatus(e.message)}finally{mapBusy=false;$('savemap').disabled=!map}};
$('exportmap').onclick=()=>{try{const m=packageMap();m.version===2?downloadBlob(imageBlob(m),'r1-map.r1map'):download(m,'r1-map.json')}catch(e){notify(e.message)}};
// Control IDs and available functions come from the firmware catalogue.
let catalogue,selectedControl=0;
const optionList=(element,options,value)=>{element.replaceChildren(...options.map(([v,t])=>new Option(t,v)));if(value!==undefined&&options.some(o=>String(o[0])===String(value)))element.value=value};
const behaviourNames=['Proportional','Momentary','Toggle','Two position','Spring return (3 positions)','Stepped','Three-position selector','Pulse'];
function bindingSummary(c){if(c.kind===0)return `CH${c.selection} · ${c.enabled?'enabled':'neutral'}`;if(!c.enabled)return 'Default action';return c.target===4?`CH${c.selection} · ${behaviourNames[c.behaviour]}`:c.target===1?(catalogue.functions.find(f=>f.id===c.selection)?.name||'Function'):c.target===3?(c.selection===0?'Arm / cut':'Walk / normal'):`CH${(c.selection>>8)&31} ${c.selection&1?'sub-trim':'trim'}`}
async function loadControls(){catalogue=await(await request('/api/controls')).json();for(const id of ['primarycontrols','othercontrols','keycontrols'])$(id).replaceChildren();for(const c of catalogue.controls){const b=document.createElement('button');b.type='button';b.dataset.control=c.id;b.setAttribute('aria-pressed',c.id===selectedControl);const name=document.createElement('strong'),desc=document.createElement('small');name.textContent=c.name.replace('KEY: ','');desc.textContent=bindingSummary(c);b.append(name,desc);b.onclick=()=>selectControl(c.id);$(c.id<2?'primarycontrols':c.id<11?'othercontrols':'keycontrols').append(b)}selectControl(selectedControl)}
function selectControl(id){selectedControl=id;const c=catalogue.controls[id];document.querySelectorAll('[data-control]').forEach(b=>b.setAttribute('aria-pressed',+b.dataset.control===id));$('controlname').textContent=c.name;$('controlhint').textContent=c.kind===0?'Primary input: select its output channel.':'Select a channel or function, then configure its control range.';optionList($('target'),c.kind===0?[[4,'Primary channel']]:c.kind===3?[[4,'Auxiliary channel'],[2,'Channel trim / sub-trim']]:c.kind===2?[[4,'Auxiliary channel'],[1,'Handset function'],[3,'Drive mode']]:[[4,'Auxiliary channel'],[1,'Handset function']],c.target);$('context').value=c.context;$('bindingenabled').checked=c.enabled;updateBindingOptions(c.selection,c.behaviour);setBindingValues(c);$('contextfield').hidden=c.kind===0}
function updateBindingOptions(selection,behaviour){const c=catalogue.controls[selectedControl],t=+$('target').value;let options=[];if(t===4)options=Array.from({length:16},(_,i)=>[i+1,`CH${i+1}`]);if(t===1)options=catalogue.functions.map(f=>[f.id,f.name]);if(t===3)options=[[0,'Arm / cut'],[1,'Walk / normal']];if(t===2)for(let i=1;i<=16;i++)for(let p=0;p<2;p++)options.push([0x8000|(i<<8)|p,`CH${i} ${p?'sub-trim':'trim'}`]);optionList($('selection'),options,selection);let modes=c.kind===0?[0]:c.kind===2?[3]:c.kind===3?[4,5,6]:[1,2,7];optionList($('behaviour'),modes.map(v=>[v,behaviourNames[v]]),behaviour);$('behaviourfield').hidden=t!==4||c.kind===0;const ranged=c.kind!==0&&(t===4||t===2);$('bindingvalues').hidden=!ranged;for(const key of ['minimum','maximum','step','initial','failsafe'])$('binding'+key).disabled=!ranged||t===2&&(key==='initial'||key==='failsafe')}
function setBindingValues(c){const parameter=+$('target').value===2,limit=parameter?200:1000;for(const key of ['minimum','maximum','step','initial','failsafe']){const el=$('binding'+key);el.min=key==='step'?0.1:-limit/10;el.max=key==='step'?limit/5:limit/10;el.value=(c[key]??(key==='minimum'?-limit:key==='maximum'?limit:key==='step'?50:0))/10}$('bindinginverted').checked=!!c.inverted}
$('target').onchange=()=>{updateBindingOptions();const parameter=+$('target').value===2;setBindingValues({minimum:parameter?-200:-1000,maximum:parameter?200:1000,step:parameter?10:50,initial:0,failsafe:0,inverted:false})};
$('binding').onsubmit=async e=>{e.preventDefault();$('savebinding').disabled=true;try{const body=new URLSearchParams({input:selectedControl,target:$('target').value,selection:$('selection').value,behaviour:$('behaviour').value,context:$('context').value,enabled:$('bindingenabled').checked?1:0});if(!$('bindingvalues').hidden){for(const key of ['minimum','maximum','step','initial','failsafe'])body.set(key,Math.round(Number($('binding'+key).value)*10));body.set('inverted',$('bindinginverted').checked?1:0)}notify(await(await request('/api/control',body)).text());await loadControls()}catch(e){notify(e.message)}finally{$('savebinding').disabled=false}};

// Street browsing uses OSM tiles; offline street images are rendered from OSM data.
// Satellite downloads use a single georeferenced image export, not a tile cache.
const satelliteService='https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer';
const satelliteCredit='Esri, Vantor, Earthstar Geographics, and the GIS User Community';
function mapTileUrl(style,z,x,y){const n=2**z;x=((x%n)+n)%n;return style==='satellite'?`${satelliteService}/tile/${z}/${y}/${x}`:`https://tile.openstreetmap.org/${z}/${x}/${y}.png`}

const mapView={lon:144.9631,lat:-37.8136,z:16,open:false,style:'street'};let pan=null,tileTimer,searchAt=0,areaBusy=false;const searchCache=new Map();
function world(lon,lat,z){const n=256*2**z,s=Math.sin(Math.max(-85,Math.min(85,lat))*Math.PI/180);return [(lon+180)/360*n,(.5-Math.log((1+s)/(1-s))/(4*Math.PI))*n]}
function unworld(x,y,z){const n=256*2**z;return [x/n*360-180,Math.atan(Math.sinh(Math.PI*(1-2*y/n)))*180/Math.PI]}
function selectionSize(){return Math.min($('slippy').clientWidth,$('slippy').clientHeight)*.8}
function selectionBounds(){const [x,y]=world(mapView.lon,mapView.lat,mapView.z),w=selectionSize(),h=w;const [west,north]=unworld(x-w/2,y-h/2,mapView.z),[east,south]=unworld(x+w/2,y+h/2,mapView.z);return {west,east,south,north}}
function renderTiles(){if(!mapView.open)return;$('mapframe').style.width=$('mapframe').style.height=selectionSize()+'px';const el=$('slippy'),w=el.clientWidth,h=el.clientHeight,[cx,cy]=world(mapView.lon,mapView.lat,mapView.z),left=cx-w/2,top=cy-h/2,n=2**mapView.z;const wanted=new Set();for(let x=Math.floor(left/256);x<=Math.floor((left+w)/256);x++)for(let y=Math.floor(top/256);y<=Math.floor((top+h)/256);y++){if(y<0||y>=n)continue;const key=`${mapView.style}-${mapView.z}-${x}-${y}`;wanted.add(key);let img=$('tiles').querySelector(`[data-tile="${key}"]`);if(!img){img=new Image();img.alt='';img.dataset.tile=key;img.draggable=false;img.referrerPolicy='strict-origin-when-cross-origin';img.src=mapTileUrl(mapView.style,mapView.z,x,y);img.onerror=()=>{$('mapprogress').textContent='Map imagery unavailable. Check internet access; file import still works.'};$('tiles').append(img)}img.style.left=`${x*256-left}px`;img.style.top=`${y*256-top}px`}for(const img of $('tiles').children)if(!wanted.has(img.dataset.tile))img.remove();const b=selectionBounds();$('bounds').textContent=`${mapView.lat.toFixed(5)}, ${mapView.lon.toFixed(5)} · zoom ${mapView.z}`;$('fetcharea').disabled=areaBusy||b.east-b.west>.25||b.north-b.south>.25||b.west< -180||b.east>180;$('zoomout').disabled=mapView.z<=12;$('zoomin').disabled=mapView.z>=19}
function openMap(){mapView.open=true;$('mapbrowser').hidden=false;$('openmap').hidden=true;renderTiles()}
$('openmap').onclick=openMap;
$('mapstyle').onchange=()=>{mapView.style=$('mapstyle').value==='satellite'?'satellite':'street';map=null;$('savemap').disabled=$('exportmap').disabled=true;$('preview').getContext('2d').clearRect(0,0,$('preview').width,$('preview').height);$('mapdetails').textContent='Download the selected area to prepare a new preview.';$('savestatus').textContent='';$('mapprogress').textContent='';$('mapcredits').textContent=mapView.style==='satellite'?satelliteCredit:'© OpenStreetMap contributors';$('mapstylehint').textContent=mapView.style==='satellite'?'Satellite / aerial photography. Detail and capture date vary by location. Credits are included in the downloaded image.':'Street maps include roads, buildings, parks and labels.';openMap()};
function zoom(delta){mapView.z=Math.max(12,Math.min(19,mapView.z+delta));renderTiles()}
$('zoomin').onclick=()=>zoom(1);$('zoomout').onclick=()=>zoom(-1);
$('slippy').onpointerdown=e=>{if(e.button!==0)return;pan={x:e.clientX,y:e.clientY,origin:world(mapView.lon,mapView.lat,mapView.z)};$('slippy').setPointerCapture(e.pointerId)};
$('slippy').onpointermove=e=>{if(!pan)return;const [lon,lat]=unworld(pan.origin[0]-e.clientX+pan.x,pan.origin[1]-e.clientY+pan.y,mapView.z);mapView.lon=((lon+540)%360)-180;mapView.lat=Math.max(-85,Math.min(85,lat));clearTimeout(tileTimer);tileTimer=setTimeout(renderTiles,80)};
$('slippy').onpointerup=$('slippy').onpointercancel=()=>{pan=null;renderTiles()};
$('slippy').onkeydown=e=>{const movement={ArrowLeft:[-100,0],ArrowRight:[100,0],ArrowUp:[0,-100],ArrowDown:[0,100]}[e.key];if(movement){e.preventDefault();let p=world(mapView.lon,mapView.lat,mapView.z);[mapView.lon,mapView.lat]=unworld(p[0]+movement[0],p[1]+movement[1],mapView.z);renderTiles()}};
window.addEventListener('resize',()=>{clearTimeout(tileTimer);tileTimer=setTimeout(renderTiles,150)});
async function externalJSON(url,options={}){const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),35000);try{const r=await fetch(url,{...options,credentials:'omit',signal:controller.signal});if(!r.ok)throw Error(`Map service returned ${r.status}; try again later`);return await r.json()}finally{clearTimeout(timer)}}
$('placesearch').onsubmit=async e=>{e.preventDefault();const q=$('address').value.trim();if(!q||Date.now()-searchAt<1200)return;searchAt=Date.now();$('searchbutton').disabled=true;try{let data=searchCache.get(q);if(!data){data=await externalJSON('https://photon.komoot.io/api/?limit=5&q='+encodeURIComponent(q));searchCache.set(q,data)}$('places').replaceChildren();for(const f of data.features||[]){const p=f.properties,b=document.createElement('button');b.type='button';b.textContent=[p.name,p.street,p.city,p.state,p.country].filter(Boolean).join(', ');b.onclick=()=>{[mapView.lon,mapView.lat]=f.geometry.coordinates;mapView.z=17;$('mapname').value=(p.name||q).slice(0,40);openMap();$('places').replaceChildren()};$('places').append(b)}if(!data.features?.length)notify('No matches. Try a suburb or nearby street, or pan the map.')}catch(e){notify('Search unavailable: '+e.message)}finally{setTimeout(()=>$('searchbutton').disabled=false,1200)}};
// Clip each segment to the chosen area so ways crossing the boundary cannot
// enlarge the handset map or join unrelated line fragments.
function clipPaths(path,b){const out=[];let current=[];for(let i=1;i<path.length;i++){const a=path[i-1],c=path[i],dx=c[0]-a[0],dy=c[1]-a[1];let lo=0,hi=1,ok=true;const ps=[-dx,dx,-dy,dy],qs=[a[0]-b.west,b.east-a[0],a[1]-b.south,b.north-a[1]];for(let k=0;k<4;k++){if(ps[k]===0){if(qs[k]<0)ok=false}else{const t=qs[k]/ps[k];if(ps[k]<0)lo=Math.max(lo,t);else hi=Math.min(hi,t)}}if(!ok||lo>hi){if(current.length>1)out.push(current);current=[];continue}const first=[a[0]+lo*dx,a[1]+lo*dy],last=[a[0]+hi*dx,a[1]+hi*dy];if(current.length&&Math.hypot(current.at(-1)[0]-first[0],current.at(-1)[1]-first[1])>1e-9){if(current.length>1)out.push(current);current=[]}if(!current.length)current.push(first);current.push(last)}if(current.length>1)out.push(current);return out}
function simplifyLine(points,tolerance){if(points.length<=2)return points;let max=0,index=0;const a=points[0],b=points.at(-1),dx=b[0]-a[0],dy=b[1]-a[1],den=dx*dx+dy*dy;for(let i=1;i<points.length-1;i++){const p=points[i],t=den?Math.max(0,Math.min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/den)):0,d=Math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy);if(d>max){max=d;index=i}}return max>tolerance?[...simplifyLine(points.slice(0,index+1),tolerance).slice(0,-1),...simplifyLine(points.slice(index),tolerance)]:[a,b]}
function vectorPackage(data,b){if(data.remark)throw Error('Map service did not complete the area query; try a smaller area');let paths=[];for(const way of data.elements||[])if(way.geometry)paths.push(...clipPaths(way.geometry.map(p=>[p.lon,p.lat]),b));if(!paths.length)throw Error('No mapped outlines here. Move or widen the area, or import a track file.');if(paths.length>32)throw Error(`${paths.length} outlines in this area; zoom in to fit the 32-path limit.`);let simplified=paths;for(let tolerance=1e-6;simplified.flat().length>512&&tolerance<=.001;tolerance*=2)simplified=paths.map(p=>simplifyLine(p,tolerance));const result={version:1,name:$('mapname').value.trim()||'Driving area',attribution:'© OpenStreetMap contributors (ODbL)',paths:simplified.map(p=>p.map(c=>c.map(v=>+v.toFixed(7))))};validate(result);return result}
async function downloadStreetImage(b){const box=[b.south,b.west,b.north,b.east].join(',');const query=`[out:json][timeout:25][maxsize:4000000];(way[highway](${box});way[building](${box});way[landuse](${box});way[leisure](${box});way[natural](${box});way[waterway](${box});relation[type=multipolygon][leisure](${box});relation[type=multipolygon][natural](${box});relation[type=multipolygon][landuse](${box}););out geom;`;return renderOsmImage(await externalJSON('https://overpass-api.de/api/interpreter',{method:'POST',body:new URLSearchParams({data:query})}),b)}
function satelliteCrop(extent,b,w,h){if(!extent||extent.spatialReference?.wkid!==4326||![extent.xmin,extent.xmax,extent.ymin,extent.ymax].every(Number.isFinite)||extent.xmin>b.west+1e-8||extent.xmax<b.east-1e-8||extent.ymin>b.south+1e-8||extent.ymax<b.north-1e-8||extent.xmax<=extent.xmin||extent.ymax<=extent.ymin)throw Error('Satellite service returned invalid geographic bounds');return [(b.west-extent.xmin)/(extent.xmax-extent.xmin)*w,(extent.ymax-b.north)/(extent.ymax-extent.ymin)*h,(b.east-b.west)/(extent.xmax-extent.xmin)*w,(b.north-b.south)/(extent.ymax-extent.ymin)*h]}
function loadMapImage(url){return new Promise((resolve,reject)=>{const image=new Image(),timer=setTimeout(()=>{image.src='';reject(Error('Satellite image timed out; try again'))},35000);image.crossOrigin='anonymous';image.onload=()=>{clearTimeout(timer);resolve(image)};image.onerror=()=>{clearTimeout(timer);reject(Error('Satellite image unavailable; check internet access or try another area'))};image.src=url})}
async function downloadSatelliteImage(b){
 const aspect=(b.east-b.west)*Math.cos((b.north+b.south)*Math.PI/360)/(b.north-b.south),w=aspect>=1?512:Math.max(2,Math.round(512*aspect)),h=aspect>=1?Math.max(2,Math.round(512/aspect)):512;
 const query=new URLSearchParams({bbox:[b.west,b.south,b.east,b.north].join(','),bboxSR:'4326',imageSR:'4326',size:`${w},${h}`,format:'jpg',f:'json'});
 const result=await externalJSON(satelliteService+'/export?'+query);
 if(result.error)throw Error('Satellite service: '+(result.error.message||'export failed'));
 const url=new URL(result.href);if(url.protocol!=='https:'||url.hostname!=='server.arcgisonline.com')throw Error('Unexpected satellite image location');
 const crop=satelliteCrop(result.extent,b,result.width,result.height),image=await loadMapImage(url.href);
 const canvas=document.createElement('canvas');canvas.width=w;canvas.height=h;const ctx=canvas.getContext('2d');ctx.drawImage(image,...crop,0,0,w,h);
 // Keep credits inside the image so they survive package transfer and offline use.
 ctx.font='10px sans-serif';const lines=[];let line='';for(const word of satelliteCredit.split(' ')){const next=line?line+' '+word:word;if(ctx.measureText(next).width>w-8&&line){lines.push(line);line=word}else line=next}if(line)lines.push(line);
 ctx.fillStyle='#18252ddd';ctx.fillRect(0,h-lines.length*12-4,w,lines.length*12+4);ctx.fillStyle='#ffffff';ctx.textBaseline='top';lines.forEach((line,i)=>ctx.fillText(line,4,h-lines.length*12-2+i*12));
 const rgba=ctx.getImageData(0,0,w,h),pixels=new Uint8Array(w*h*2);for(let i=0;i<w*h;i++){const v=((rgba.data[i*4]>>3)<<11)|((rgba.data[i*4+1]>>2)<<5)|(rgba.data[i*4+2]>>3);pixels[i*2]=v&255;pixels[i*2+1]=v>>8;rgba.data[i*4]=Math.round((v>>11)*255/31);rgba.data[i*4+1]=Math.round(((v>>5)&63)*255/63);rgba.data[i*4+2]=Math.round((v&31)*255/31)}ctx.putImageData(rgba,0,0);
 return {version:2,format:'rgb565le',projection:'equirectangular',sourceLabel:'Satellite · Esri',name:$('mapname').value.trim()||'Driving area',attribution:satelliteCredit,bounds:b,width:w,height:h,pixels,canvas};
}
$('fetcharea').onclick=async()=>{if(areaBusy)return;const b=selectionBounds(),style=mapView.style;if(b.east-b.west>.25||b.north-b.south>.25||b.west< -180||b.east>180){notify('Zoom in to select a smaller area away from the date line');return}areaBusy=true;$('mapstyle').disabled=true;map=null;$('savemap').disabled=$('exportmap').disabled=true;renderTiles();$('mapprogress').textContent=style==='satellite'?'Downloading satellite image…':'Preparing street map…';try{map=await(style==='satellite'?downloadSatelliteImage(b):downloadStreetImage(b));$('mapname').value=map.name;$('attribution').value=map.attribution;preview();$('mapprogress').textContent='Ready below. Review the handset preview, then save.';notify('Map downloaded to browser; not yet saved to handset.')}catch(e){$('mapprogress').textContent=e.message;notify(e.message)}finally{areaBusy=false;$('mapstyle').disabled=false;renderTiles()}};

// Render OSM feature data ourselves. No browsing-layer tiles enter the package.
function renderOsmImage(data,b){
 if(data.remark)throw Error('Map service did not complete the query; try a smaller area.');
 const ways=(data.elements||[]).filter(e=>e.geometry?.length>1);
 const relations=(data.elements||[]).filter(e=>e.type==='relation'&&e.members);
 if(!ways.length&&!relations.length)throw Error('No OSM features here. Choose another area.');
 const cos=Math.cos((b.north+b.south)*Math.PI/360),aspect=(b.east-b.west)*cos/(b.north-b.south);
 const w=aspect>=1?512:Math.max(2,Math.round(512*aspect)),h=aspect>=1?Math.max(2,Math.round(512/aspect)):512;
 const canvas=document.createElement('canvas');canvas.width=w;canvas.height=h;const ctx=canvas.getContext('2d');
 const point=p=>[(p.lon-b.west)/(b.east-b.west)*w,(b.north-p.lat)/(b.north-b.south)*h];
 const features=[...ways.map(e=>({tags:e.tags||{},rings:[e.geometry]})),...relations.map(e=>{
  const rings=[];for(const role of ['outer','inner']){const pieces=e.members.filter(m=>m.type==='way'&&((m.role||'outer')===role)&&m.geometry).map(m=>m.geometry.slice());
   const same=(a,b)=>a.lon===b.lon&&a.lat===b.lat;
   while(pieces.length){let ring=pieces.shift(),changed=true;while(changed&&!same(ring[0],ring.at(-1))){changed=false;for(let i=0;i<pieces.length;i++){let p=pieces[i];if(same(ring.at(-1),p[0])){}else if(same(ring.at(-1),p.at(-1)))p=p.slice().reverse();else continue;ring.push(...p.slice(1));pieces.splice(i,1);changed=true;break}}if(ring.length>3&&same(ring[0],ring.at(-1)))rings.push(ring)}
  }return {tags:e.tags||{},rings};})];
 const closed=f=>f.rings.length&&f.rings[0].length>3&&f.rings[0][0].lon===f.rings[0].at(-1).lon&&f.rings[0][0].lat===f.rings[0].at(-1).lat;
 function path(f){ctx.beginPath();for(const ring of f.rings){ring.forEach((p,i)=>{const [x,y]=point(p);i?ctx.lineTo(x,y):ctx.moveTo(x,y)});if(closed(f))ctx.closePath()}}
 function fill(f,color){path(f);ctx.fillStyle=color;ctx.fill('evenodd')}
 ctx.fillStyle='#eeece5';ctx.fillRect(0,0,w,h);
 for(const f of features){const t=f.tags;if(!closed(f)||t.building)continue;let color=t.natural==='water'||t.waterway==='riverbank'?'#9dccdf':t.leisure==='pitch'?'#a4ce9c':t.landuse==='forest'||t.natural==='wood'?'#a8c697':t.leisure||t.landuse==='grass'||t.landuse==='meadow'||t.natural==='grassland'?'#c4dfb3':t.landuse==='residential'?'#e1dfdb':t.landuse==='industrial'?'#ddd5dd':null;if(color)fill(f,color)}
 for(const f of features)if(f.tags.building&&closed(f)){fill(f,'#cbbfb3');path(f);ctx.strokeStyle='#b2a596';ctx.lineWidth=.7;ctx.stroke()}
 for(const f of features)if(f.tags.waterway&&!closed(f)){path(f);ctx.strokeStyle='#80bcd8';ctx.lineWidth=f.tags.waterway==='river'?5:2;ctx.stroke()}
 const roads=features.filter(f=>f.tags.highway);
 for(const outer of [true,false])for(const f of roads){const type=f.tags.highway,trail=['footway','path','cycleway','steps','track'].includes(type);path(f);ctx.lineCap='round';ctx.lineJoin='round';ctx.setLineDash(trail?[3,2]:[]);ctx.strokeStyle=trail?'#a99783':outer?'#c2bcb0':['motorway','trunk','primary','secondary'].includes(type)?'#f3d891':'#ffffff';ctx.lineWidth=trail?1.4:outer?7:5;ctx.stroke()}ctx.setLineDash([]);
 // Labels are baked into the image; omit overlaps rather than unreadable text.
 const labels=[];ctx.font='12px sans-serif';ctx.textAlign='center';ctx.textBaseline='middle';
 for(const f of features){let text=f.tags.name;if(!text&&f.tags.building)text=f.tags['addr:housenumber'];if(!text||text.length>45||!f.rings[0]?.length)continue;const ring=f.rings[0],p=f.tags.highway?ring[Math.floor(ring.length/2)]:{lon:ring.reduce((s,p)=>s+p.lon,0)/ring.length,lat:ring.reduce((s,p)=>s+p.lat,0)/ring.length},[x,y]=point(p),tw=ctx.measureText(text).width+6;
  if(x-tw/2<1||x+tw/2>w-1||y<9||y>h-9||labels.some(a=>Math.abs(x-a.x)<(tw+a.w)/2&&Math.abs(y-a.y)<16))continue;
  labels.push({x,y,w:tw});ctx.strokeStyle='#ffffff';ctx.lineWidth=3;ctx.strokeText(text,x,y);ctx.fillStyle='#364643';ctx.fillText(text,x,y);
 }
 const rgba=ctx.getImageData(0,0,w,h),pixels=new Uint8Array(w*h*2);
 for(let i=0;i<w*h;i++){const r=rgba.data[i*4],g=rgba.data[i*4+1],blue=rgba.data[i*4+2],v=((r>>3)<<11)|((g>>2)<<5)|(blue>>3);pixels[i*2]=v&255;pixels[i*2+1]=v>>8;rgba.data[i*4]=Math.round((v>>11)*255/31);rgba.data[i*4+1]=Math.round(((v>>5)&63)*255/63);rgba.data[i*4+2]=Math.round((v&31)*255/31)}ctx.putImageData(rgba,0,0);
 return {version:2,format:'rgb565le',projection:'equirectangular',sourceLabel:'Street · OSM',name:$('mapname').value.trim()||'Driving area',attribution:'© OpenStreetMap contributors (ODbL)',bounds:b,width:w,height:h,pixels,canvas};
}
function imageBlob(m){const metadata={version:2,format:m.format,projection:m.projection,name:m.name,attribution:m.attribution,bounds:m.bounds,width:m.width,height:m.height,sourceLabel:m.sourceLabel};const json=new TextEncoder().encode(JSON.stringify(metadata));if(json.length>2048)throw Error('Map metadata exceeds limit');const header=new Uint8Array(8);header.set([82,49,73,77]);new DataView(header.buffer).setUint32(4,json.length,true);return new Blob([header,json,m.pixels],{type:'application/octet-stream'})}
function downloadBlob(blob,name){const a=document.createElement('a'),url=URL.createObjectURL(blob);a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000)}
function readImagePackage(buffer){const bytes=new Uint8Array(buffer),view=new DataView(buffer);if(bytes.length<8||String.fromCharCode(...bytes.slice(0,4))!=='R1IM')throw Error('Invalid image map');const n=view.getUint32(4,true);if(n>2048||bytes.length<8+n)throw Error('Invalid map header');const m=JSON.parse(new TextDecoder().decode(bytes.slice(8,8+n)));const b=m.bounds;
 if(m.version!==2||m.format!=='rgb565le'||m.projection!=='equirectangular'||!Number.isInteger(m.width)||!Number.isInteger(m.height)||m.width<2||m.height<2||m.width>512||m.height>512||bytes.length!==8+n+m.width*m.height*2||!b||![b.west,b.east,b.south,b.north].every(Number.isFinite)||b.west< -180||b.east>180||b.south< -85||b.north>85||b.east<=b.west||b.north<=b.south||b.east-b.west>.25||b.north-b.south>.25)throw Error('Unsupported image map or bounds');
 m.pixels=bytes.slice(8+n);m.canvas=document.createElement('canvas');m.canvas.width=m.width;m.canvas.height=m.height;const ctx=m.canvas.getContext('2d'),rgba=ctx.createImageData(m.width,m.height);for(let i=0;i<m.width*m.height;i++){const v=m.pixels[i*2]|m.pixels[i*2+1]<<8;rgba.data[i*4]=Math.round((v>>11)*255/31);rgba.data[i*4+1]=Math.round(((v>>5)&63)*255/63);rgba.data[i*4+2]=Math.round((v&31)*255/31);rgba.data[i*4+3]=255}ctx.putImageData(rgba,0,0);return m;
}

let live=false,liveRevision=0,liveEpoch=0,liveBusy=false,liveTimer,liveGeneration=0,lastLiveAt=0,liveActiveUntil=0,pendingLiveInput=null,liveSending=false,remoteHoldZone=null,remoteWake=false,webHold=null,lcdGesture=null;
const lcd=$('livecanvas');
function liveStatus(text){$('livestatus').textContent=text}
function scheduleLive(delay=0){clearTimeout(liveTimer);if(live)liveTimer=setTimeout(pollLive,delay)}
function stopLive(){cancelWebHold();lcdGesture=null;live=false;pendingLiveInput=null;++liveGeneration;clearTimeout(liveTimer);$('livestart').textContent='Connect live screen';liveStatus('Disconnected');}
async function pollLive(){
 if(mapBusy){if(live)liveTimer=setTimeout(pollLive,500);return;}
 if(!live)return;if(document.hidden||liveBusy||webHold){liveTimer=setTimeout(pollLive,500);return}
 const generation=liveGeneration,started=Date.now();liveBusy=true;
 const abort=new AbortController(),timeout=setTimeout(()=>abort.abort(),20000);
 try{const r=await fetch('/api/screen?since='+liveRevision+'&epoch='+liveEpoch,{signal:abort.signal,cache:'no-store'});
  if(r.headers.has('X-R1-Unlocked'))pinLocked=r.headers.get('X-R1-Unlocked')!=='1';if(r.headers.has('X-R1-Control'))setRemoteMode(r.headers.get('X-R1-Control')==='1');if(r.headers.has('X-R1-Wake'))remoteWake=r.headers.get('X-R1-Wake')==='1';if(r.headers.has('X-R1-Hold-Zone')){const z=r.headers.get('X-R1-Hold-Zone').split(',').map(Number);remoteHoldZone=z.length===4&&z.every(Number.isFinite)?z:null}if(!r.ok)throw Error(await r.text());if(generation!==liveGeneration)return;
  if(r.status===202){liveStatus('Preparing LCD view…');return}
  if(r.status!==204){liveActiveUntil=Date.now()+2000;const bytes=await r.arrayBuffer();if(generation!==liveGeneration)return;const v=new DataView(bytes);
   if(bytes.byteLength<20||v.getUint32(0)!==0x52315343)throw Error('Invalid screen response');
   const w=v.getUint16(12,true),h=v.getUint16(14,true),y=v.getUint16(16,true),rows=v.getUint16(18,true);
   if(w<1||h<1||w>480||h>480||!rows||y+rows>h||bytes.byteLength!==20+w*rows*2)throw Error('Invalid screen dimensions');
   if(lcd.width!==w||lcd.height!==h){lcd.width=w;lcd.height=h}
   lcd.style.width=w+"px";
   const ctx=lcd.getContext('2d'),image=ctx.createImageData(w,rows);
   for(let i=0;i<w*rows;i++){const c=v.getUint16(20+i*2,true),j=i*4;image.data[j]=(c>>11)*255/31;image.data[j+1]=((c>>5)&63)*255/63;image.data[j+2]=(c&31)*255/31;image.data[j+3]=255}
   ctx.putImageData(image,0,y);liveRevision=v.getUint32(4,true);liveEpoch=v.getUint32(8,true);lcd.hidden=false;
  }
  lastLiveAt=Date.now();liveStatus(pinLocked?'Live · enter PIN on the keypad below to unlock':webWritable?'Live · CUT · tap to operate':'Live · read-only · transmitter is not CUT');
 }catch(e){if(generation===liveGeneration){stopLive();liveStatus(e.name==='AbortError'?'Live screen transfer timed out after 20 seconds. Check the connection, then reconnect.':'Connection lost: '+e.message+'. Reconnect when the handset is available on Wi-Fi.')}}
 finally{
  clearTimeout(timeout);liveBusy=false;
  if(live&&generation===liveGeneration){
   if(pendingLiveInput){const tap=pendingLiveInput;pendingLiveInput=null;
    if(tap.epoch===liveEpoch&&Date.now()-tap.at<2000&&!document.hidden&&!mapBusy)liveInput(tap.action,tap.x,tap.y);
    else{liveStatus('Screen changed; please tap again');scheduleLive()}
   }else scheduleLive(Math.max(0,(Date.now()<liveActiveUntil?200:1000)-(Date.now()-started)));
  }
 }
}
$('livestart').onclick=()=>{if(live){stopLive();return}live=true;++liveGeneration;liveRevision=liveEpoch=0;lastLiveAt=0;liveActiveUntil=Date.now()+2000;$('livestart').textContent='Disconnect live screen';pollLive()};
async function liveInput(action,x=0,y=0,gesture={}){
 if(action===8&&liveBusy)return;
 if(action===0&&remoteWake)action=7;

 if(pinLocked&&action!==0&&action!==7){liveStatus("Enter your PIN on the mirrored keypad first.");return}
 if(!live||!liveEpoch||Date.now()-lastLiveAt>2000||mapBusy||document.hidden){liveStatus('Wait for a fresh screen before tapping');return}
 if(liveBusy){
  if(liveSending){liveStatus('Applying previous tap…');return}
  if(!pendingLiveInput){pendingLiveInput={action,x,y,epoch:liveEpoch,at:Date.now()};liveStatus('Tap queued…')}
  else liveStatus('Please wait for the queued tap');
  return;
 }
 const generation=liveGeneration;clearTimeout(liveTimer);liveBusy=true;liveSending=true;
 try{
  if(!webWritable&&action!==7){
   liveStatus('Entering CUT for remote control…');
   await request('/api/screen/input',new URLSearchParams({action:9,epoch:liveEpoch}));
   let ready=false;
   for(let attempt=0;attempt<30;++attempt){
    const access=await(await request('/api/access')).json();
    if(access.controlAllowed===true){ready=true;break}
    await new Promise(resolve=>setTimeout(resolve,100));
    if(!live||generation!==liveGeneration||document.hidden)throw Error('Remote control cancelled');
   }
   if(!ready)throw Error('Outputs are not ready for configuration; check the handset safety status');
   if(!live||generation!==liveGeneration||document.hidden)throw Error('Remote control cancelled');
   setRemoteMode(true);
  }
  await request('/api/screen/input',new URLSearchParams({action,x,y,epoch:liveEpoch,...gesture}));if(generation===liveGeneration)liveStatus('Updating…')}
 catch(e){if(generation===liveGeneration)liveStatus(e.message)}
 finally{liveBusy=false;liveSending=false;pendingLiveInput=null;if(live&&generation===liveGeneration){liveActiveUntil=Date.now()+2000;scheduleLive()}}
}
function inHoldZone(x,y,z=remoteHoldZone){return z&&x>=z[0]&&y>=z[1]&&x<z[0]+z[2]&&y<z[1]+z[3]}
async function sendHold(h,action){
 const abort=new AbortController(),timeout=setTimeout(()=>abort.abort(),1200);
 try{const r=await fetch('/api/screen/input',{method:'POST',headers:{'X-R1-Token':token},body:new URLSearchParams({action,x:h.x,y:h.y,epoch:h.epoch,hold:h.id||0}),signal:abort.signal});if(!r.ok)throw Error(await r.text());return await r.text()}
 finally{clearTimeout(timeout)}
}
function cancelWebHold(){const h=webHold;webHold=null;if(!h)return;clearTimeout(h.timer);if(h.id)sendHold(h,5).catch(()=>{});if(live)scheduleLive();}
async function holdPulse(h){
 if(webHold!==h)return;
 if(!live||!webWritable||pinLocked||document.hidden||h.generation!==liveGeneration||Date.now()-h.started>4500){cancelWebHold();return}
 try{await sendHold(h,6);if(webHold!==h)return;h.ready=Date.now()-h.started>=800;liveStatus(h.ready?'Release to enable · neutral and link checks apply':'Keep holding Drive…');h.timer=setTimeout(()=>holdPulse(h),250)}
 catch(e){if(webHold===h){cancelWebHold();liveStatus('Hold cancelled: '+e.message)}}
}
async function startWebHold(x,y){
 if(!live||!webWritable||pinLocked||liveBusy||mapBusy||document.hidden||Date.now()-lastLiveAt>2000){liveStatus('Wait for a fresh screen, then hold Drive');return}
 const h={x,y,epoch:liveEpoch,generation:liveGeneration,id:0,ready:false,zone:remoteHoldZone};webHold=h;clearTimeout(liveTimer);liveStatus('Starting hold…');
 try{h.id=Number(await sendHold(h,3));if(!Number.isInteger(h.id)||h.id<1)throw Error('Invalid hold acknowledgement');if(webHold!==h){sendHold(h,5).catch(()=>{});return}h.started=Date.now();holdPulse(h)}
 catch(e){if(webHold===h){cancelWebHold();liveStatus('Hold cancelled: '+e.message)}}
}
async function finishWebHold(){
 const h=webHold;if(!h)return;
 if(!h.id||!h.ready||document.hidden||h.generation!==liveGeneration){cancelWebHold();liveStatus('Hold cancelled · hold until ready, then release');return}
 webHold=null;clearTimeout(h.timer);liveBusy=true;
 try{liveStatus(await sendHold(h,4));setRemoteMode(false)}
 catch(e){liveStatus('Enable not requested: '+e.message);sendHold(h,5).catch(()=>{})}
 finally{liveBusy=false;if(live)scheduleLive()}
}
async function releaseDriveGesture(g,x,y){
 if(Math.abs(x-g.x)>=24||Math.abs(y-g.y)>=24||!inHoldZone(x,y,g.zone)){cancelWebHold();return}
 if(Date.now()-g.started<=350&&g.epoch===liveEpoch&&g.generation===liveGeneration&&!document.hidden){
  cancelWebHold();await liveInput(0,x,y);return;
 }
 await finishWebHold();
}
lcd.onclick=null;
function lcdPoint(e){const r=lcd.getBoundingClientRect();return [Math.floor((e.clientX-r.left)*lcd.width/r.width),Math.floor((e.clientY-r.top)*lcd.height/r.height)]}
lcd.onpointerdown=e=>{if(e.button!==0||lcdGesture)return;e.preventDefault();const [x,y]=lcdPoint(e);lcdGesture={pointer:e.pointerId,x,y,started:Date.now(),epoch:liveEpoch,generation:liveGeneration,zone:remoteHoldZone,arm:webWritable&&!remoteWake&&!!inHoldZone(x,y)};lcd.setPointerCapture(e.pointerId);if(lcdGesture.arm)startWebHold(x,y)};
lcd.onpointermove=e=>{if(!lcdGesture||lcdGesture.pointer!==e.pointerId)return;const [x,y]=lcdPoint(e);if(lcdGesture.arm&&(!inHoldZone(x,y,webHold?.zone)||Math.abs(x-lcdGesture.x)>24||Math.abs(y-lcdGesture.y)>24)){lcdGesture.cancelled=true;cancelWebHold()}};
lcd.onpointerup=e=>{if(!lcdGesture||lcdGesture.pointer!==e.pointerId)return;const g=lcdGesture;lcdGesture=null;if(g.cancelled)return;if(g.arm)releaseDriveGesture(g,...lcdPoint(e));else{const [x,y]=lcdPoint(e);if(Math.abs(x-g.x)<24&&Math.abs(y-g.y)<24)liveInput(0,x,y);else if(g.epoch===liveEpoch&&g.generation===liveGeneration&&!remoteWake&&Date.now()-g.started<=1200)liveInput(8,g.x,g.y,{endX:x,endY:y,duration:Date.now()-g.started})}};
lcd.onpointercancel=()=>{lcdGesture=null;cancelWebHold()};
window.addEventListener('blur',()=>{lcdGesture=null;cancelWebHold()});
document.addEventListener('visibilitychange',()=>{if(document.hidden){lcdGesture=null;cancelWebHold()}});
$('livehome').onclick=()=>liveInput(1);$('livesetup').onclick=()=>liveInput(2);
let accessBusy=false;
const accessTimer=setInterval(async()=>{if(document.hidden||live||accessBusy||mapBusy)return;accessBusy=true;try{await request('/api/access')}catch(e){setRemoteMode(false);notify('Handset unavailable: '+e.message)}finally{accessBusy=false}},2000);
window.addEventListener('pagehide',()=>{stopLive();clearInterval(accessTimer)});

Promise.all([load(),loadControls()]).then(()=>notify(webWritable?'Connected · remote controls available while CUT.':'Connected · read-only until transmitter is CUT.')).catch(e=>notify('Could not connect: '+e.message));
</script></html>)HTML";
