// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';import http from 'node:http';import fs from 'node:fs/promises';import path from 'node:path';
const {chromium}=await import(process.env.PIGEON_PLAYWRIGHT_MODULE||'../build/gui-deps/testing/node_modules/playwright/index.mjs');
const base=path.resolve('.'),fallback=process.env.PIGEON_SITE_ASSETS||path.resolve('web/site/dist/client');
const mime={'.js':'text/javascript','.wasm':'application/wasm','.html':'text/html','.css':'text/css','.json':'application/json','.ttf':'font/ttf'};
const server=http.createServer(async(req,res)=>{try{
 const url=new URL(req.url,'http://localhost'),name=url.pathname;
 if(name==='/api/trials'){res.setHeader('Content-Type','application/json');res.end('{"total":0}');return;}
 let relative=name.startsWith('/webapp/emu/')?'build/emu/'+name.slice(12):name.startsWith('/webapp/pigeon/')?'web/pigeon/'+name.slice(15):name.startsWith('/webapp/')?'web/'+name.slice(8):'web/'+name.slice(1);
 if(relative.endsWith('/'))relative+='index.html';let file=path.resolve(base,relative);
 if(!file.startsWith(base+'/'))throw Error('path');
 let data;try{data=await fs.readFile(file);}catch(e){if(!fallback)throw e;file=path.join(fallback,path.basename(name));data=await fs.readFile(file);}
 res.setHeader('Content-Type',mime[path.extname(file)]||'application/octet-stream');res.end(data);
 }catch{res.writeHead(404);res.end();}});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const browser=await chromium.launch({headless:true,args:['--autoplay-policy=no-user-gesture-required'],executablePath:process.env.PIGEON_CHROMIUM});
try{
 const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('console',m=>{if(m.type()==='error')console.error('browser:',m.text());});page.on('requestfailed',r=>console.error('request failed',r.url(),r.failure()));
 await page.goto(`http://127.0.0.1:${server.address().port}/webapp/emu/`);
 assert.equal(await page.locator('#reader-enable').isChecked(),false);
 await page.locator('#go').click();try{await page.locator('#start').waitFor({state:'hidden'});}catch(error){console.error('Start status:',await page.locator('#status').textContent(),'Page errors:',errors,'Start button:',await page.locator('#go').textContent(),'disabled:',await page.locator('#go').isDisabled());throw error;}
 await page.waitForFunction(()=>Number(document.getElementById('screen').dataset.deviceTime)>3200);
 await page.locator('#reader-enable').check();
 await page.waitForFunction(()=>/No song loaded/i.test(document.getElementById('reader-transcript').textContent));
 assert.equal(await page.locator('#reader-live').getAttribute('aria-live'),'polite');
 assert.equal(await page.locator('#reader-alert').getAttribute('role'),'alert');
 await page.getByRole('button',{name:'FX',exact:true}).click();
 await page.waitForFunction(()=>/effects amounts/i.test(document.getElementById('reader-transcript').textContent));
 await page.locator('#reader-where').click();
 await page.waitForFunction(()=>/Knob 2, delay/i.test(document.getElementById('reader-transcript').textContent));
 await page.waitForFunction(()=>/Knob 4, reverb/i.test(document.getElementById('reader-live').textContent));
 assert.match(await page.locator('#reader-live').textContent(),/Knob 1, filter/i);
 assert(!/Edit resets effects/i.test(await page.locator('#reader-transcript').textContent()));
 await page.locator('#reader-enable').uncheck();assert(await page.locator('#reader-where').isDisabled());
 assert.deepEqual(errors,[]);console.log('PIGEON reader page: real AudioWorklet, local dictionary, live regions, page announcements, WHERE and opt-out passed.');
}finally{await browser.close();await new Promise(resolve=>server.close(resolve));}
