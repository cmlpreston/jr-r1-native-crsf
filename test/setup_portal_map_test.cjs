// Run the real embedded browser geometry code without DOM/network dependencies.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(__dirname+'/../include/setup_portal_page.h','utf8');
const scope={$:()=>({value:'Test track'})};vm.createContext(scope);
for(const name of ['world','unworld','clipPaths','simplifyLine','vectorPackage','validate']){
 const start=source.indexOf('function '+name+'('),next=source.indexOf('\n',start);
 assert.ok(start>=0);vm.runInContext(source.slice(start,next),scope);
}
const bounds={west:0,east:0.01,south:0,north:0.01};
let paths=scope.clipPaths([[-1,.005],[1,.005]],bounds);
assert.equal(paths.length,1);assert.ok(Math.abs(paths[0][0][0])<1e-10);assert.ok(Math.abs(paths[0][1][0]-.01)<1e-10);
assert.equal(scope.clipPaths([[-1,-1],[-2,-2]],bounds).length,0);
// Disconnected clipped runs must not be joined by a spurious line.
assert.equal(scope.clipPaths([[.002,.002],[.02,.002],[.02,.008],[.002,.008]],bounds).length,2);
for(const pair of [[144.96,-37.81],[-73.9,40.7],[0,0]]){
 const w=scope.world(...pair,17),p=scope.unworld(...w,17);assert.ok(Math.abs(p[0]-pair[0])<1e-7&&Math.abs(p[1]-pair[1])<1e-7);
}
const geometry=Array.from({length:900},(_,i)=>({lon:.001+i*.000009,lat:.005+Math.sin(i/15)*.00001}));
const pkg=scope.vectorPackage({elements:[{geometry}]},bounds);
assert.ok(pkg.paths.flat().length<=512);assert.ok(pkg.attribution.includes('OpenStreetMap'));
assert.throws(()=>scope.vectorPackage({remark:'runtime timeout',elements:[]},bounds),/did not complete/);
assert.throws(()=>scope.vectorPackage({elements:[]},bounds),/No mapped/);
assert.throws(()=>scope.vectorPackage({elements:Array.from({length:33},()=>({geometry:[{lon:.001,lat:.002},{lon:.003,lat:.004}]}))},bounds),/32-path/);
assert.throws(()=>scope.validate({version:1,paths:[[[0,0],[1,1]]]}),/local area/);
console.log('setup portal map geometry: PASS');

// Binary map round-trip, exact pixel bytes and malformed input rejection.
Object.assign(scope,{TextEncoder,TextDecoder,Uint8Array,DataView,Blob,
 document:{createElement:()=>({getContext:()=>({createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:()=>{}})})}});
vm.runInContext(source.slice(source.indexOf('function imageBlob('),source.indexOf('\nlet live=')),scope);
(async()=>{
 const m={version:2,format:'rgb565le',projection:'equirectangular',name:'Test',attribution:'OSM',bounds,width:2,height:2,pixels:new Uint8Array([0,248,224,7,31,0,255,255])};
 const bytes=await scope.imageBlob(m).arrayBuffer();
 const decoded=scope.readImagePackage(bytes);
 assert.deepEqual(Array.from(decoded.pixels),Array.from(m.pixels));
 assert.throws(()=>scope.readImagePackage(bytes.slice(0,-1)),/Unsupported/);
 const corrupt=bytes.slice(0);new DataView(corrupt).setUint32(4,2049,true);
 assert.throws(()=>scope.readImagePackage(corrupt),/header/);
 console.log('setup portal image package: PASS');
})().catch(e=>{console.error(e);process.exitCode=1});

// Save feedback must distinguish a committed save, rejection and unknown timeout.
(async()=>{
 let xhr,notice='',mode;
 const uploadScope={token:'test-token',mapStatus:t=>notice=t,setRemoteMode:v=>mode=v,
 XMLHttpRequest:class {constructor(){xhr=this;this.upload={}}open(){}setRequestHeader(){}send(){}getResponseHeader(){return '1'}}};
 vm.createContext(uploadScope);
 const start=source.indexOf('function uploadMap(');
 vm.runInContext(source.slice(start,source.indexOf('\n',start)),uploadScope);
 let pending=uploadScope.uploadMap({});
 xhr.upload.onprogress({lengthComputable:true,loaded:50,total:100});assert.match(notice,/50%/);
 xhr.upload.onload();assert.match(notice,/Waiting/);
 xhr.status=200;xhr.responseText='Map saved';xhr.onload();assert.equal(await pending,'Map saved');assert.equal(mode,true);
 pending=uploadScope.uploadMap({});xhr.status=400;xhr.responseText='Storage not ready';xhr.onload();await assert.rejects(pending,/Storage not ready/);
 pending=uploadScope.uploadMap({});xhr.ontimeout();await assert.rejects(pending,/result is unknown/);
 pending=uploadScope.uploadMap({});xhr.onerror();await assert.rejects(pending,/Connection lost/);
 console.log('setup portal upload feedback: PASS');
})().catch(e=>{console.error(e);process.exitCode=1});

