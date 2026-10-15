// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {Device} from './device.js';
import {buildBank} from '../emu/loader.js';
const wire=n=>[n&127,(n>>>7)&127,(n>>>14)&127];
const image=buildBank([{name:'expanded',pcm:Float32Array.from({length:900000},(_,i)=>Math.sin(i*.1)*.6)},null]);
assert(image.length>0x67000&&image.length<=0x7D000);
const commands=[],old=new Device(async([cmd])=>{commands.push(cmd);return [75,3,...wire(0x67000),...wire(4096),0,3];});
await assert.rejects(old.writeBank(image),/supports 408 KiB/);assert.deepEqual(commands,[90],'capacity rejection must precede BEGIN/erase');
const expanded=new Device(async()=>[75,3,...wire(0x7D000),...wire(4096),0,3]);await expanded.negotiate();assert.equal(expanded.capacity,0x7D000);
console.log('Capacity negotiation: old/new layouts and oversized-bank refusal before erase passed.');
