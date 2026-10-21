// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
import {chromium} from '../../build/gui-deps/testing/node_modules/playwright/index.mjs';

// Exercise current page sources against the locally built firmware.
const published = process.env.PIGEON_RECORDING_PUBLISHED === '1';
const root = path.resolve(published ? 'web/site/build/pages' : 'web');
const server = http.createServer(async (req, res) => {
  try {
    const name = new URL(req.url, 'http://localhost').pathname;
    const asset = published ? name : name.replace(/^\/webapp\//, '/');
    let file = path.resolve(root, '.' + (asset.endsWith('/') ? asset + 'index.html' : asset));
    if (!file.startsWith(root + '/')) throw Error();
    if (!published && asset === '/nixie.js') file = path.resolve('web/site/build/pages/nixie.js');
    if (!published && asset === '/emu/felucca.wasm') file = path.resolve('build/emu/felucca.wasm');
    if (!published && asset.startsWith('/emu/words/')) file = path.resolve('web/words', asset.slice('/emu/words/'.length));
    const data = await fs.readFile(file);
    res.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : file.endsWith('.wasm') ? 'application/wasm' : file.endsWith('.html') ? 'text/html' : 'application/octet-stream');
    res.end(data);
  } catch { res.writeHead(404); res.end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const browser = await chromium.launch({headless: true});
try {
  const page = await browser.newPage();
  await page.route('**/api/trials', route => route.fulfill({json: {total: 1}}));
  await page.goto(`http://127.0.0.1:${server.address().port}/webapp/emu/`);
  assert(await page.locator('#record-start').isDisabled());
  await page.locator('#go').click();
  await page.locator('#start').waitFor({state: 'hidden'});
  // Feed a known tone through the actual emulator's sample bank.
  const count = 22050, wav = Buffer.alloc(44 + count * 2);
  wav.write('RIFF'); wav.writeUInt32LE(wav.length - 8, 4); wav.write('WAVEfmt ', 8);
  wav.writeUInt32LE(16, 16); wav.writeUInt16LE(1, 20); wav.writeUInt16LE(1, 22);
  wav.writeUInt32LE(22050, 24); wav.writeUInt32LE(44100, 28);
  wav.writeUInt16LE(2, 32); wav.writeUInt16LE(16, 34); wav.write('data', 36); wav.writeUInt32LE(count * 2, 40);
  for (let i = 0; i < count; i++) wav.writeInt16LE(Math.round(Math.sin(i * 0.1) * 20000), 44 + i * 2);
  await page.locator('#song1-file').setInputFiles({name: 'tone.wav', mimeType: 'audio/wav', buffer: wav});
  await page.locator('#song1-editor .apply-section').click();
  await page.getByText('Samples loaded. Play the keys to try them.', {exact: true}).waitFor();
  await page.waitForFunction(() => Number(document.getElementById('screen').dataset.deviceTime) > 3500);
  await page.locator('#record-start').click();
  assert(await page.locator('#record-start').isDisabled());
  await page.locator('#record-start').blur();
  await page.keyboard.down('z');
  await page.waitForTimeout(1200);
  await page.keyboard.up('z');
  await page.locator('#record-stop').click();
  await page.locator('#record-download').waitFor({state: 'visible'});
  const result = await page.evaluate(async () => {
    const link = document.getElementById('record-download');
    const blob = await (await fetch(link.href)).blob();
    const context = new AudioContext();
    const audio = await context.decodeAudioData(await blob.arrayBuffer());
    const peak = audio.getChannelData(0).reduce((a, v) => Math.max(a, Math.abs(v)), 0);
    await context.close();
    return {size: blob.size, channels: audio.numberOfChannels, duration: audio.duration, peak, filename: link.download};
  });
  assert(result.size > 100, JSON.stringify(result)); assert.equal(result.channels, 2);
  assert(result.duration > 1); assert(result.peak > 0.01, JSON.stringify(result));
  assert.match(result.filename, /^pigeon-recording-.*\.wav$/);
  const downloadEvent = page.waitForEvent('download');
  await page.locator('#record-download').click();
  assert.equal((await downloadEvent).suggestedFilename(), result.filename);
  await page.locator('#record-start').click();
  assert(await page.locator('#record-download').isHidden());
  await page.waitForTimeout(150);
  await page.locator('#record-stop').click();
  await page.locator('#record-download').waitFor({state: 'visible'});
  assert(await page.locator('#record-start').isEnabled());
  console.log('Recording: stereo audio, non-silent output, download, and repeated takes passed.');
} finally { await browser.close(); server.close(); }
