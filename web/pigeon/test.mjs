// SPDX-License-Identifier: GPL-3.0-only
import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";
import {
  normalize,
  imaEncode,
  imaDecode,
  onsets,
  indexedMp3,
  makeSong,
  repack,
  replaceKit,
  inspect,
  component,
  padAudio,
  encodedPad,
  join,
  crc,
} from "./bank.js";
import { Device, b64, validateArchive } from "./device.js";
import { bkPack, bkUnpack, bkU32, bkR32, BACKUP_IDS } from "../fm1backup.js";
fs.mkdirSync(new URL("../../build/gui-deps/", import.meta.url), {
  recursive: true,
});
const sandbox = { console };
vm.createContext(sandbox);
vm.runInContext(
  fs.readFileSync(new URL("vendor/lame.js", import.meta.url), "utf8"),
  sandbox,
);
const x = Int16Array.from({ length: 22050 }, (_, i) =>
  Math.round(18000 * Math.sin(i * 0.13)),
);
function song(br = 32) {
  const encoder = new sandbox.lamejs.Mp3Encoder(1, 22050, br),
    chunks = [];
  for (let i = 0; i < x.length; i += 1152)
    chunks.push(new Uint8Array(encoder.encodeBuffer(x.subarray(i, i + 1152))));
  chunks.push(new Uint8Array(encoder.flush()));
  const raw = join(chunks);
  return makeSong(x, indexedMp3(raw, x.length), "TEST");
}
for (const br of [24, 32, 40, 48, 56, 64]) {
  const a = song(br);
  assert.equal(inspect(a).songs[0].fields[4], 2);
  fs.writeFileSync(
    new URL(`../../build/gui-deps/song-${br}.kob`, import.meta.url),
    a,
  );
}
const a = song(),
  metadata = Uint8Array.from({ length: 4096 }, (_, i) => i & 255),
  first = join([a, metadata]),
  second = song(64),
  dual = repack(first, second, 2);
assert.deepEqual(component(dual, 1).song, component(first, 1).song);
assert.deepEqual(dual.slice(inspect(dual).used), metadata);
const changed = repack(dual, second, 1);
assert.deepEqual(component(changed, 2).song, component(dual, 2).song);
const kit = replaceKit(dual, [x, x.subarray(0, 501)]);
fs.writeFileSync(
  new URL("../../build/gui-deps/dual-kit.kob", import.meta.url),
  kit,
);
fs.writeFileSync(
  new URL("../../build/gui-deps/codec-vector.json", import.meta.url),
  JSON.stringify({
    samples: [...x],
    adpcm: [...imaEncode(x)],
    onsets: onsets(x),
  }),
);
assert.equal(inspect(kit).pads, 2);
assert.deepEqual(component(kit, 1).song, component(dual, 1).song);
assert.deepEqual(component(kit, 2).song, component(dual, 2).song);
assert.equal(padAudio(kit, 1).length, 501);
assert.equal(inspect(replaceKit(kit, [])).pads, 0);
const onePadChanged = replaceKit(kit, [x.subarray(0, 100), encodedPad(kit, 1)]);
assert.deepEqual(encodedPad(onePadChanged, 1), encodedPad(kit, 1));
assert.equal(imaEncode(Int16Array.of(0, 100, -100, 1000, -1000)).length, 3);
assert.equal(imaDecode(imaEncode(x), x.length).length, x.length);
assert.equal(normalize(Float32Array.of(-1, 0, 1))[2], 30000);
assert.deepEqual(onsets(new Int16Array(1000)), [0]);
assert.throws(() => replaceKit(dual, Array(17).fill(x)));
assert.throws(() => replaceKit(dual, [new Int16Array(1000000)]));
const corrupt = dual.slice();
corrupt[4096] ^= 1;
assert.throws(() => inspect(corrupt));
const objects = Array.from({ length: 10 }, (_, id) => {
  const bytes = Uint8Array.of(id + 1);
  return { id, size: 1, crc: crc(bytes), data: b64(bytes) };
});
const archive = {
  format: "pigeon-archive",
  version: 1,
  objects,
  bank: b64(kit),
  bank_crc: crc(kit),
};
assert.deepEqual(validateArchive(archive).bank, kit);
const bad = structuredClone(archive);
bad.objects[2].crc ^= 1;
let contacted = false;
await assert.rejects(
  new Device(async () => {
    contacted = true;
  }).restore(bad),
);
assert.equal(contacted, false);
let flash = kit.slice(),
  staged = null,
  logs = [],
  objs = new Map(objects.map((o) => [o.id, Uint8Array.of(o.id + 1)]));
