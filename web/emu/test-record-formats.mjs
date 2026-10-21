// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import http from 'node:http';
import fs from 'node:fs/promises';
import path from 'node:path';
const {chromium} = await import(process.env.PLAYWRIGHT_MODULE || '../../build/gui-deps/testing/node_modules/playwright/index.mjs');
const published = process.env.PIGEON_RECORDING_PUBLISHED === '1';
const root = path.resolve(published ? 'web/site/build/pages' : 'web');
const emuPath = published ? '/webapp/emu/index.html' : '/emu/index.html';
const server = http.createServer(async (req, res) => {
  try {
    const file = path.resolve(root, '.' + new URL(req.url, 'http://localhost').pathname);
    if (!file.startsWith(root + '/')) throw Error();
    res.setHeader('Content-Type', file.endsWith('.js') ? 'text/javascript' : 'text/html');
    res.end(await fs.readFile(file));
  } catch { res.writeHead(404); res.end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const browser = await chromium.launch({headless: true, ...(process.env.CHROME_PATH ? {executablePath: process.env.CHROME_PATH} : {})});
try {
  const page = await browser.newPage();
  page.setDefaultTimeout(30000);
  page.on('pageerror', error => console.error('Recording page error:', error.message));
  await page.goto(`http://127.0.0.1:${server.address().port}${emuPath}`);
  // Exercise the page's real recording controls with a deterministic stereo source,
  // independently of firmware builds or stored sample banks.
  const html = await fs.readFile(path.join(root, emuPath), 'utf8');
  const code = html.slice(html.indexOf('const recordStart ='), html.indexOf('// ------------------------------------------------------------------ switching on'));
  await page.evaluate(async code => {
    const audioContext = new AudioContext({sampleRate: 44100});
    const node = audioContext.createChannelMerger(2);
    const left = new OscillatorNode(audioContext, {frequency: 440});
    const right = new OscillatorNode(audioContext, {frequency: 880});
    left.connect(node, 0, 0); right.connect(node, 0, 1); left.start(); right.start();
    await new Function('audioContext', 'node', `${code};return enableRecording();`)(audioContext, node);
  }, code);
  for (let take = 0; take < 2; take++) {
    await page.locator('#record-start').click();
    assert(await page.locator('#record-download').isHidden());
    assert(await page.locator('#record-download-mp3').isHidden());
    await page.waitForTimeout(600);
    await page.locator('#record-stop').click();
    try { await page.locator('#record-download-mp3').waitFor({state: 'visible'}); }
    catch (error) { console.error('Recording status:', await page.locator('#record-status').textContent()); throw error; }
    for (const [id, ext] of [['record-download', 'wav'], ['record-download-mp3', 'mp3']]) {
      const result = await page.evaluate(async id => {
        const link = document.getElementById(id), bytes = await (await fetch(link.href)).arrayBuffer();
        const context = new AudioContext(), audio = await context.decodeAudioData(bytes.slice(0));
        const channels = Array.from({length: audio.numberOfChannels}, (_, c) => {
          const values = audio.getChannelData(c);
          const power = frequency => {
            let real = 0, imaginary = 0;
            for (let i = 0; i < values.length; i++) {
              const angle = 2 * Math.PI * frequency * i / audio.sampleRate;
              real += values[i] * Math.cos(angle); imaginary += values[i] * Math.sin(angle);
            }
            return real * real + imaginary * imaginary;
          };
          return {low: power(440), high: power(880), peak: values.reduce((p, v) => Math.max(p, Math.abs(v)), 0),
            crossings: values.reduce((n, v, i) => n + (i > 0 && v > 0 && values[i - 1] <= 0 ? 1 : 0), 0)};
        });
        await context.close();
        return {filename: link.download, duration: audio.duration, channels, magic: new TextDecoder().decode(bytes.slice(0, 4))};
      }, id);
      assert(result.filename.endsWith('.' + ext)); assert(result.duration > 0.3);
      assert.equal(result.channels.length, 2); assert(result.channels.every(c => c.peak > 0.1));
      assert(result.channels[0].low > result.channels[0].high * 10, JSON.stringify(result));
      assert(result.channels[1].high > result.channels[1].low * 10, JSON.stringify(result));
      if (ext === 'wav') assert.equal(result.magic, 'RIFF');
      const download = page.waitForEvent('download'); await page.locator('#' + id).click();
      assert.equal((await download).suggestedFilename(), result.filename);
    }
    assert(await page.locator('#record-start').isEnabled());
  }
  console.log('WAV and MP3: stereo separation, decoded audio, downloads, and repeated takes passed.');
} finally { await browser.close(); await new Promise(resolve => server.close(resolve)); }
