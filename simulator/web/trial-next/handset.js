// Shared handset settings for the browser demonstration. No hardware calls.
const handsetDefaults={scheme:'light',brightness:100,timeout:60,idle:'Logo saver',buzzer:true,clicks:true,pitch:3200,duration:10,txCells:2,txLow:6.6};
let handset={...handsetDefaults};
try{const s=JSON.parse(localStorage.getItem('r1-v2-handset')||'{}');for(const k of Object.keys(handsetDefaults))if(typeof s?.[k]===typeof handsetDefaults[k])handset[k]=s[k];}catch{}
let setupTab='model';
const handsetSchemes={
 light:{name:'Light',bg:'#f3f5f1',surface:'#ffffff',selection:'#e0e9df',text:'#213a3c',muted:'#60776d',line:'#d0ddd3',accent:'#007e6c'},
 graphite:{name:'Graphite / teal',bg:'#151b20',surface:'#202a30',selection:'#28433f',text:'#edf3f2',muted:'#aab7bd',line:'#3b4a52',accent:'#58d2c3'},
 midnight:{name:'Midnight / ice blue',bg:'#111827',surface:'#1d2a3b',selection:'#293e56',text:'#eef5ff',muted:'#b1bfd0',line:'#3c4e64',accent:'#82c5ff'},
 charcoal:{name:'Warm charcoal / ivory',bg:'#1c1a18',surface:'#2c2925',selection:'#433c30',text:'#f3eee6',muted:'#bfb6a9',line:'#4e4840',accent:'#e1cfab'},
 slate:{name:'Slate / soft lilac',bg:'#1b1b26',surface:'#292a39',selection:'#413a55',text:'#f0eef8',muted:'#b9b7ca',line:'#48495f',accent:'#c4b1f6'}
};
if(!handsetSchemes[handset.scheme])handset.scheme='light';
handset.brightness=Math.max(20,Math.min(100,handset.brightness));
function setupTabs(){return `<div class="tabs">${btn('Model','setup-model',setupTab==='model'?'chosen':'')}${btn('Handset','setup-handset',setupTab==='handset'?'chosen':'')}</div>`;}
function handsetMenu(){return title('Setup','Shared handset settings',false)+setupTabs()+`<div class="menu-grid handset-menu">
 <button data-go="handset-display">Display<small>Colour scheme · brightness · idle</small></button>
 <button data-go="handset-sounds">Sounds / alerts<small>Buzzer · key clicks · pitch</small></button>
 <button data-go="handset">RF module<small>Shared module settings</small></button>
 <button data-go="handset-tx">TX battery<small>Cell count · low voltage warning</small></button>
 <button data-go="handset-network">Network / web<small>Home Wi-Fi · web access</small></button>
 <button data-go="handset-system">System<small>Inputs · firmware · PIN · restart</small></button></div>`;}
function handsetOptions(values,selected){return values.map(v=>`<option value="${esc(v)}" ${v===selected?'selected':''}>${esc(v)}</option>`).join('');}
function handsetActions(note='Browser demonstration · shared across all models.'){return `<p class="help">${note}</p><div class="bottom-actions">${btn('Cancel','back')}${btn('Apply to preview','save-handset','primary')}</div>`;}
function handsetDisplay(){return title('Display','Shared')+`<form id="handset-form" data-section="display">
 <div class="row"><label for="handset-scheme">Colour scheme</label><select id="handset-scheme" name="scheme">${Object.entries(handsetSchemes).map(([key,s])=>`<option value="${key}" ${handset.scheme===key?'selected':''}>${s.name}</option>`).join('')}</select></div>
 <div class="row"><label for="handset-brightness">Brightness</label><div><input id="handset-brightness" name="brightness" type="number" min="20" max="100" step="5" value="${handset.brightness}"> %</div></div>
 <div class="row"><label for="handset-timeout">Screen timeout</label><select id="handset-timeout" name="timeout">${[0,30,60,120,300].map(n=>`<option value="${n}" ${handset.timeout===n?'selected':''}>${n?n+' sec':'Never'}</option>`).join('')}</select></div>
 <div class="row"><label for="handset-idle">Idle action</label><select id="handset-idle" name="idle">${handsetOptions(['Logo saver','Display off'],handset.idle)}</select></div>
 ${handsetActions('Palette/brightness preview. Idle behavior runs on the R1.')}</form>`;}
function handsetSounds(){return title('Sounds / alerts','Shared')+`<form id="handset-form" data-section="sounds">
 <div class="row"><label for="handset-buzzer">Buzzer</label><input id="handset-buzzer" name="buzzer" type="checkbox" ${handset.buzzer?'checked':''}></div>
 <div class="row"><label for="handset-clicks">Key clicks</label><input id="handset-clicks" name="clicks" type="checkbox" ${handset.clicks?'checked':''}></div>
 <div class="row"><label for="handset-pitch">Pitch · Hz</label><input id="handset-pitch" name="pitch" type="number" min="200" max="6000" step="100" value="${handset.pitch}"></div>
 <div class="row"><label for="handset-duration">Click length · ms</label><input id="handset-duration" name="duration" type="number" min="5" max="100" step="5" value="${handset.duration}"></div>
 <div class="bottom-actions">${btn('Test sound','test-handset-sound')}${btn('Apply to preview','save-handset','primary')}</div><p class="help">Sound plays on this computer only.</p></form>`;}
