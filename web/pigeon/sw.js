// SPDX-License-Identifier: GPL-3.0-only
// Bump this version whenever a shipped runtime asset changes.
const CACHE = "pigeon-library-v20";
const ASSETS = [
  "./",
  "index.html",
  "install-count.js",
  "style.css",
  "fonts/LittleCharacterRegular.ttf",
  "fonts/LittleCharacter-OFL.txt",
  "app.js?v=20",
  "bank.js",
  "bank-handoff.js",
  "device.js",
  "editing.js",
  "editors.js",
  "wave-editor.js",
  "../fm1pkg.js",
  "../fm1ota.js",
  "encode-worker.js",
  "manifest.webmanifest",
  "icon.svg",
  "vendor/lame.js",
  "../ko_bank.js",
  "../fm1backup.js",
  "vendor/README.md",
  "vendor/lamejs-NOTICE.txt",
  "vendor/LGPL-3.0.txt",
  "vendor/GPL-3.0.txt",
  "vendor/lamejs-1.2.1-source.tgz",
];
self.addEventListener("install", (e) =>
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(ASSETS))),
);
// Do not forcibly replace an active tool during a transfer. A fresh page uses the new cache.
self.addEventListener("activate", (e) =>
  e.waitUntil(
    (async () => {
      for (const name of await caches.keys())
        if (name.startsWith("pigeon-library-") && name !== CACHE)
          await caches.delete(name);
      await self.clients.claim();
    })(),
  ),
);
self.addEventListener("fetch", (e) => {
  if (e.request.method !== "GET") return;
  const url = new URL(e.request.url),
    allowed = new Set(ASSETS.map((p) => new URL(p, self.location.href).href));
  if (url.origin === self.location.origin && allowed.has(url.href))
    e.respondWith(
      caches
        .open(CACHE)
        .then(async (c) => (await c.match(e.request)) || fetch(e.request)),
    );
});
self.addEventListener("message", (e) => {
  if (e.data === "CHECK_OFFLINE")
    e.waitUntil(
      (async () => {
        const c = await caches.open(CACHE),
          ready = (await Promise.all(ASSETS.map((p) => c.match(p)))).every(
            Boolean,
          );
        e.ports[0]?.postMessage({ ready });
      })(),
    );
});
