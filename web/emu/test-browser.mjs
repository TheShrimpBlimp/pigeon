// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
import {chromium} from '../../build/gui-deps/testing/node_modules/playwright/index.mjs';
const root=path.resolve('build/emu');
const server=http.createServer(async(req,res)=>{try{const name=new URL(req.url,'http://localhost').pathname;const p=path.resolve(root,'.'+(name.endsWith('/')?name+'index.html':name));if(!p.startsWith(root+'/'))throw Error();const data=await fs.readFile(p);res.setHeader('Content-Type',p.endsWith('.js')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(data);}catch{res.writeHead(404);res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({headless:true});
try{
 const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto(`http://127.0.0.1:${server.address().port}/`);
 await page.locator('#go').click();await page.locator('#start').waitFor({state:'hidden'});
 await page.waitForFunction(()=>Number(document.getElementById("screen").dataset.deviceTime)>1000);
 await page.evaluate(()=>{
   const input={id:'fm1',name:'FM-1',state:'connected',onmidimessage:null};window.testInput=input;window.sent=[];
   const output={name:'FM-1',state:'connected',send(d){window.sent.push(d);if(d[5]===1)setTimeout(()=>input.onmidimessage?.({data:Uint8Array.from([240,125,70,76,110,0,1,247])}),0);}};
   navigator.requestMIDIAccess=async()=>({inputs:new Map([['fm1',input]]),outputs:new Map([['fm1',output]])});
 });
 await page.locator('#midi-connect').click();await page.getByText('FM1 connected · full panel controller',{exact:true}).waitFor();
 await page.evaluate(()=>window.testInput.onmidimessage({data:Uint8Array.from([240,125,70,76,111,1,0,8,1,0,0,0,64,64,64,64,64,64,64,60,5,247])}));
 await page.waitForTimeout(150);
 await page.locator('#midi-disconnect').click();assert(await page.evaluate(()=>window.sent.some(d=>d[5]===0)));
 for(const width of [1280,390]){await page.setViewportSize({width,height:900});assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await page.screenshot({path:`build/pigeon-emulator-${width}.png`,fullPage:true});}
 assert.deepEqual(errors,[]);console.log('Browser: AudioWorklet boot, full-panel handshake, panel events, disconnect, desktop/mobile passed.');
}finally{await browser.close();await new Promise(r=>server.close(r));}
