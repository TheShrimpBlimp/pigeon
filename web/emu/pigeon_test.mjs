// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {storeSectors,restoreSectors} from './worklet.js';
import {panelFrame} from './controller.js';
const bytes=fs.readFileSync('build/emu/felucca.wasm');
const {instance}=await WebAssembly.instantiate(bytes,{}), e=instance.exports;
e._initialize();e.web_nor_erase();e.web_boot();
let peak=0;
function render(ms){for(let i=0;i<ms*44.1;i+=128){e.web_render(128);for(const v of new Float32Array(e.memory.buffer,e.web_out_l(),128)){assert(Number.isFinite(v));peak=Math.max(peak,Math.abs(v));}}}
render(1200);assert(e.web_screen_draws()>0);
e.web_keys(1);render(200);e.web_keys(0);render(80);assert(peak>0.001,'demo keys must sound');
e.web_buttons(1<<10);render(60);e.web_buttons(0);render(60);assert.equal(e.web_playing(),1);
e.web_buttons(1<<10);render(60);e.web_buttons(0);render(60);assert.equal(e.web_playing(),0);
const nor=new Uint8Array(e.memory.buffer,e.web_nor(),e.web_nor_size());
for(const offset of [0x78000,0xEA000,0xFC000])nor[offset]=42;
const saved=storeSectors(nor), copy=new Uint8Array(nor.length).fill(255);restoreSectors(copy,saved);
for(const offset of [0x78000,0xEA000,0xFC000])assert.equal(copy[offset],42);
assert.equal(panelFrame([240,125,70,76,111,1,0,0,0,0,0,0,64,64,64,64,64,64,64,60,5,247]).master,700);
assert.equal(panelFrame([240,125,70,76,111,1,0,0,0,0,0,0,65,63,64,64,64,64,64,0,0,247]).enc[1],-1);
assert.equal(panelFrame([240,125,70,76,111,1,247]),null);
console.log('Pigeon WASM: boot, audible demo, transport, pool persistence and controller frames passed.');
