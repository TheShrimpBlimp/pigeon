// SPDX-License-Identifier: GPL-3.0-only
import {fileURLToPath} from 'node:url';
import assert from 'node:assert/strict';import http from 'node:http';import fs from 'node:fs/promises';import path from 'node:path';
const {chromium}=await import(process.env.PIGEON_PLAYWRIGHT_MODULE||new URL('../../repositories/pigeon-tool/node_modules/playwright/index.mjs',import.meta.url).href);
const root=fileURLToPath(new URL('../',import.meta.url));
const server=http.createServer(async(req,res)=>{try{let name=new URL(req.url,'http://localhost').pathname;if(name.endsWith('/'))name+='index.html';const f=path.resolve(root,'.'+name);if(!f.startsWith(root))throw Error();const data=await fs.readFile(f);res.setHeader('Content-Type',f.endsWith('.js')?'text/javascript':f.endsWith('.css')?'text/css':f.endsWith('.html')?'text/html':'application/octet-stream');res.end(data);}catch{res.writeHead(404);res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({headless:true,executablePath:process.env.PIGEON_CHROMIUM});
const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
async function bankInfo(){return page.evaluate(async()=>{const db=await new Promise(r=>{const q=indexedDB.open('pigeon-library',1);q.onsuccess=()=>r(q.result);});const bank=await new Promise(r=>{const q=db.transaction('files').objectStore('files').get('bank');q.onsuccess=()=>r(q.result);});db.close();const {crc,component,inspect}=await import('/pigeon/bank.js'),{songChops}=await import('/pigeon/editing.js');return {crc:crc(bank),first:crc(component(bank,1).song),second:component(bank,2)?crc(component(bank,2).song):null,pads:inspect(bank).pads,chops2:component(bank,2)?songChops(bank,2):[]};});}
try{
 await page.goto(`http://127.0.0.1:${server.address().port}/pigeon/`);
 await page.waitForFunction(()=>!document.querySelector('#open').disabled);
 await page.locator('#open').setInputFiles(fileURLToPath(new URL('../../build/ko-fixtures/minute-mp3.kob',import.meta.url)));
 await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('opened locally'));
 const before=await bankInfo();await page.locator('#collect-chops').selectOption(['0','1']);await page.locator('#collect-name').fill('First finds');await page.locator('#collect-save').click();
 await page.waitForFunction(()=>document.querySelector('#collect-status').textContent.includes('Collected 2'));
 assert.equal((await bankInfo()).crc,before.crc,'collecting must not change the open bank');
 await page.locator('#open').setInputFiles(fileURLToPath(new URL('../../build/ko-fixtures/dual.kob',import.meta.url)));
 await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('opened locally'));
 await page.locator('#collect-song').selectOption('2');await page.locator('#collect-chops').selectOption('0');await page.locator('#collect-name').fill('Second song');await page.locator('#collect-save').click();
 await page.waitForFunction(()=>document.querySelector('#collect-status').textContent.includes('Collected 1'));
 assert.equal(await page.locator('#collect-saved option').count(),3);
 await page.reload();await page.waitForFunction(()=>document.querySelector('#collect-saved').options.length===3&&!document.querySelector('#collect-add').disabled);
 const values=await page.locator('#collect-saved option').evaluateAll(es=>es.map(e=>e.value));
 await page.locator('#collect-saved').selectOption([values[0],values[2]]);await page.locator('#collect-add').click();
 assert.equal(await page.locator('#collect-order option').count(),2);
 await page.locator('#collect-order').selectOption('1');await page.locator('#collect-up').click();assert.match(await page.locator('#collect-order option').first().textContent(),/Second song/);
 await page.locator('#collect-target').selectOption('2');await page.locator('#collect-quality').selectOption('24');const previous=await bankInfo();await page.locator('#collect-build').click();
 await page.waitForFunction(()=>document.querySelector('#collect-status').textContent.includes('into Song 2 locally'));
 const built=await bankInfo();assert.equal(built.first,previous.first,'building Song 2 must preserve Song 1');assert.equal(built.chops2.length,2);assert.equal(await page.locator('#collect-saved option').count(),3);
 await page.locator('#undo').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('Local edit undone'));assert.equal((await bankInfo()).crc,previous.crc);
 await page.locator('#collect-saved').selectOption(values[0]);await page.locator('#collect-preview').click();await page.waitForFunction(()=>document.querySelector('#collect-status').textContent.includes('Previewing'));await page.locator('#collect-stop').click();
 const event=page.waitForEvent('download');await page.locator('#collect-download').click();const download=await event;const file=await download.path(),data=JSON.parse(await fs.readFile(file,'utf8'));assert.equal(data.format,'pigeon-chops');assert.equal(data.chops.length,2);
 await page.locator('#collect-import').setInputFiles(file);await page.waitForFunction(()=>document.querySelector('#collect-saved').options.length===5);
 await page.locator('#collect-target').selectOption('drums');await page.locator('#collect-build').click();await page.waitForFunction(()=>document.querySelector('#collect-status').textContent.includes('into the drum kit locally'));const kit=await bankInfo();assert.equal(kit.pads,2);assert.equal(kit.first,previous.first);assert.equal(kit.second,previous.second);
 assert.equal(await page.locator('#collect-saved option').count(),5,'building must keep collected sources');
 const imported=await page.locator('#collect-saved option').last().getAttribute('value');await page.locator('#collect-saved').selectOption(imported);await page.locator('#collect-delete').click();await page.waitForFunction(()=>document.querySelector('#collect-saved').options.length===3);
 await page.setViewportSize({width:390,height:844});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1),'mobile layout must not overflow');await page.setViewportSize({width:1280,height:900});
 await page.locator('#collect-song').selectOption('1');await page.locator('#collect-chops').selectOption('0');
 const untrimmed=await bankInfo();await page.locator('#collect-trim').click();
 await page.waitForFunction(()=>document.querySelector('#collect-status').textContent.includes('Kept 1 chops in Song 1'));
 const trimmed=await bankInfo();assert.notEqual(trimmed.first,untrimmed.first);assert.equal(trimmed.second,untrimmed.second);assert.equal(trimmed.pads,untrimmed.pads);
 await page.locator('#undo').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('Local edit undone'));assert.equal((await bankInfo()).crc,untrimmed.crc);
 assert.deepEqual(errors,[]);await page.locator('#chop-collection').screenshot({path:fileURLToPath(new URL('../../build/chop-library-browser.png',import.meta.url))});
 console.log('Chop library browser: collect multiple songs, persistence, ordered assembly, preserved other slot, undo, preview, export/import, kit build and delete passed.');
}catch(error){console.error('Browser errors:',errors,'Status:',await page.locator('#status').textContent().catch(()=>''),'Library:',await page.locator('#collect-status').textContent().catch(()=>''));throw error;}finally{await browser.close();server.close();}
