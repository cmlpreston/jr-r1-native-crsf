const fs=require('fs'),vm=require('vm'),assert=require('assert/strict');
const source=fs.readFileSync(__dirname+'/../include/setup_portal_page.h','utf8');
const elements={};const $=id=>elements[id]??={value:'',checked:false,textContent:''};
const state={slot:1,mode:2,duration:300,minimumLap:3,target:10,autoStart:true,raceAlarm:false,onSeconds:123,elapsedMs:6000,running:false,ready:false,bestMs:3036,averageMs:4000,totalMs:8000,laps:[3036,4964]};
let posted=null,accept=false,blob,clicked=false;
const ctx={$,URLSearchParams,Blob,setTimeout:fn=>fn(),confirm:()=>accept,notify:()=>{},
 URL:{createObjectURL:b=>{blob=b;return'blob:test'},revokeObjectURL:()=>{}},
 document:{createElement:()=>({click(){clicked=true}})},request:async(url,body)=>{assert.equal(url,'/api/timer');if(body)posted=Object.fromEntries(body);return{json:async()=>state}}};
vm.createContext(ctx);
vm.runInContext(source.split('\n').filter(l=>/^(let timerSnapshot|async function loadTimer|\$\('timer(?:refresh|form|clear|export)'\))/.test(l)).join('\n'),ctx);
(async()=>{
 await ctx.loadTimer();assert.equal($('timerduration').value,300);assert.equal($('timerminimum').value,3);
 assert.match($('timerlaps').textContent,/1\. 3\.04 s/);
 await $('timerform').onsubmit({preventDefault(){}});assert.equal(posted.slot,'1');assert.equal(posted.duration,'300');assert.equal(posted.minimumLap,'3');assert.equal(posted.autoStart,'1');assert.equal(posted.raceAlarm,'0');
 posted=null;await $('timerclear').onclick();assert.equal(posted,null);accept=true;await $('timerclear').onclick();assert.equal(posted.confirm,'clear-laps');assert.equal(posted.action,'5');
 $('timerexport').onclick();assert(clicked);assert.equal(await blob.text(),'Lap,Milliseconds,Seconds\n1,3036,3.036\n2,4964,4.964');
 console.log('Timer web: seconds/settings mapping, clear confirmation and exact lap CSV PASS');
})().catch(e=>{console.error(e);process.exitCode=1});
