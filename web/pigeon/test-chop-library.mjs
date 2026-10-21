// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {compactChops,assembleCollected,collectedHit,exportCollection,importCollection} from './chop-library.js';
const pcm=Float32Array.from({length:1000},(_,i)=>i/2000),c={a:100,b:300,pitch:0,flags:4096,play:32768,reverse:1,cents:25,gain:512,xfade:4};
const packed=compactChops(pcm,[c,{...c,a:200,b:400},{...c,a:700,b:800}]);
assert.equal(packed.pcm.length,400,'overlapping regions must share audio and gaps must be discarded');
assert.deepEqual(packed.chops.map(c=>[c.a,c.b]),[[0,200],[100,300],[300,400]]);
assert.deepEqual(packed.pcm.subarray(0,300),pcm.subarray(100,400));assert.deepEqual(packed.pcm.subarray(300),pcm.subarray(700,800));
const entry={id:'a',name:'Found chops',...packed},second={id:'b',name:'Next song',...compactChops(pcm,[{...c,a:500,b:700}])};
const song=assembleCollected([entry,second],[{id:'a',index:1},{id:'b',index:0},{id:'a',index:0},{id:'a',index:1}]);
assert.equal(song.pcm.length,500,'duplicate and overlapping assignments share stored audio');
assert.deepEqual(song.chops.map(c=>[c.a,c.b]),[[100,300],[300,500],[0,200],[100,300]]);
for(const chop of song.chops){assert.equal(chop.play,32768);assert.equal(chop.cents,25);assert.equal(chop.reverse,1);assert.equal(chop.gain,512);}
const restored=importCollection(exportCollection(entry));assert.deepEqual(restored.pcm,entry.pcm);assert.deepEqual(restored.chops,entry.chops);
assert.throws(()=>importCollection({...exportCollection(entry),rate:44100}));assert.throws(()=>importCollection({...exportCollection(entry),chops:[{...c,a:0,b:99999}]}));assert.throws(()=>compactChops(pcm,[{...c,a:-1}]));assert.throws(()=>assembleCollected([entry],[]));
const hit=collectedHit({pcm,chops:[{a:100,b:500,pitch:0,flags:0,play:32768,reverse:1,gain:256,cents:0}]},0);
assert.equal(hit.length,201);assert.equal(hit[0],Math.round(pcm[300]*32767));assert.equal(hit.at(-1),Math.round(pcm[100]*32767));
assert.deepEqual(pcm,Float32Array.from({length:1000},(_,i)=>i/2000),'collect/build must not change source audio');
console.log('Chop library: compact ranges, shared overlaps, mixed-source order, duplicate assignments, settings, one-shots, export/import and validation passed.');