const u21 = (n) => [n & 127, (n >>> 7) & 127, (n >>> 14) & 127],
  r21 = (a) => a[0] | (a[1] << 7) | (a[2] << 14);
async function request([cmd, p]) {
  logs.push([cmd, ...p.slice(0, 2)]);
  if (cmd === 90) return [75, 3, ...u21(0x67000), ...u21(4096), 0, 3];
  if (cmd === 94) {
    const off = r21(p),
      n = p[3] | (p[4] << 7),
      used = new DataView(flash.buffer).getUint32(8, true);
    return [
      0,
      ...bkPack(
        flash.subarray(
          off >= 0x67000 ? used + off - 0x67000 : off,
          (off >= 0x67000 ? used + off - 0x67000 : off) + n,
        ),
      ),
    ];
  }
  if (cmd === 91) {
    staged = new Uint8Array(r21(p) + 4096);
    staged.fill(255);
    return [0];
  }
  if (cmd === 92) {
    const bytes = bkUnpack(
      p.slice(3),
      p.length - 3 - Math.ceil((p.length - 3) / 8),
    );
    staged.set(bytes, r21(p));
    return [0];
  }
  if (cmd === 93) {
    flash = staged;
    return [0];
  }
  if (cmd === 95) {
    staged.set(
      bkUnpack(p.slice(3), 256),
      new DataView(staged.buffer).getUint32(8, true) + r21(p),
    );
    return [0];
  }
  if (cmd === 65) {
    const result = [1, 0, 13];
    for (const id of BACKUP_IDS) {
      const bytes = objs.get(id) || new Uint8Array();
      result.push(id, ...bkU32(bytes.length), ...bkU32(crc(bytes)));
    }
    return result;
  }
  if (cmd === 66) {
    const off = bkR32(p, 1),
      n = p[6] | (p[7] << 7);
    return [
      p[0],
      0,
      ...bkU32(off),
      n & 127,
      n >>> 7,
      ...bkPack(objs.get(p[0]).subarray(off, off + n)),
    ];
  }
  if (cmd === 67) {
    const [stage, id] = p;
    if (stage === 0) objs.set(id, new Uint8Array(bkR32(p, 2)));
    if (stage === 1) {
      const off = bkR32(p, 2),
        packed = p.slice(7),
        n = packed.length - Math.ceil(packed.length / 8);
      objs.get(id).set(bkUnpack(packed, n), off);
    }
    return [stage, id, 0];
  }
  throw Error(`Unexpected command ${cmd}`);
}
const d = new Device(request);
assert.deepEqual(await d.readBank(), kit);
const captured = await d.capture();
validateArchive(captured);
assert.deepEqual(await d.writeBank(kit), kit);
await d.restore(archive);
assert.deepEqual(
  logs.filter((p) => p[0] === 67 && p[1] === 2).map((p) => p[2]),
  [2, 3, 4, 5, 6, 7, 8, 9, 1, 0],
);
await assert.rejects(new Device(async () => [0]).writeBank(kit));
console.log(
  "PIGEON GUI: all bitrates, bank integrity, independent replacement, kit, archive preflight, transfer/readback, and restore order passed",
);
