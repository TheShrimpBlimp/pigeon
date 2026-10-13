// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
import {chromium} from '../../build/gui-deps/testing/node_modules/playwright/index.mjs';
const root=path.resolve('web/site/build/pages');
const server=http.createServer(async(req,res)=>{try{const name=new URL(req.url,'http://localhost').pathname;const p=path.resolve(root,'.'+(name.endsWith('/')?name+'index.html':name));if(!p.startsWith(root+'/'))throw Error();const data=await fs.readFile(p);res.setHeader('Content-Type',p.endsWith('.js')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':p.endsWith('.css')?'text/css':'application/octet-stream');res.end(data);}catch{res.writeHead(404);res.end();}});
function wav(seconds){const count=Math.floor(seconds*22050),b=Buffer.alloc(44+count*2);b.write('RIFF');b.writeUInt32LE(b.length-8,4);b.write('WAVEfmt ',8);b.writeUInt32LE(16,16);b.writeUInt16LE(1,20);b.writeUInt16LE(1,22);b.writeUInt32LE(22050,24);b.writeUInt32LE(44100,28);b.writeUInt16LE(2,32);b.writeUInt16LE(16,34);b.write('data',36);b.writeUInt32LE(count*2,40);for(let i=0;i<count;i++)b.writeInt16LE(Math.round(Math.sin(i*0.1)*24000),44+i*2);return b;}
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({headless:true});
try{
 const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
 const trialIds=new Set();let trialPosts=0;
 await page.route('**/api/trials',route=>{const request=route.request();if(request.method()==='POST'){trialPosts++;trialIds.add(request.postDataJSON().id);}return route.fulfill({json:{total:trialIds.size}});});
 await page.goto(`http://127.0.0.1:${server.address().port}/webapp/emu/`);
 await page.locator('#go').click();await page.locator('#start').waitFor({state:'hidden'});
 await page.waitForFunction(()=>Number(document.getElementById("screen").dataset.deviceTime)>1000);

 await page.getByText('Anonymous browser trials',{exact:true}).waitFor();
 assert.equal(trialIds.size,1);assert.equal(await page.locator('#trial-total .nixie-unlit b').count(),60);
 assert.equal(await page.locator('#trial-total .nixie-active').allTextContents().then(a=>a.join('')),'000001');
 assert.equal(await page.locator('#song1-name').textContent(),'Empty');
 assert.equal(await page.locator('#sample-usage').textContent(),'0.0 KiB / 408 KiB');
 await page.locator('#song1-file').setInputFiles({name:'editable.wav',mimeType:'audio/wav',buffer:wav(2)});
 await page.getByText('Song 1 selected. Edit the section, then press Apply to Song 1.',{exact:true}).waitFor();
 assert.equal(await page.locator('#song1-name').textContent(),'Empty','choosing a file must not load it before editing');
 await page.locator('#song1-editor canvas').scrollIntoViewIfNeeded();
 const wave=await page.locator('#song1-editor canvas').boundingBox();
 await page.mouse.move(wave.x+wave.width*.2,wave.y+wave.height/2);await page.mouse.down();await page.mouse.move(wave.x+wave.width*.7,wave.y+wave.height/2);await page.mouse.up();
 assert(Number(await page.locator('#song1-editor .trim-start').inputValue())>0);
 await page.locator('#song1-editor .trim-zoom').fill('2');await page.locator('#song1-editor .trim-scroll').fill('300');
 await page.locator('#song1-editor .trim-start').fill('0.25');await page.locator('#song1-editor .trim-end').fill('1');
 await page.locator('#song1-editor .trim-start').focus();await page.keyboard.press('ArrowUp');assert.equal(await page.locator('#song1-editor .trim-start').inputValue(),'0.26');await page.locator('#song1-editor .trim-start').fill('0.25');
 await page.locator('#song1-editor .loop-preview select').selectOption('1');await page.locator('#song1-editor .preview-section').click();
 await page.locator('#song1-editor .preview-stop:not([disabled])').waitFor();await page.locator('#song1-editor .preview-stop').click();
 await page.locator('#song1-editor .trim-end').fill('3');await page.locator('#song1-editor .apply-section').click();
 await page.getByText('Choose valid start and end points inside the audio.',{exact:true}).waitFor();assert.equal(await page.locator('#song1-name').textContent(),'Empty');
 await page.locator('#song1-editor .trim-end').fill('1');await page.locator('#song1-editor .apply-section').click();
 await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();
 const trimmedCount=await page.evaluate(async()=>{const db=await new Promise(r=>{const q=indexedDB.open('pigeon-emulator-samples',1);q.onsuccess=()=>r(q.result);});const value=await new Promise(r=>{const q=db.transaction('samples').objectStore('samples').get('current');q.onsuccess=()=>r(q.result);});db.close();return new DataView(value.image.buffer,value.image.byteOffset,value.image.byteLength).getUint32(24,true);});
 assert.equal(trimmedCount,16538,'encoded song must contain the selected region only');
 await page.reload();await page.locator('#go').click();await page.locator('#start').waitFor({state:'hidden'});
 assert.equal(await page.locator('#song1-editor .trim-start').inputValue(),'0.25');assert.equal(await page.locator('#song1-editor .trim-end').inputValue(),'1');
 assert.equal(await page.locator('#song1-editor .edit-source').textContent(),'Selected: editable.wav · 2.00 s');

 await page.locator('#song1-file').setInputFiles({name:'my-song.wav',mimeType:'audio/wav',buffer:wav(1)});
 await page.getByText('Song 1 selected. Edit the section, then press Apply to Song 1.',{exact:true}).waitFor();
 await page.locator('#song1-editor .apply-section').click();
 await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();
 assert.equal(await page.locator('#song1-name').textContent(),'my-song.wav');
 await page.locator('#song2-file').setInputFiles({name:'second.wav',mimeType:'audio/wav',buffer:wav(0.5)});
 await page.getByText('Song 2 selected. Edit the section, then press Apply to Song 2.',{exact:true}).waitFor();
 await page.locator('#song2-editor .apply-section').click();
 await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();
 await page.locator('#kit-files').setInputFiles([{name:'pad.wav',mimeType:'audio/wav',buffer:wav(.1)}]);
 await page.getByText('Drum files selected. Edit each pad, then press Apply drum kit.',{exact:true}).waitFor();
 await page.locator('#pad-editors .trim-end').fill('0.05');
 await page.locator('#apply-kit').click();
 await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();
 assert.equal(await page.locator('#kit-name').textContent(),'pad.wav');
 const used64=await page.locator('#sample-meter').getAttribute('value');
 await page.locator('#song1-quality').selectOption('24');
 await page.locator('#song1-editor .apply-section').click();await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();
 await page.locator('#song1-file:not([disabled])').waitFor();
 const used24=await page.locator('#sample-meter').getAttribute('value');assert(Number(used24)<Number(used64),'lower bitrate must save space');
 await page.locator('#song1-quality').selectOption('64');await page.locator('#song1-editor .apply-section').click();await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();await page.locator('#song1-file:not([disabled])').waitFor();
 await page.locator('#song1-file').setInputFiles({name:'too-long.wav',mimeType:'audio/wav',buffer:wav(65)});
 await page.getByText('Song 1 selected. Edit the section, then press Apply to Song 1.',{exact:true}).waitFor();
 await page.locator('#song1-editor .apply-section').click();
 await page.waitForFunction(()=>document.getElementById('sample-status').textContent.includes('408 KiB'));
 assert.equal(await page.locator('#song1-name').textContent(),'my-song.wav');
 await page.locator('#song1-quality').selectOption('24');await page.locator('#song1-file:not([disabled])').waitFor();
 await page.locator('#song1-file').setInputFiles({name:'compressed.wav',mimeType:'audio/wav',buffer:wav(65)});
 await page.getByText('Song 1 selected. Edit the section, then press Apply to Song 1.',{exact:true}).waitFor();
 await page.locator('#song1-editor .apply-section').click();
 await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();
 assert.equal(await page.locator('#song1-name').textContent(),'compressed.wav');
 assert(Number(await page.locator('#sample-meter').getAttribute('value'))<=417792);
 await page.locator('#storage-mode').selectOption('unlimited');await page.locator('#song1-file:not([disabled])').waitFor();
 assert(await page.locator('#song1-quality').isHidden());
 await page.locator('#storage-mode').selectOption('fm1');await page.locator('#song1-file:not([disabled])').waitFor();
 assert.equal(await page.locator('#storage-mode').inputValue(),'fm1');assert(await page.locator('#song1-quality').isVisible());
 await page.reload();await page.locator('#go').click();await page.locator('#start').waitFor({state:'hidden'});
 await page.getByText('Anonymous browser trials',{exact:true}).waitFor();assert.equal(trialIds.size,1);assert(trialPosts>=3);
 assert.equal(await page.locator('#song1-name').textContent(),'compressed.wav');
 assert.equal(await page.locator('#song1-quality').inputValue(),'24');
 assert.equal(await page.locator('#storage-mode').inputValue(),'fm1');
 await page.evaluate(()=>{
   const input={id:'fm1',name:'FM-1',state:'connected',onmidimessage:null};window.testInput=input;window.sent=[];
   navigator.requestMIDIAccess=async options=>{if(options?.sysex)throw Error('SysEx should not be requested');return {inputs:new Map([['fm1',input]]),outputs:new Map()};};
 });
 await page.locator('#midi-connect').click();await page.getByText('Connected: FM-1. Use its keys to play the selected track.',{exact:true}).waitFor();
 await page.evaluate(()=>window.testInput.onmidimessage({data:Uint8Array.from([144,60,100])}));
 await page.evaluate(()=>window.testInput.onmidimessage({data:Uint8Array.from([128,60,0])}));
 await page.locator('#midi-disconnect').click();assert(await page.evaluate(()=>window.testInput.onmidimessage===null));
 await page.locator('#clear-samples').click();await page.getByText('Samples cleared.',{exact:true}).waitFor();
 assert.equal(await page.locator('#song1-name').textContent(),'Empty');
 await page.evaluate(()=>document.fonts.ready);
 assert(await page.evaluate(()=>document.fonts.check('16px Little')));
 assert.equal(await page.locator('body').evaluate(e=>getComputedStyle(e).backgroundColor),'rgb(17, 21, 15)');
 for(const width of [1280,390]){await page.setViewportSize({width,height:900});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await page.screenshot({path:`build/pigeon-emulator-${width}.png`,fullPage:true});}
 for(const route of ['/', '/webapp/emu/', '/webapp/pigeon/', '/webapp/installer/', '/webapp/editor/', '/firmware/LICENSES/']){
  await page.goto(`http://127.0.0.1:${server.address().port}${route}`);
  if(route==='/'){const callout=page.locator('.browser-trial-callout');assert(await callout.isVisible());assert.equal(await callout.locator('a').getAttribute('href'),'webapp/emu/');assert((await callout.boundingBox()).y<500);}
  const coffee=page.locator('.coffee-link');assert.equal(await coffee.count(),1);assert.equal(await coffee.getAttribute('href'),'https://buymeacoffee.com/theshrimpb1');
  const box=await coffee.boundingBox();assert(box&&box.y<350,`coffee link near top on ${route}`);
 }
 assert.deepEqual(errors,[]);console.log('Browser: empty boot, own audio, waveform drag, numeric trim, preview/loop, invalid trim refusal, trimmed playback metadata, editor persistence, song/kit loading, both storage modes, persistence, MIDI-only connection, clearing, lit.chr, desktop/mobile passed.');
}finally{await browser.close();await new Promise(r=>server.close(r));}
