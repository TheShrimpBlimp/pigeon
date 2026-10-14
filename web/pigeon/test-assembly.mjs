import assert from 'node:assert/strict';
import {assemble,trimmedSegments} from './audio-assembly.js';
const a=Float32Array.from({length:2205},(_,i)=>i/2205),b=new Float32Array(4410).fill(-.5);
const clips=[{pcm:a,start:0,end:.1,name:'A',filename:'A.wav',fadeMs:0},{pcm:b,start:.1,end:.2,name:'B',filename:'B.wav',fadeMs:0}];
const joined=assemble(clips);assert.equal(joined.pcm.length,4410);assert.deepEqual(joined.pcm.subarray(0,2205),a);assert(joined.pcm.subarray(2205).every(v=>v===-.5));assert.deepEqual(joined.segments.map(c=>[c.a,c.b]),[[0,2205],[2205,4410]]);
assert.deepEqual(trimmedSegments(joined.segments,1102,3307).map(c=>[c.a,c.b]),[[0,1103],[1103,2205]]);
assert.throws(()=>assemble([{...clips[0],start:-1}]));assert.throws(()=>assemble([{...clips[0],end:.5}]));assert.throws(()=>assemble(Array(33).fill(clips[0])));assert.throws(()=>assemble([]));
console.log('Audio assembly: trimmed PCM, order, exact chop boundaries, crop mapping and invalid selections passed.');

const faded=assemble([{...clips[1],start:0,end:.2,gainDb:-6,fadeMs:3}]).pcm;assert(faded[0]===0);assert(faded.at(-1)===0);assert(Math.abs(faded[100]+.5*10**(-6/20))<1e-6);assert.throws(()=>assemble([{...clips[0],gainDb:13}]));
