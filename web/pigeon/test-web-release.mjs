// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
import {chromium} from '../../build/gui-deps/testing/node_modules/playwright/index.mjs';
import {inspect,crc} from './bank.js';
import {buildBank} from '../emu/loader.js';
import {validateArchive,b64} from './device.js';
import {editStep,project,readStep} from './editing.js';
const root=path.resolve(process.env.PIGEON_SITE_ROOT||'/tmp/pigeon-editor-site/dist/client');
const server=http.createServer(async(req,res)=>{try{const url=new URL(req.url,'http://localhost');if(url.pathname.includes('/api/')){res.setHeader('Content-Type','application/json');res.end('{"total":1}');return;}const target=path.resolve(root,'.'+(url.pathname.endsWith('/')?url.pathname+'index.html':url.pathname));if(!target.startsWith(root+'/'))throw Error();const data=await fs.readFile(target);res.setHeader('Content-Type',target.endsWith('.js')?'text/javascript':target.endsWith('.wasm')?'application/wasm':target.endsWith('.html')?'text/html':target.endsWith('.css')?'text/css':'application/octet-stream');res.end(data);}catch{res.writeHead(404);res.end();}});
const wav=name=>{const count=22050,b=Buffer.alloc(44+count*2);b.write('RIFF');b.writeUInt32LE(b.length-8,4);b.write('WAVEfmt ',8);b.writeUInt32LE(16,16);b.writeUInt16LE(1,20);b.writeUInt16LE(1,22);b.writeUInt32LE(22050,24);b.writeUInt32LE(44100,28);b.writeUInt16LE(2,32);b.writeUInt16LE(16,34);b.write('data',36);b.writeUInt32LE(count*2,40);for(let i=0;i<count;i++)b.writeInt16LE(Math.round(Math.sin(i*.13)*18000),44+i*2);return {name,mimeType:'audio/wav',buffer:b};};
await new Promise(r=>server.listen(0,'127.0.0.1',r));const origin=`http://127.0.0.1:${server.address().port}`;
const browser=await chromium.launch({headless:true,args:['--autoplay-policy=no-user-gesture-required']});
try{
 const page=await browser.newPage({viewport:{width:1280,height:900}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto(origin+'/');await page.getByRole('button',{name:'for font nerds',exact:true}).click();
 const dialog=page.getByRole('dialog');await dialog.waitFor({state:'visible'});assert.equal(await dialog.locator('a').getAttribute('href'),'https://www.hyperfixedpod.com/listen/hyperfixed/little-by-little');assert((await dialog.textContent()).includes("I'm not affiliated with that podcast I just really like it."));await page.keyboard.press('Escape');await dialog.waitFor({state:'hidden'});
 await page.goto(origin+'/webapp/pigeon/');await page.locator('#status').filter({hasText:/Ready|Restored/}).waitFor();
 await page.locator('#file0').setInputFiles([wav('first.wav'),wav('second.wav')]);await page.waitForFunction(()=>document.querySelectorAll('#clips0 .clip-row').length===2);
 await page.locator('#file0').setInputFiles(wav('third.wav'));await page.waitForFunction(()=>document.querySelectorAll('#clips0 .clip-row').length===3);
 assert.equal(Number(await page.locator('#end0').inputValue()),3);
 const canvas=page.locator('#wave0');await canvas.scrollIntoViewIfNeeded();const box=await canvas.boundingBox();
 await page.mouse.move(box.x+box.width*.2,box.y+box.height*.45);await page.mouse.down();await page.mouse.move(box.x+box.width*.4,box.y+box.height*.45,{steps:8});await page.mouse.up();await page.keyboard.press('Backspace');
 await page.waitForFunction(()=>Number(document.getElementById('end0').value)<2.5);assert(Number(await page.locator('#end0').inputValue())>2.3);
 await page.keyboard.press('ControlOrMeta+z');await page.waitForFunction(()=>Number(document.getElementById('end0').value)===3);
 await page.keyboard.press('ControlOrMeta+Shift+z');await page.waitForFunction(()=>Number(document.getElementById('end0').value)<2.5);
 await page.keyboard.press('ControlOrMeta+z');await page.waitForFunction(()=>Number(document.getElementById('end0').value)===3);
 assert.equal(await canvas.getAttribute('data-wave-columns'),'220');
 await canvas.click({position:{x:box.width*.5,y:box.height*.45}});await page.keyboard.press('s');await page.waitForFunction(()=>document.querySelectorAll('#clips0 .clip-row').length===4);
 await page.evaluate(()=>{window.dragEvents=[];for(const kind of ['dragstart','drop','dragend'])document.addEventListener(kind,e=>window.dragEvents.push({kind,target:e.target.tagName,types:[...(e.dataTransfer?.types||[])],data:e.dataTransfer?.getData('application/x-pigeon-clip')}));});
 await page.locator('#timeline-strip0 button').last().dragTo(page.locator('#timeline-strip0 button').first());await page.waitForFunction(()=>document.querySelector('#clips0 .clip-row p').textContent.includes('third.wav'),null,{timeout:10000}).catch(async e=>{console.log(await page.evaluate(()=>({events:window.dragEvents,status:document.getElementById('status').textContent,clips:[...document.querySelectorAll('#clips0 .clip-row p')].map(n=>n.textContent)})),errors);throw e;});
 await page.keyboard.press('ControlOrMeta+z');await page.waitForFunction(()=>document.querySelector('#clips0 .clip-row p').textContent.includes('first.wav'));
 await page.locator('#apply0').click();await page.locator('#status').filter({hasText:/Song 1 imported/}).waitFor();
 await page.screenshot({path:'build/pigeon-editor-release.png',fullPage:true});
 const original=await page.evaluate(async()=>{const d=await new Promise(r=>{const q=indexedDB.open('pigeon-library',1);q.onsuccess=()=>r(q.result);});const value=await new Promise(r=>{const q=d.transaction('files').objectStore('files').get('bank');q.onsuccess=()=>r(q.result);});d.close();return Array.from(value);});
 await page.locator('#open-emulator').click();await page.waitForURL('**/webapp/emu/#loader-bank=*');assert((await page.locator('#incoming-bank').textContent()).includes('Switch on'));
 await page.locator('#go').click();await page.locator('#incoming-bank').filter({hasText:'Loaded from the sample tool'}).waitFor({timeout:45000});
 async function download(id){const pending=page.waitForEvent('download');await page.locator(id).click();const d=await pending;return fs.readFile(await d.path());}
 const exported=new Uint8Array(await download('#save-bank')),used=inspect(new Uint8Array(original)).used;assert.deepEqual(exported.slice(0,used),new Uint8Array(original).slice(0,used));
 const session=JSON.parse((await download('#save-session')).toString()),validated=validateArchive(session);
 const current=editStep(validated.objects[0].bytes,0,0,{chop:2,velocity:77});session.objects[0]={...session.objects[0],data:b64(current),crc:crc(current)};
 await page.locator('#import-session').setInputFiles({name:'edited.pigeon',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(session))});await page.locator('#sample-status').filter({hasText:'Session loaded'}).waitFor();
 const restored=validateArchive(JSON.parse((await download('#save-session')).toString()));assert.equal(readStep(project(restored.objects[0].bytes),0,0).velocity,77);
 // Expanded bank support must work with the unchanged, published WASM binary.
 const expanded=buildBank([{name:'expanded',pcm:Float32Array.from({length:900000},(_,i)=>Math.sin(i*.1)*.6)},null]);assert(expanded.length>0x67000);
 await page.locator('#import-session').setInputFiles({name:'expanded.kob',mimeType:'application/octet-stream',buffer:Buffer.from(expanded)});await page.locator('#sample-status').filter({hasText:'Bank loaded'}).waitFor({timeout:45000});
 const expandedOut=new Uint8Array(await download('#save-bank'));assert.deepEqual(expandedOut.slice(0,expanded.length),expanded);
 await page.setViewportSize({width:390,height:844});await page.goto(origin+'/webapp/pigeon/');await page.waitForFunction(()=>document.getElementById('offline').textContent==='Offline ready');
 assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));assert.deepEqual(errors,[]);
 console.log('Web release: font dialog, firmware-style waveform, append, Backspace, undo/redo, split, drag reorder, bank handoff, session import/patterns, expanded banks on published WASM, and mobile layout passed.');
}finally{await browser.close();await new Promise(r=>server.close(r));}
