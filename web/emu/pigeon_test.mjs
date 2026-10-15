// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {normalize,indexedMp3,join,inspect} from '../pigeon/bank.js';
import {storeSectors,restoreSectors} from './worklet.js';
import {buildBank,FM1_POOL} from './loader.js';
const bytes=fs.readFileSync('build/emu/felucca.wasm');
const {instance}=await WebAssembly.instantiate(bytes,{}), e=instance.exports;
e._initialize();e.web_nor_erase();e.web_boot();
let peak=0;
function render(ms){for(let i=0;i<ms*44.1;i+=128){e.web_render(128);for(const v of new Float32Array(e.memory.buffer,e.web_out_l(),128)){assert(Number.isFinite(v));peak=Math.max(peak,Math.abs(v));}}}
render(1200);assert(e.web_screen_draws()>0);
assert.equal(e.web_samples_loaded(),0,'empty boot');
e.web_keys(1);render(100);e.web_keys(0);render(50);assert.equal(peak,0,'empty slots must be silent');
render(1900); // Finish the three-second splash before auditioning imported samples.
const tone=n=>Float32Array.from({length:n},(_,i)=>Math.sin(i*0.1)*0.7);
const small=[{name:'own.wav',pcm:tone(22050)},null];
function load(image,unlimited=false){const ptr=e.web_upload_buffer(image.length);assert(ptr);new Uint8Array(e.memory.buffer,ptr,image.length).set(image);return e.web_load_bank(image.length,unlimited?1:0,0);}
assert.equal(load(buildBank(small)),0);assert.equal(e.web_samples_loaded(),1);
e.web_keys(1);render(200);e.web_keys(0);render(80);assert(peak>0.001,'imported audio must sound');
const sandbox={console};vm.createContext(sandbox);vm.runInContext(fs.readFileSync('web/pigeon/vendor/lame.js','utf8'),sandbox);
for(const bitrate of [24,32,40,48,56,64]){
 const pcm=normalize(small[0].pcm),encoder=new sandbox.lamejs.Mp3Encoder(1,22050,bitrate),chunks=[];
 for(let i=0;i<pcm.length;i+=1152)chunks.push(new Uint8Array(encoder.encodeBuffer(pcm.subarray(i,i+1152))));
 chunks.push(new Uint8Array(encoder.flush()));const compressed={...indexedMp3(join(chunks),pcm.length),bitrate};
 const song={...small[0],compressed};const image=buildBank([song,null]);assert.equal(inspect(image).songs[0].codec,'MP3');
 assert.equal(load(image),0,`MP3 ${bitrate} bank must load`);peak=0;e.web_keys(1);render(200);e.web_keys(0);render(80);assert(peak>0.001,`MP3 ${bitrate} must sound in real WASM firmware`);
}
const expanded=[{name:'expanded.wav',pcm:tone(900000)},null];
const expandedBank=buildBank(expanded,[],true);assert(expandedBank.length>FM1_POOL && expandedBank.length<=0x7D000);assert.equal(load(expandedBank),0,'expanded hardware bank must play');
const large=[{name:'long.wav',pcm:tone(1100000)},null];
assert.throws(()=>buildBank(large),/408 KiB/);
const unlimited=buildBank(large,[],true);assert(unlimited.length>FM1_POOL);
assert.equal(load(unlimited),1,'hardware cap enforced in WASM');
assert.equal(load(unlimited,true),0);assert.equal(e.web_sample_pool_size(),unlimited.length);
peak=0;e.web_keys(1);render(200);e.web_keys(0);render(80);assert(peak>0.001,'expanded pool audio must sound');
const corrupt=unlimited.slice();corrupt[5000]^=1;assert.equal(load(corrupt,true),2);
e.web_clear_bank();assert.equal(e.web_samples_loaded(),0);

e.web_buttons(1<<10);render(60);e.web_buttons(0);render(60);assert.equal(e.web_playing(),1);
e.web_buttons(1<<10);render(60);e.web_buttons(0);render(60);assert.equal(e.web_playing(),0);
// Legacy synth banks are opaque session data, with no active preset RAM mirror.
function restoreObject(id,data){const ptr=e.web_upload_buffer(Math.max(1,data.length));new Uint8Array(e.memory.buffer,ptr,data.length).set(data);return e.web_restore_object(id,data.length);}
function sessionObject(id){const ptr=e.web_session_object(id),length=e.web_session_size();return new Uint8Array(e.memory.buffer,ptr,length).slice();}
const legacyBank=new Uint8Array(3080),bankView=new DataView(legacyBank.buffer);
bankView.setUint32(0,0x31425055,true);bankView.setUint16(4,192,true);bankView.setUint16(6,16,true);legacyBank[24]=99;
assert.equal(restoreObject(6,legacyBank),0);assert.deepEqual(sessionObject(6),legacyBank);
const runtimeBefore=sessionObject(0);sessionObject(6);assert.deepEqual(sessionObject(0),runtimeBefore);
const legacyPatches=new Uint8Array(3728),patchView=new DataView(legacyPatches.buffer);
patchView.setUint32(0,0x36465055,true);patchView.setUint16(4,1,true);patchView.setUint16(6,32,true);patchView.setUint32(8,1,true);legacyPatches[25]=93;
assert.equal(restoreObject(9,legacyPatches),0);assert.deepEqual(sessionObject(9),legacyPatches);
assert.equal(restoreObject(9,new Uint8Array()),0);assert.equal(sessionObject(9).length,0);
const legacyOld=new Uint8Array(3472),oldView=new DataView(legacyOld.buffer);
oldView.setUint32(0,0x42364d46,true);oldView.setUint16(4,1,true);oldView.setUint16(6,27,true);oldView.setUint32(8,1,true);legacyOld[21]=47;
assert.equal(restoreObject(8,legacyOld),0);assert.deepEqual(sessionObject(8),legacyOld);
assert.equal(restoreObject(9,new Uint8Array()),0);assert.deepEqual(sessionObject(8),legacyOld);
const nor=new Uint8Array(e.memory.buffer,e.web_nor(),e.web_nor_size());
for(const offset of [0x78000,0xEA000,0xFC000])nor[offset]=42;
const saved=storeSectors(nor), copy=new Uint8Array(nor.length).fill(255);restoreSectors(copy,saved);
for(const offset of [0x78000,0xEA000,0xFC000])assert.equal(copy[offset],42);
console.log('PIGEON WASM: empty boot, silent empty slots, own audio, FM1 capacity rejection, expanded pool playback, CRC validation, clearing, transport and persistence passed.');
