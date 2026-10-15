// SPDX-License-Identifier: GPL-3.0-only
// Real browser: imports/worker, state persistence, cached reload, all features offline.
import assert from "node:assert/strict";
import "./test-editing.mjs";
import http from "node:http";
import fs from "node:fs/promises";
import path from "node:path";
import { crc } from "./bank.js";
import { b64,validateArchive } from "./device.js";
import { fileURLToPath } from "node:url";
const { chromium } = await import(
  process.env.PIGEON_PLAYWRIGHT_MODULE ||
    new URL(
      "../../build/gui-deps/testing/node_modules/playwright/index.mjs",
      import.meta.url,
    ).href
);
const root = path.resolve(
    process.env.PIGEON_WEB_ROOT ||
      fileURLToPath(new URL("../", import.meta.url)),
  ),
  build = fileURLToPath(new URL("../../build/gui-deps/", import.meta.url));
const mime = {
  ".html": "text/html",
  ".js": "text/javascript",
  ".css": "text/css",
  ".json": "application/json",
  ".webmanifest": "application/manifest+json",
  ".svg": "image/svg+xml",
};
let requests = 0;
const requestPaths = [];
const server = http.createServer(async (req, res) => {
  requests++;
  requestPaths.push(req.url);
  try {
    let name = decodeURIComponent(
      new URL(req.url, "http://localhost").pathname,
    );
    if (name.endsWith("/")) name += "index.html";
    const target = path.resolve(root, "." + name);
    if (!target.startsWith(root + path.sep)) throw Error("Invalid path");
    const bytes = await fs.readFile(target);
    res.setHeader(
      "Content-Type",
      mime[path.extname(target)] || "application/octet-stream",
    );
    res.end(bytes);
  } catch {
    res.writeHead(404);
    res.end("Not found");
  }
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const browser = await chromium.launch({ headless: true }),
  context = await browser.newContext({ acceptDownloads: true }),
  page = await context.newPage();
const errors = [];
page.on("pageerror", (e) => errors.push(e.message));
page.on("dialog", (d) => d.accept());
try {
  await page.goto(`http://127.0.0.1:${server.address().port}/pigeon/`);
  await page.waitForFunction(
    () => document.getElementById("offline").textContent === "Offline ready",
  );
  assert.equal(await page.locator("#used").textContent(), "No bank loaded");
  await page.evaluate(() => document.fonts.ready);
  assert.equal(
    await page.evaluate(() => document.fonts.check('12px "Little Character"')),
    true,
  );
  assert.match(
    await page
      .locator("body")
      .evaluate((el) => getComputedStyle(el).fontFamily),
    /Little Character/,
  );
  // Generate a small stereo PCM WAV without relying on OS codecs or ffmpeg.
  const n = 22050,
    raw = Buffer.alloc(44 + n * 4);
  raw.write("RIFF", 0);
  raw.writeUInt32LE(raw.length - 8, 4);
  raw.write("WAVEfmt ", 8);
  raw.writeUInt32LE(16, 16);
  raw.writeUInt16LE(1, 20);
  raw.writeUInt16LE(2, 22);
  raw.writeUInt32LE(22050, 24);
  raw.writeUInt32LE(88200, 28);
  raw.writeUInt16LE(4, 32);
  raw.writeUInt16LE(16, 34);
  raw.write("data", 36);
  raw.writeUInt32LE(n * 4, 40);
  for (let i = 0; i < n; i++) {
    raw.writeInt16LE(Math.round(16000 * Math.sin(i * 0.2)), 44 + i * 4);
    raw.writeInt16LE(Math.round(12000 * Math.sin(i * 0.2)), 46 + i * 4);
  }
  const wav = { name: "hit.wav", mimeType: "audio/wav", buffer: raw };
  await page.locator('#file0').setInputFiles([wav,{...wav,name:'second.wav'}]);
  await page.waitForFunction(()=>document.querySelectorAll('#clips0 .clip-row').length===2);
  await page.locator('#merge0:not([disabled])').waitFor({state:'attached'});
  await page.locator('#songs .clip-builder').first().evaluate(el=>el.open=true);
  await page.getByLabel('Clip 1 end',{exact:true}).fill('0.1');
  await page.getByLabel('Clip 1 gainDb',{exact:true}).fill('-6');await page.getByLabel('Clip 2 gainDb',{exact:true}).fill('-6');
  await page.locator('#clips0 .clip-row').last().getByText('Move up',{exact:true}).click();
  assert((await page.locator('#clips0 .clip-row p').first().textContent()).includes('second.wav'));
  await page.locator('#merge0').click();await page.locator('#apply0').click();
  await page.waitForFunction(()=>document.getElementById('status').textContent.startsWith('Song 1 imported'));
  const joined=await page.evaluate(async()=>{const d=await new Promise(r=>{const q=indexedDB.open('pigeon-library',1);q.onsuccess=()=>r(q.result);});const b=await new Promise(r=>{const q=d.transaction('files').objectStore('files').get('bank');q.onsuccess=()=>r(q.result);});d.close();const {songChops}=await import('/pigeon/editing.js');return songChops(b,1);});
  const gainPeak=await page.evaluate(async()=>{const d=await new Promise(r=>{const q=indexedDB.open('pigeon-library',1);q.onsuccess=()=>r(q.result);}),bank=await new Promise(r=>{const q=d.transaction('files').objectStore('files').get('bank');q.onsuccess=()=>r(q.result);});d.close();const {component}=await import('/pigeon/bank.js'),c=component(bank,1),ac=new AudioContext(),audio=await ac.decodeAudioData(c.song.slice((c.h[12]+1)*4).buffer);let peak=0;for(const n of audio.getChannelData(0))peak=Math.max(peak,Math.abs(n));await ac.close();return peak;});
  assert(gainPeak<.35 && gainPeak>.05,'encoding must preserve clip attenuation instead of normalizing it away');
  assert.equal(joined.length,2);assert.equal(joined[0].a,0);assert.equal(joined[0].b,joined[1].a);assert(joined[1].b>joined[1].a);
  await page.locator('#clips0 .clip-row').first().getByText('Remove',{exact:true}).click();assert.equal(await page.locator('#clips0 .clip-row').count(),1);
  await page.locator("#draft-clear0").click();
  await page.locator("#file0").setInputFiles(wav);
  await page.waitForFunction(() =>
    document.getElementById("status").textContent.startsWith("Files appended"),
  );
  const canvas=page.locator('#wave0');await canvas.scrollIntoViewIfNeeded();const box=await canvas.boundingBox();
  await canvas.click({position:{x:box.width*.4,y:box.height*.4}});
  assert(Number(await canvas.getAttribute('data-cursor'))>0);assert(Number(await page.locator('#start0').inputValue())>.3,'click places an empty selection at the cursor');
  await canvas.hover({position:{x:box.width*.4,y:box.height*.4}});await page.mouse.wheel(0,-180);
  await page.waitForFunction(()=>Number(document.querySelector('#wave0').dataset.zoom)>0);
  await canvas.click({position:{x:box.width*.85,y:box.height*.94}});
  assert(Number(await canvas.getAttribute('data-scroll'))>0,'overview pans the zoomed view');
  await canvas.dblclick({position:{x:box.width*.5,y:box.height*.4}});
  assert.equal(Number(await canvas.getAttribute('data-zoom')),0);
  await page.keyboard.down('Shift');await page.mouse.move(box.x+box.width*.2,box.y+box.height*.4);await page.mouse.down();await page.mouse.move(box.x+box.width*.6,box.y+box.height*.4,{steps:6});await page.mouse.up();await page.keyboard.up('Shift');
  const trimStart=Number(await page.locator('#start0').inputValue()),trimEnd=Number(await page.locator('#end0').inputValue());assert(trimStart>.15&&trimEnd<.65);
  await canvas.focus();await page.keyboard.press('Backspace');
  await page.waitForFunction(()=>Number(document.getElementById('end0').value)<.7);
  assert(Number(await page.locator('#end0').inputValue())>.5,'Backspace removes only the selection');
  await page.keyboard.press('ControlOrMeta+z');
  await page.waitForFunction(()=>Number(document.getElementById('end0').value)>.99);
  assert.equal(await canvas.getAttribute('data-wave-columns'),'220');
  await page.locator('#start0').evaluate(el=>{for(let p=el.parentElement;p;p=p.parentElement)if(p.tagName==='DETAILS')p.open=true;});
  await page.locator('#start0').fill('0');await page.locator('#end0').fill('1');
  await page.locator("#apply0").click();
  await page.waitForFunction(
    () =>
      document
        .getElementById("status")
        .textContent.startsWith("Song 1 imported"),
    null,
    { timeout: 30000 },
  );
  assert.match(await page.locator("#info0").textContent(), /MP3/);
  await page.screenshot({
    path: path.join(build, "gui-desktop.png"),
    fullPage: true,
  });
  await context.setOffline(true);
  await new Promise((r) => server.close(r)); // remove the server entirely as well as disabling the network
  const before = requests;
  await page.locator("#file1").setInputFiles({ ...wav, name: "offline.wav" });
  await page.waitForFunction(() =>
    document
      .getElementById("status")
      .textContent.startsWith("Files appended"),
  );
  await page.locator('#rate1').evaluate(el=>{for(let p=el.parentElement;p;p=p.parentElement)if(p.tagName==='DETAILS')p.open=true;});
  await page.locator("#rate1").selectOption("24");
  await page.locator("#apply1").click();
  await page.waitForFunction(
    () =>
      document
        .getElementById("status")
        .textContent.startsWith("Song 2 imported"),
    null,
    { timeout: 30000 },
  );
  assert.match(await page.locator("#name1").textContent(), /offline/i);
  await page
    .locator("#kit-files")
    .setInputFiles([wav, { ...wav, name: "snare.wav" }]);
  await page.waitForFunction(() =>
    document
      .getElementById("status")
      .textContent.startsWith("Drum kit replaced"),
  );
  // Undo is a complete committed workspace snapshot, including keyboard shortcuts.
  const bankBytes=()=>page.evaluate(async()=>{const d=await new Promise(r=>{const q=indexedDB.open('pigeon-library',1);q.onsuccess=()=>r(q.result);});const b=await new Promise(r=>{const q=d.transaction('files').objectStore('files').get('bank');q.onsuccess=()=>r(q.result);});d.close();return Array.from(b);});
  const originalBank=await bankBytes();
  await page.locator('#rename-song').evaluate(el=>{for(let p=el.parentElement;p;p=p.parentElement)if(p.tagName==='DETAILS')p.open=true;});
  await page.locator('#rename-song').fill('UNDO');await page.locator('#rename-apply').click();
  await page.locator('#status').filter({hasText:'Song renamed locally'}).waitFor();
  const renamedBank=await bankBytes();assert.notDeepEqual(renamedBank,originalBank);
  await page.locator('h1').click();await page.keyboard.press('Control+z');
  await page.locator('#status').filter({hasText:'Local edit undone'}).waitFor();assert.deepEqual(await bankBytes(),originalBank);
  await page.keyboard.press('Control+Shift+z');await page.locator('#status').filter({hasText:'Local edit redone'}).waitFor();assert.deepEqual(await bankBytes(),renamedBank);
  await page.keyboard.press('Meta+z');await page.locator('#status').filter({hasText:'Local edit undone'}).waitFor();assert.deepEqual(await bankBytes(),originalBank);
  await page.keyboard.press('Control+y');await page.locator('#status').filter({hasText:'Local edit redone'}).waitFor();assert.deepEqual(await bankBytes(),renamedBank);
  await page.locator('#undo').click();await page.locator('#status').filter({hasText:'Local edit undone'}).waitFor();
  await page.locator('#library-name').evaluate(el=>{for(let p=el.parentElement;p;p=p.parentElement)if(p.tagName==='DETAILS')p.open=true;});
  await page.locator('#library-name').fill('Offline test');await page.locator('#library-save').click();
  await page.locator('#status').filter({hasText:'Saved in this browser’s library'}).waitFor();
  assert.equal(await page.locator('#library-list option').count(),1);
  await page.locator('#rename-song').fill('ALTERED');await page.locator('#rename-apply').click();await page.locator('#status').filter({hasText:'Song renamed locally'}).waitFor();
  await page.locator('#library-load').click();await page.locator('#status').filter({hasText:'Library entry loaded locally'}).waitFor();assert.deepEqual(await bankBytes(),originalBank);
  await page.locator('#pad-copy-target').selectOption('15');await page.locator('#pad-copy').click();
  await page.locator('#status').filter({hasText:'Kit updated locally'}).waitFor();
  const padCopy=await page.evaluate(async()=>{const d=await new Promise(r=>{const q=indexedDB.open('pigeon-library',1);q.onsuccess=()=>r(q.result);});const b=await new Promise(r=>{const q=d.transaction('files').objectStore('files').get('bank');q.onsuccess=()=>r(q.result);});d.close();const {encodedPad,inspect}=await import('/pigeon/bank.js');return {count:inspect(b).pads,a:encodedPad(b,0),b:encodedPad(b,15)};});
  assert.equal(padCopy.count,16);assert.deepEqual(padCopy.a,padCopy.b);
  await page.locator('#undo').click();await page.locator('#status').filter({hasText:'Local edit undone'}).waitFor();assert.deepEqual(await bankBytes(),originalBank);
  await page.locator("#pad0").click();
  await page.waitForFunction(
    () => document.getElementById("status").textContent === "Playing pad 1",
  );
  await page.locator("#stored0").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent === "Playing stored Song 1",
  );
  // All new editors must work after networking has been disabled.
  await page.locator("#load-chops").click();
  await page.waitForFunction(() =>
    document.getElementById("status").textContent.startsWith("Loaded Song"),
  );
  await page.locator('#chop-attack').evaluate(el=>{for(let p=el.parentElement;p;p=p.parentElement)if(p.tagName==='DETAILS')p.open=true;});
  await page.locator("#chop-attack").fill("50");
  await page.locator("#chop-choke").fill("1");
  await page.locator("#chop-release").fill("200");
  await page.locator("#chop-apply").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent === "Chop updated locally",
  );
  await page.locator("#chop-duplicate").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent === "Chops updated locally",
  );
  await page.locator('#chop-index').selectOption('1');await page.locator('#chop-pitch').fill('7');await page.locator('#chop-start').fill(String(Number(await page.locator('#chop-start').inputValue())+.005));await page.locator('#chop-attack').fill('0');await page.locator('#chop-apply').click();
  await page.locator('#status').filter({hasText:'Chop updated locally'}).waitFor();
  const chopCountBeforeCopy=await page.locator('#chop-index option').count();
  const beforeReassign=await bankBytes();
  for(const action of ['swap','move']) {
    await page.locator('#chop-index').selectOption('1');await page.locator('#chop-copy').evaluate(el=>{for(let p=el.parentElement;p;p=p.parentElement)if(p.tagName==='DETAILS')p.open=true;});
  await page.locator('#chop-copy').selectOption('0');await page.locator(`#chop-${action}`).click();
    await page.locator('#status').filter({hasText:'Chops updated locally'}).waitFor();
    assert.equal(await page.locator('#chop-index').inputValue(),'0');assert.equal(await page.locator('#chop-pitch').inputValue(),'7');
    assert.equal(await page.locator('#chop-index option').count(),chopCountBeforeCopy);
    await page.locator('#undo').click();await page.locator('#status').filter({hasText:'Local edit undone'}).waitFor();assert.deepEqual(await bankBytes(),beforeReassign);
  }
  await page.locator('#chop-index').selectOption('1');
  await page.locator('#chop-copy').selectOption('0');await page.locator('#chop-full').click();
  await page.locator('#status').filter({hasText:'Chops updated locally'}).waitFor();
  const copied=await page.evaluate(async()=>{const d=await new Promise(r=>{const q=indexedDB.open('pigeon-library',1);q.onsuccess=()=>r(q.result);}),image=await new Promise(r=>{const q=d.transaction('files').objectStore('files').get('bank');q.onsuccess=()=>r(q.result);});d.close();const {songChops}=await import('/pigeon/editing.js');return songChops(image,1);});
  assert.equal(copied.length,chopCountBeforeCopy);assert.deepEqual(copied[1],copied[0]);assert.equal(await page.locator('#chop-index').inputValue(),'1');assert.equal(await page.locator('#chop-pitch').inputValue(),'0');
  await page.locator("#pad-pitch").fill("5");
  await page.locator("#pad-label").fill("Kick");
  await page.locator("#pad-settings-apply").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent ===
      "Pad settings updated locally",
  );
  assert.equal(await page.locator("#padname0").textContent(), "Kick");
  await page.locator("#pad-right").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent === "Kit updated locally",
  );
  assert.equal(await page.locator("#padname1").textContent(), "Kick");
  await page.locator("#pad-remove").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent === "Kit updated locally",
  );
  const project = await fs.readFile(
    path.join(build, "../expansion-project.funa"),
  );
  const bank = await fs.readFile(path.join(build, "dual-kit.kob"));
  const objects = Array.from({ length: 10 }, (_, id) => {
    const bytes = id === 0 || (id >= 2 && id <= 9) ? project : Uint8Array.of(2);
    return { id, size: bytes.length, crc: crc(bytes), data: b64(bytes) };
  });
  const session = {
    format: "pigeon-archive",
    version: 1,
    objects,
    bank: b64(bank),
    bank_crc: crc(bank),
  };
  await page.locator("#open").setInputFiles({
    name: "test.pigeon",
    mimeType: "application/json",
    buffer: Buffer.from(JSON.stringify(session)),
  });
  await page.waitForFunction(() =>
    document.getElementById("status").textContent.startsWith("Session opened"),
  );
  await page.locator("#pattern-length").fill("32");
  await page.locator("#pattern-length-apply").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent ===
      "Pattern length updated locally",
  );
  assert.equal(await page.locator("#pattern-grid button").count(), 32);
  await page.locator("#pattern-chop").fill("7");
  await page.locator("#pattern-probability").fill("70");
  await page.locator("#pattern-repeats").fill("1");
  await page.locator("#pattern-pitch").fill("5");
  await page.locator("#pattern-nudge").fill("-2");
  await page.locator("#pattern-condition").selectOption("2");
  await page.locator("#pattern-apply").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent ===
      "Pattern step updated locally",
  );
  assert.match(
    await page.locator("#pattern-grid button").first().textContent(),
    /7/,
  );
  await page.locator('#pattern-chop').fill('9');await page.locator('#pattern-apply').click();
  await page.locator('#status').filter({hasText:'Pattern step updated locally'}).waitFor();
  await page.locator('#undo').click();await page.locator('#status').filter({hasText:'Local edit undone'}).waitFor();
  assert.match(await page.locator('#pattern-grid button').first().textContent(),/7/);
  await page.locator('#library-name').fill('Full session');await page.locator('#library-save').click();
  await page.locator('#status').filter({hasText:'Saved in this browser’s library'}).waitFor();
  await page.locator('#library-list').selectOption({label:'Full session (session)'});
  const libraryDownload=page.waitForEvent('download');await page.locator('#library-download').click();
  const libraryFile=await libraryDownload;assert.equal(libraryFile.suggestedFilename(),'Full session.pigeon');
  validateArchive(JSON.parse(await fs.readFile(await libraryFile.path(),'utf8')));
  await page
    .locator("#firmware-file")
    .setInputFiles(path.join(build, "../pigeon.fwsc"));
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent ===
      "Firmware package validated locally",
  );
  const downloaded = page.waitForEvent("download");
  await page.locator("#save").click();
  const download = await downloaded;
  await download.saveAs(path.join(build, "browser.kob"));
  await page.reload();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent ===
      "Restored your last local bank.",
  );
  await page.waitForFunction(
    () => document.getElementById("offline").textContent === "Offline ready",
  );
  assert.equal(await page.locator("#pattern-grid button").count(), 32);
  assert.match(
    await page.locator("#pattern-grid button").first().textContent(),
    /7/,
  );
  assert.equal(await page.locator("#pattern-pitch").inputValue(), "5");
  assert.equal(await page.locator("#pattern-nudge").inputValue(), "-2");
  assert.equal(await page.locator("#pattern-condition").inputValue(), "2");
  assert.equal(requests, before, JSON.stringify(requestPaths.slice(before)));
  // Compare snapshots after reload and inspect responsive overflow.
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({
    path: path.join(build, "gui-mobile.png"),
    fullPage: true,
  });
  assert.equal(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
    true,
  );
  assert.deepEqual(errors, []);
  console.log(
    "PIGEON browser: import/encoding, waveform/chop/pad/pattern editing, names, firmware validation, export and reload offline, persistence, and responsive layout passed",
  );
} finally {
  await browser.close();
  await new Promise((r) => server.close(r));
}
