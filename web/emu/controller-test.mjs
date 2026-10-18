// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {connectController,connectPanel} from './controller.js';
globalThis.Option=class {constructor(text,value){this.textContent=text;this.value=value;}};
globalThis.document={createElement:()=>({})};
const select=()=>({value:'',options:[],replaceChildren(...options){this.options=options;this.value=options[0]?.value||'';}});
const sent=[],requests=[],messages=[],input={id:'in',name:'FM1',state:'connected',onmidimessage:null},output={id:'out',name:'FM1',state:'connected',send:d=>sent.push(d)};
const access={inputs:new Map([['in',input]]),outputs:new Map([['out',output]])},navigator={requestMIDIAccess:async opts=>{requests.push(opts);return access;}},status={textContent:''};
let stop=await connectController({navigator,select:select(),status,send:m=>messages.push(m)});
assert.equal(requests[0],undefined,'ordinary MIDI connection must not request SysEx');
input.onmidimessage({data:Uint8Array.of(144,60,100)});assert.deepEqual(messages.at(-1),{type:'midi',data:[144,60,100]});
assert.equal(sent.length,0);stop();assert.equal(input.onmidimessage,null);
stop=await connectPanel({navigator,inputSelect:select(),outputSelect:select(),status,send:m=>messages.push(m)});
assert.deepEqual(requests.at(-1),{sysex:true});assert.deepEqual(sent.at(-1),[240,125,70,76,110,1,247]);
const frame=Uint8Array.of(240,125,70,76,111,1,5,1,1,2,3,4,65,64,63,64,66,64,64,60,5,247);
const before=messages.length;input.onmidimessage({data:frame.subarray(0,20)});assert.equal(messages.length,before,'malformed frame is ignored');
input.onmidimessage({data:frame});
assert.deepEqual(messages.slice(before),[{type:'buttons',mask:133},{type:'keys',mask:1|2<<7|3<<14|4<<21},{type:'enc',role:0,n:1},{type:'enc',role:2,n:-1},{type:'enc',role:4,n:2},{type:'master',value:700}]);
assert.match(status.textContent,/keys, knobs and buttons/);stop();assert.deepEqual(sent.at(-1),[240,125,70,76,110,0,247]);assert.equal(input.onmidimessage,null);assert.deepEqual(messages.slice(-2),[{type:'buttons',mask:0},{type:'keys',mask:0}]);
console.log('MIDI input and FM1 panel: permissions, frame validation, encoder/button/key routing and release passed.');
// Opening a selected MIDI port must not repeatedly disable/re-enable its lease.
const realInterval=globalThis.setInterval,realClear=globalThis.clearInterval,realNow=Date.now;
let tick,now=10000;globalThis.setInterval=fn=>{tick=fn;return 1;};globalThis.clearInterval=()=>{};Date.now=()=>now;
try{
 stop=await connectPanel({navigator,inputSelect:select(),outputSelect:select(),status,send:m=>messages.push(m)});
 input.onmidimessage({data:frame});
 const commands=sent.length,events=messages.length;access.onstatechange();
 assert.equal(sent.length,commands,'port-open statechange must preserve current lease');assert.equal(messages.length,events,'unchanged ports must not release held controls');
 now+=1100;tick();assert.match(status.textContent,/Reconnecting/);assert.deepEqual(messages.slice(-2),[{type:'buttons',mask:0},{type:'keys',mask:0}]);
 input.onmidimessage({data:Uint8Array.of(240,125,70,76,110,0,1,247)});assert.match(status.textContent,/panel connected/);
 now+=2600;tick();assert.match(status.textContent,/connection lost/);assert.equal(input.onmidimessage,null);
 stop();
}finally{Date.now=realNow;globalThis.setInterval=realInterval;globalThis.clearInterval=realClear;}
console.log('Panel lease: unchanged-port events preserve control, renewal ACKs confirm liveness, lost responses clear stale connected status.');