// Satellite export bounds may be expanded by the service to preserve aspect ratio.
// Crop back to the selected bounds, otherwise GPS traces drift on the image.
for(const name of ['satelliteCrop','mapTileUrl']){
 const start=source.indexOf('function '+name+'(');
 vm.runInContext(source.slice(start,source.indexOf('\n',start)),scope);
}
vm.runInContext("const satelliteService='https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer'",scope);
assert.equal(scope.mapTileUrl('satellite',3,2,5),'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/3/5/2');
assert.equal(scope.mapTileUrl('street',3,-1,5),'https://tile.openstreetmap.org/3/7/5.png');
const extent={xmin:-1,xmax:3,ymin:-2,ymax:2,spatialReference:{wkid:4326}};
assert.deepEqual(Array.from(scope.satelliteCrop(extent,{west:0,east:2,south:-1,north:1},400,400)),[100,100,200,200]);
assert.throws(()=>scope.satelliteCrop({...extent,spatialReference:{wkid:3857}},bounds,512,256),/invalid geographic/);
assert.throws(()=>scope.satelliteCrop({...extent,xmin:1},bounds,512,256),/invalid geographic/);
assert.throws(()=>scope.satelliteCrop({...extent,xmax:NaN},bounds,512,256),/invalid geographic/);
// Check the complete embedded script parses, not only extracted helper functions.
new vm.Script(source.split('<script>')[1].split('</script>')[0]);
console.log('setup portal satellite geometry and script: PASS');


// Square selection uses the same dimension for the visible frame and bounds.
{
 const squareScope={mapView:{lon:145.08567,lat:-37.83633,z:17}};
 vm.createContext(squareScope);
 for(const name of ['world','unworld','selectionSize','selectionBounds']){
  const start=source.indexOf('function '+name+'(');
  vm.runInContext(source.slice(start,source.indexOf('\n',start)),squareScope);
 }
 for(const [clientWidth,clientHeight] of [[810,360],[320,300],[280,360]]){
  squareScope.$=()=>({clientWidth,clientHeight});
  const b=squareScope.selectionBounds();
  const nw=squareScope.world(b.west,b.north,17),se=squareScope.world(b.east,b.south,17);
  assert.ok(Math.abs((se[0]-nw[0])-(se[1]-nw[1]))<1e-7);
  assert.equal(squareScope.selectionSize(),Math.min(clientWidth,clientHeight)*.8);
 }
 console.log('square map selection: PASS');
}

// Refresh a stale boot token without reloading/discarding the prepared map.
(async()=>{
 let replies=[],uploads=[],fresh={token:'new-boot-token',storage:true,controlAllowed:true};
 const prepared={pixels:'keep these bytes'},originalState={unsaved:'keep edits'};
 const s={token:'old-boot-token',map:prepared,state:originalState,mapStatus:()=>{},setRemoteMode:()=>{},
  request:async()=>({json:async()=>fresh}),
  uploadMap:async data=>{uploads.push({data,token:s.token});const r=replies.shift();if(r)throw r;return 'saved'}};
 vm.createContext(s);
 for(const name of ['refreshMapSession','saveMapWithSession']){
  const at=source.indexOf('async function '+name+'(');vm.runInContext(source.slice(at,source.indexOf('\n',at)),s);
 }
 const data={prepared:true};assert.equal(await s.saveMapWithSession(data),'saved');
 assert.equal(uploads[0].token,'new-boot-token');assert.equal(uploads[0].data,data);
 assert.equal(s.map,prepared);assert.equal(s.state,originalState);
 uploads=[];replies=[{sessionExpired:true}];await s.saveMapWithSession(data);assert.equal(uploads.length,2);
 uploads=[];replies=[{sessionExpired:true},{sessionExpired:true}];await assert.rejects(s.saveMapWithSession(data));assert.equal(uploads.length,2);
 uploads=[];replies=[Error('No confirmation: result is unknown')];await assert.rejects(s.saveMapWithSession(data),/unknown/);assert.equal(uploads.length,1);
 uploads=[];fresh.controlAllowed=false;await assert.rejects(s.saveMapWithSession(data),/CUT/);assert.equal(uploads.length,0);
 fresh.controlAllowed=true;fresh.storage=false;await assert.rejects(s.saveMapWithSession(data),/storage/);
 fresh.storage=true;fresh.token='';await assert.rejects(s.saveMapWithSession(data),/refresh/);
 console.log('map session renewal, bounded retry and draft retention: PASS');
})().catch(e=>{console.error(e);process.exitCode=1});

// Reset a configured gyro draft without submitting or altering channel response.
{
 const elements={};
 for(const key of ['includeBase','constantPercent','outputMinimum','outputMaximum','cutOverride','cutPercent','rateEnabled','rateControl','lowNegative','lowPositive','highNegative','highPositive','transitionMs'])elements[key]={value:60};
 for(let i=0;i<2;i++)for(const key of ['enabled','source','weight','negativeWeight','sourceMode','thresholdPercent','hysteresisPercent','followTrim','replace','condition','control'])elements['mix'+i+key]={value:1};
 elements.endpoint={value:85};
 const status={textContent:''};let previews=0;
 const context={$:id=>id==='mixsettings'?{elements}:status,clearTimeout:()=>{},mixPreviewTimer:0,previewMixing:()=>previews++};
 vm.createContext(context);
 vm.runInContext(source.slice(source.indexOf('function resetMixingDraft(){'),source.indexOf("$('resetmixing').onclick=")),context);
 context.resetMixingDraft();
 assert.equal(elements.includeBase.value,1);
 assert.equal(elements.constantPercent.value,0);
 assert.equal(elements.outputMinimum.value,-100);
 assert.equal(elements.outputMaximum.value,100);
 for(const key of ['cutOverride','rateEnabled','mix0enabled','mix1enabled'])assert.equal(elements[key].value,0);
 for(let i=0;i<2;i++){
  assert.equal(elements['mix'+i+'sourceMode'].value,0);
  assert.equal(elements['mix'+i+'thresholdPercent'].value,0);
  assert.equal(elements['mix'+i+'hysteresisPercent'].value,5);
 }
 assert.equal(elements.endpoint.value,85);
 assert.equal(previews,1);
 assert.match(status.textContent,/draft.*Save/);
 console.log('mix reset restores ordinary input and removes gyro limits without saving: PASS');
}
