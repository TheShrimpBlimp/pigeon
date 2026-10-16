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
 await page.route('**/api/trials',r=>r.fulfill({json:{total:123}}));
 await page.goto(`http://127.0.0.1:${server.address().port}/webapp/emu/`);
 const loader=page.locator('.sample-loader');
 assert.equal(await loader.locator('.sample-toolbar .control-group').count(),3);
 assert.equal(await loader.locator('.sample-slot .song-header').count(),3);
 assert.equal(await loader.locator('#song1-editor .export-settings').getAttribute('open'),null);
 assert.equal(await loader.locator('#song1-quality').evaluate(e=>!!e.closest('.export-settings')),true);
 await page.locator('#go').click();await page.locator('#start').waitFor({state:'hidden'});
 await page.locator('#song1-file').setInputFiles({name:'layout.wav',mimeType:'audio/wav',buffer:b});
 await page.getByText('Song 1 selected. Edit the section, then press Apply to Song 1.',{exact:true}).waitFor();
 const wave=page.locator('#song1-editor canvas');await wave.scrollIntoViewIfNeeded();const box=await wave.boundingBox();
 await page.mouse.move(box.x+box.width*.2,box.y+box.height*.45);await page.mouse.down();await page.mouse.move(box.x+box.width*.65,box.y+box.height*.45);await page.mouse.up();
 assert(Number(await page.locator('#song1-editor .trim-start').inputValue())>0);
 await page.mouse.wheel(0,-100);await page.waitForFunction(()=>Number(document.querySelector('#song1-editor canvas').dataset.zoom)>0);
 assert.match(await page.locator('#song1-editor .trim-cursor').textContent(),/Cursor/);
 await page.locator('#song1-editor .export-settings summary').click();
 await page.locator('#song1-editor .trim-start').fill('0.25');await page.locator('#song1-editor .trim-end').fill('1');await page.locator('#song1-quality').selectOption('32');
 await page.locator('#song1-editor .apply-section').click();await page.getByText('Samples loaded. Play the keys to try them.',{exact:true}).waitFor();assert.equal(await page.locator('#song1-name').textContent(),'layout.wav');assert(!await page.locator('#save-bank').isDisabled());
 await page.locator('#song1-editor .preview-section').click();await page.locator('#song1-editor .preview-stop:not([disabled])').waitFor();await page.locator('#song1-editor .preview-stop').click();
 await page.locator('#kit-files').setInputFiles({name:'pad.wav',mimeType:'audio/wav',buffer:b});try{await page.getByText('Drum files selected. Edit each pad, then press Apply drum kit.',{exact:true}).waitFor();}catch(e){console.log('Sample status:',await page.locator('#sample-status').textContent(),'page errors:',errors);throw e;}assert.equal(await page.locator('#pad-editors .playback-controls').count(),1);
 for(const width of [1280,390]){await page.setViewportSize({width,height:900});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await loader.screenshot({path:`build/emulator-loader-layout-${width}.png`});}
 assert.deepEqual(errors,[]);console.log('Emulator loader layout: source/listen/view/export grouping, shared waveform cursor/zoom, import/apply/preview, drum editor, desktop/mobile passed.');
}finally{await browser.close();await new Promise(r=>server.close(r));}
