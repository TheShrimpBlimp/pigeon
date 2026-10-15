import assert from 'node:assert/strict';
import {assemble,trimmedSegments,mergedChops} from './audio-assembly.js';
const a=Float32Array.from({length:2205},(_,i)=>i/2205),b=new Float32Array(4410).fill(-.5);
const clips=[{pcm:a,start:0,end:.1,name:'A',filename:'A.wav',fadeMs:0},{pcm:b,start:.1,end:.2,name:'B',filename:'B.wav',fadeMs:0}];
const joined=assemble(clips);assert.equal(joined.pcm.length,4410);assert.deepEqual(joined.pcm.subarray(0,2205),a);assert(joined.pcm.subarray(2205).every(v=>v===-.5));assert.deepEqual(joined.segments.map(c=>[c.a,c.b]),[[0,2205],[2205,4410]]);
assert.deepEqual(trimmedSegments(joined.segments,1102,3307).map(c=>[c.a,c.b]),[[0,1103],[1103,2205]]);
assert.throws(()=>assemble([{...clips[0],start:-1}]));assert.throws(()=>assemble([{...clips[0],end:.5}]));assert.throws(()=>assemble(Array(33).fill(clips[0])));assert.throws(()=>assemble([]));
console.log('Audio assembly: trimmed PCM, order, exact chop boundaries, crop mapping and invalid selections passed.');

const faded=assemble([{...clips[1],start:0,end:.2,gainDb:-6,fadeMs:3}]).pcm;assert(faded[0]===0);assert(faded.at(-1)===0);assert(Math.abs(faded[100]+.5*10**(-6/20))<1e-6);assert.throws(()=>assemble([{...clips[0],gainDb:13}]));

const layout=mergedChops(joined.segments,[{a:0,b:4410,pitch:0,flags:0}]);
assert.equal(layout.length,32);
assert.deepEqual(layout.slice(0,2),joined.segments,'whole sections must stay on keys 1 and 2');
assert(layout.slice(2).every(c=>c.b-c.a>=64 && c.a>=0 && c.b<=4410));
assert(layout.slice(2).every(c=>c.b<=2205 || c.a>=2205),'automatic chops cannot cross source sections');
assert.deepEqual(joined.segments.map(c=>[c.a,c.b]),[[0,2205],[2205,4410]],'inputs stay unchanged');
const many=Array.from({length:32},(_,i)=>({a:i*100,b:(i+1)*100,pitch:0,flags:0}));
assert.deepEqual(mergedChops(many,[{a:0,b:3200}]),many);
const tiny=mergedChops([{a:0,b:64,pitch:0,flags:0}],[{a:0,b:64}]);assert.equal(tiny.length,2);assert(tiny.every(c=>c.b-c.a>=64));
console.log('Merged layout: full sections first, 32 playable chops, section boundaries and short audio passed.');
