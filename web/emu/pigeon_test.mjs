// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
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
const tone=n=>Float32Array.from({length:n},(_,i)=>Math.sin(i*0.1)*0.7);
const small=[{name:'own.wav',pcm:tone(22050)},null];
function load(image,unlimited=false){const ptr=e.web_upload_buffer(image.length);assert(ptr);new Uint8Array(e.memory.buffer,ptr,image.length).set(image);return e.web_load_bank(image.length,unlimited?1:0,0);}
assert.equal(load(buildBank(small)),0);assert.equal(e.web_samples_loaded(),1);
e.web_keys(1);render(200);e.web_keys(0);render(80);assert(peak>0.001,'imported audio must sound');
const large=[{name:'long.wav',pcm:tone(900000)},null];
assert.throws(()=>buildBank(large),/408 KiB/);
const unlimited=buildBank(large,[],true);assert(unlimited.length>FM1_POOL);
assert.equal(load(unlimited),1,'hardware cap enforced in WASM');
assert.equal(load(unlimited,true),0);assert.equal(e.web_sample_pool_size(),unlimited.length);
peak=0;e.web_keys(1);render(200);e.web_keys(0);render(80);assert(peak>0.001,'expanded pool audio must sound');
const corrupt=unlimited.slice();corrupt[5000]^=1;assert.equal(load(corrupt,true),2);
e.web_clear_bank();assert.equal(e.web_samples_loaded(),0);

e.web_buttons(1<<10);render(60);e.web_buttons(0);render(60);assert.equal(e.web_playing(),1);
e.web_buttons(1<<10);render(60);e.web_buttons(0);render(60);assert.equal(e.web_playing(),0);
const nor=new Uint8Array(e.memory.buffer,e.web_nor(),e.web_nor_size());
for(const offset of [0x78000,0xEA000,0xFC000])nor[offset]=42;
const saved=storeSectors(nor), copy=new Uint8Array(nor.length).fill(255);restoreSectors(copy,saved);
for(const offset of [0x78000,0xEA000,0xFC000])assert.equal(copy[offset],42);
console.log('PIGEON WASM: empty boot, silent empty slots, own audio, FM1 capacity rejection, expanded pool playback, CRC validation, clearing, transport and persistence passed.');
