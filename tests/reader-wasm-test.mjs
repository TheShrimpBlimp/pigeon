// SPDX-License-Identifier: GPL-3.0-only
// Exercise the real compiled protocol and observer through the browser exports.
import assert from 'node:assert/strict';import fs from 'node:fs';
const {instance}=await WebAssembly.instantiate(fs.readFileSync('build/emu/felucca.wasm'),{}),e=instance.exports;e._initialize();e.web_nor_erase();e.web_boot();
function render(ms){for(let i=0;i<Math.ceil(ms*44.1);i+=128)e.web_render(128);}
function input(cmd){const frame=Uint8Array.from([240,125,83,82,cmd,...(cmd===1?[1]:[]),247]);new Uint8Array(e.memory.buffer,e.web_sysex_buffer(),frame.length).set(frame);assert.equal(e.web_sysex(frame.length),1);}
function drain(){const frames=[];for(let size=e.web_said_size();size;size=e.web_said_size()){frames.push(Array.from(new Uint8Array(e.memory.buffer,e.web_said_buf(),size)));e.web_said_done();}return frames;}
const text=f=>String.fromCharCode(...f.slice(6,-1));
render(3200);assert.equal(e.web_said_size(),0,'silent until HELLO');
input(1);let frames=drain();assert(frames.some(f=>f[4]===1 && text(f).startsWith('PIGEON v0.4\0https://theshrimpblimp.com/pigeon/words/')));
render(160);frames=drain();assert(frames.some(f=>f[4]===16&&/Waveform/.test(text(f))));assert(frames.some(f=>/No song loaded/.test(text(f))));
// FX is panel bit zero; keep a debounced physical tap across two frames.
e.web_buttons(1);render(80);e.web_buttons(0);render(300);frames=drain();assert(frames.some(f=>/Effects amounts/.test(text(f))));
e.web_enc(4,1);render(240);frames=drain();assert(frames.some(f=>/delay|filter|reverb|chorus/.test(text(f))));
input(3);render(160);frames=drain();assert(frames.some(f=>/Knob 2, delay/.test(text(f))));
input(2);drain();e.web_enc(4,2);render(240);assert.equal(e.web_said_size(),0);
const ptr=e.web_sysex_buffer();new Uint8Array(e.memory.buffer,ptr,6).set([240,125,83,82,3,0]);assert.equal(e.web_sysex(6),0,'reject incomplete framing');
console.log('PIGEON reader WASM: silence, identity, versioned words URL, page changes, knob announcements, WHERE, BYE and malformed input passed.');
