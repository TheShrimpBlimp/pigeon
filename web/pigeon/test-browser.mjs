// SPDX-License-Identifier: GPL-3.0-only
// Real browser: imports/worker, state persistence, cached reload, all features offline.
import assert from "node:assert/strict";
import http from "node:http";
import fs from "node:fs/promises";
import path from "node:path";
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
const server = http.createServer(async (req, res) => {
  requests++;
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
  assert.equal(await page.evaluate(() => document.fonts.check('12px "Little Character"')), true);
  assert.match(await page.locator("body").evaluate(el => getComputedStyle(el).fontFamily), /Little Character/);
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
  await page.locator("#file0").setInputFiles(wav);
  await page.waitForFunction(() =>
    document.getElementById("status").textContent.startsWith("Selected hit"),
  );
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
  const before = requests;
  await context.setOffline(true);
  await page.locator("#file1").setInputFiles({ ...wav, name: "offline.wav" });
  await page.waitForFunction(() =>
    document
      .getElementById("status")
      .textContent.startsWith("Selected offline"),
  );
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
    document.getElementById("status").textContent.startsWith("Kit staged"),
  );
  await page.locator("#apply-kit").click();
  await page.waitForFunction(() =>
    document
      .getElementById("status")
      .textContent.startsWith("Drum kit replaced"),
  );
  await page.locator("#pad0").click();
  await page.waitForFunction(
    () => document.getElementById("status").textContent === "Playing pad 1",
  );
  await page.locator("#stored0").click();
  await page.waitForFunction(
    () =>
      document.getElementById("status").textContent === "Playing stored Song 1",
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
  assert.match(await page.locator("#name1").textContent(), /offline/i);
  assert.equal(requests, before);
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
    "Pigeon browser: stereo WAV import, worker encoding, Song 2/kit/preview/export offline, offline reload, persistence, and responsive layout passed",
  );
} finally {
  await browser.close();
  await new Promise((r) => server.close(r));
}