function handsetTx(){return title('TX battery','Shared · local draft')+`<form id="handset-form" data-section="tx"><div class="row"><label for="handset-cells">TX cell count</label><input id="handset-cells" name="txCells" type="number" min="1" max="6" value="${handset.txCells}"></div><div class="row"><label for="handset-tx-low">Low voltage warning · V</label><input id="handset-tx-low" name="txLow" type="number" min="1" max="30" step=".1" value="${handset.txLow}"></div><p class="help">Actual transmitter voltage sensing and warning configuration remain on the R1. This page stores a browser draft.</p>${handsetActions()}</form>`;}
function handsetNetwork(){return title('Network / web','Shared')+`<div class="menu-grid"><button data-go="handset-wifi">Home Wi-Fi<small>Connection</small></button><button data-go="handset-web">Web access<small>Address / credentials</small></button></div><p class="help">The native R1 retains both network pages. This trial does not connect to your radio or configure Wi-Fi.</p>`;}
function handsetSystem(){return title('System','Shared')+`<div class="menu-grid"><button data-go="handset-inputs">Input diagnostics<small>Bench monitor</small></button><button data-go="handset-info">Firmware information<small>Version / hardware</small></button><button data-go="handset-pin">PIN<small>Change / recovery</small></button><button data-go="handset-restart">Restart handset<small>R1 operation</small></button></div><p class="help">Every entry is retained on the actual R1.</p>`;}
function handsetInfo(name,text){return title(name,'Live R1 required')+`<p class="help" style="font-size:12px;margin-top:18px">${text}</p><div class="row"><span>Available on the R1</span><b>Setup → Handset</b></div><p class="help">Browser preview only. No hardware or credentials are accessed.</p>`;}
const handsetViews={
 'handset-display':handsetDisplay,'handset-sounds':handsetSounds,'handset-tx':handsetTx,'handset-network':handsetNetwork,'handset-system':handsetSystem,
 'handset-wifi':()=>handsetInfo('Home Wi-Fi','Configure and connect the handset to your home network using the native R1 page.'),
 'handset-web':()=>handsetInfo('Web access','The native page shows the handset’s web address and access credentials. These are not copied into this trial.'),
 'handset-pin':()=>handsetInfo('PIN / recovery','Change the handset PIN or use its recovery flow directly on the R1. This trial does not read or change authentication.'),
 'handset-restart':()=>handsetInfo('Restart handset','Restart is retained in the R1 System menu. No restart command is sent by the browser trial.'),
 'handset-info':()=>title('Firmware information','Browser trial')+`<div class="row"><span>UI development</span><b>V2</b></div><div class="row"><span>Native display</span><b>480 × 320 landscape</b></div><div class="row"><span>Hardware</span><b>XIAO / Waveshare LCD F</b></div><p class="help">See System → Firmware information on the R1 for its actual installed commit and diagnostics.</p>`,
 'handset-inputs':()=>title('Input diagnostics','Simulated bench')+`<div class="row"><span>Steering</span><b data-bench="steer">${steer}%</b></div><div class="row"><span>Throttle / brake</span><b data-bench="throttle">${throttle}%</b></div><p class="help">Move the test-bench sliders. The native R1 monitor retains its complete physical input diagnostics.</p>`
};
function applyHandsetDisplay(){const s=handsetSchemes[handset.scheme],screen=document.querySelector('#screen');if(!screen)return;screen.dataset.theme=handset.scheme==='light'?'day':'night';for(const k of ['bg','surface','selection','text','muted','line','accent'])screen.style.setProperty('--ui-'+k,s[k]);screen.style.filter=`brightness(${handset.brightness/100})`;}
function playHandsetTone(pitch,duration){if(!handset.buzzer)return;try{const Audio=window.AudioContext||window.webkitAudioContext,ac=new Audio(),o=ac.createOscillator(),g=ac.createGain();o.connect(g);g.connect(ac.destination);o.frequency.value=pitch;g.gain.value=.04;o.start();o.stop(ac.currentTime+duration/1000);o.onended=()=>ac.close();}catch{}}
function updateHandsetBench(){document.querySelectorAll('[data-bench]').forEach(el=>el.textContent=(el.dataset.bench==='steer'?steer:throttle)+'%');}
document.addEventListener('click',e=>{const a=e.target.closest('[data-action]')?.dataset.action;
 if(a==='setup-model'||a==='setup-handset'){setupTab=a==='setup-model'?'model':'handset';page='setup';render();}
 if(a==='test-handset-sound'){const f=document.querySelector('#handset-form');if(f.reportValidity())playHandsetTone(+f.elements.pitch.value,+f.elements.duration.value);}
 if(a==='save-handset'){const form=document.querySelector('#handset-form');if(!form.reportValidity())return;const f=new FormData(form),section=form.dataset.section;
  if(section==='display'){handset.scheme=f.get('scheme');handset.brightness=+f.get('brightness');handset.timeout=+f.get('timeout');handset.idle=f.get('idle');}
  if(section==='sounds'){handset.buzzer=f.has('buzzer');handset.clicks=f.has('clicks');handset.pitch=+f.get('pitch');handset.duration=+f.get('duration');}
  if(section==='tx'){handset.txCells=+f.get('txCells');handset.txLow=+f.get('txLow');}
  try{localStorage.setItem('r1-v2-handset',JSON.stringify(handset));}catch{}
  applyHandsetDisplay();back();toast('Handset preview settings saved');
 }
});
