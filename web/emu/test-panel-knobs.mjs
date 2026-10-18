// SPDX-License-Identifier: GPL-3.0-only
// Focused layout/import smoke check; does not run the firmware regression suite.
import assert from 'node:assert/strict';
import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
import {chromium} from '../../build/gui-deps/testing/node_modules/playwright/index.mjs';
const root=path.resolve('web/site/build/pages');
const server=http.createServer(async(req,res)=>{try{let p=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);if((await fs.stat(p)).isDirectory())p=path.join(p,'index.html');if(!p.startsWith(root+'/'))throw Error();res.setHeader('Content-Type',p.endsWith('.js')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':p.endsWith('.css')?'text/css':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.writeHead(404);res.end();}});
const pcm=44100,b=Buffer.alloc(44+pcm*2);b.write('RIFF');b.writeUInt32LE(b.length-8,4);b.write('WAVEfmt ',8);b.writeUInt32LE(16,16);b.writeUInt16LE(1,20);b.writeUInt16LE(1,22);b.writeUInt32LE(22050,24);b.writeUInt32LE(44100,28);b.writeUInt16LE(2,32);b.writeUInt16LE(16,34);b.write('data',36);b.writeUInt32LE(pcm*2,40);for(let i=0;i<pcm;i++)b.writeInt16LE(Math.round(Math.sin(i*.1)*20000),44+i*2);
await new Promise(r=>server.listen(0,'127.0.0.1',r));const browser=await chromium.launch({headless:true});
try{
 const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.route('**/api/trials',r=>r.fulfill({json:{total:1}}));
 await page.goto(`http://127.0.0.1:${server.address().port}/webapp/emu/`);
 await page.locator('#go').click();await page.locator('#start').waitFor({state:'hidden'});
 await page.locator('#song1-file').setInputFiles({name:'knobs.wav',mimeType:'audio/wav',buffer:b});
 await page.getByText('Song 1 selected. Edit the section, then press Apply to Song 1.',{exact:true}).waitFor();
 await page.locator('#song1-editor .apply-section').click();await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();
 await page.evaluate(()=>{
  const input={id:'fm1-in',name:'PIGEON',state:'connected',onmidimessage:null};window.panelTestInput=input;
  const output={id:'fm1-out',name:'PIGEON',state:'connected',send:()=>{}};
  navigator.requestMIDIAccess=async options=>{if(!options?.sysex)throw Error('Expected SysEx');return {inputs:new Map([[input.id,input]]),outputs:new Map([[output.id,output]])};};
  document.getElementById('panel-connect').closest('details').open=true;
 });
 await page.waitForFunction(()=>Number(document.getElementById("screen").dataset.deviceTime)>3500);
 await page.locator('#panel-connect').click();
 const indicator=page.locator('.knob-indicator[data-knob="KNOB 1"]');
 assert.equal(await page.locator('.knob-indicator').count(),8);
 const rotation=await indicator.getAttribute('transform');
 async function savedMetadata(){const download=page.waitForEvent('download');await page.locator('#save-bank').click();const d=await download;return new Uint8Array(await fs.readFile(await d.path()));}
 const before=await savedMetadata();
 await page.evaluate(()=>{const d=Uint8Array.of(240,125,70,76,111,1,0,0,0,0,0,0,64,64,64,69,64,64,64,60,5,247);window.panelTestInput.onmidimessage({data:d});});
 await page.getByText('KNOB 1: +5 steps received.',{exact:true}).waitFor();
 assert.notEqual(await indicator.getAttribute('transform'),rotation);
 await page.waitForTimeout(200);
 const after=await savedMetadata();
 // FKO1 metadata starts after the encoded bank payload; first chop start is +16.
 const offset=before.length-4096+256+16;
 assert.notEqual(new DataView(after.buffer,after.byteOffset,after.length).getUint32(offset,true),new DataView(before.buffer,before.byteOffset,before.length).getUint32(offset,true),'physical encoder frame must edit actual chop metadata');
 await page.locator('#panel-disconnect').click();assert(await page.evaluate(()=>window.panelTestInput.onmidimessage===null));
 assert.deepEqual(errors,[]);console.log('Panel knob: real SysEx frame changes visible knob angle and actual WASM chop boundaries; disconnect releases input.');
}finally{await browser.close();await new Promise(r=>server.close(r));}
