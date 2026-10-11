const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(__dirname+'/../include/setup_portal_page.h','utf8');
const elements={};const $=id=>elements[id]??=( {value:'',checked:false,disabled:false,hidden:false} );
let posted;
const controls=Array.from({length:19},(_,id)=>({id,name:`Control ${id}`,kind:id<2?0:id===4?3:2,target:id===4?2:4,selection:id===4?0x8300:3,behaviour:id===4?5:3,context:1,enabled:true,minimum:0,maximum:600,step:50,initial:0,failsafe:0,inverted:false}));
const ctx={$,catalogue:{controls,functions:[{id:0,name:'Timer'}]},selectedControl:9,behaviourNames:Array(9).fill('mode'),document:{querySelectorAll:()=>[]},optionList:(el,options,selected)=>{el.value=String(options.some(o=>o[0]===selected)?selected:options[0]?.[0]??'')},URLSearchParams,notify:()=>{},loadControls:async()=>{},request:async(url,body)=>{posted=Object.fromEntries(body);return{text:async()=> 'Saved'}}};
vm.createContext(ctx);
vm.runInContext(source.split('\n').filter(l=>/^(function selectControl|function updateBindingOptions|function setBindingValues|\$\('target'\)\.onchange|\$\('binding'\)\.onsubmit)/.test(l)).join('\n'),ctx);
(async()=>{
 ctx.selectControl(9);assert.equal($('bindingvalues').hidden,false);assert.equal($('bindingmaximum').value,60);
 await $('binding').onsubmit({preventDefault(){}});
 assert.equal(posted.minimum,'0');assert.equal(posted.maximum,'600');assert.equal(posted.step,'50');assert.equal(posted.failsafe,'0');
 $('bindingmaximum').value=42.5;$('bindinginverted').checked=true;await $('binding').onsubmit({preventDefault(){}});
 assert.equal(posted.maximum,'425');assert.equal(posted.inverted,'1');
 ctx.selectControl(0);assert.equal($('bindingvalues').hidden,true);await $('binding').onsubmit({preventDefault(){}});assert.equal(posted.minimum,undefined);
 ctx.selectControl(4);assert.equal($('bindinginitial').disabled,true);assert.equal($('bindingstep').disabled,false);assert.equal($('bindingmaximum').max,20);
 $('target').value='1';$('target').onchange();assert.equal($('bindingvalues').hidden,true);
 console.log('Assignment web editor: percentage conversion, loaded ranges, reversal, primary/function hiding and trim retention PASS');
})().catch(e=>{console.error(e);process.exitCode=1});
