// SPDX-License-Identifier: GPL-3.0-only
import {
  bkCrc,
  bkPack,
  bkUnpack,
  bkU32,
  bkR32,
  bkManifest,
} from "../fm1backup.js";
import { inspect, SIZE, HEAD, HEADER, join } from "./bank.js";
const u21 = (n) => [n & 127, (n >>> 7) & 127, (n >>> 14) & 127];
const same = (a, b) => a.length === b.length && a.every((n, i) => n === b[i]);
export const b64 = (b) => {
  let s = "";
  for (let i = 0; i < b.length; i += 8192)
    s += String.fromCharCode(...b.subarray(i, i + 8192));
  return btoa(s);
};
export function unb64(s) {
  if (
    typeof s !== "string" ||
    !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(s)
  )
    throw Error("Invalid archive encoding");
  return Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
}
export function validateArchive(a) {
  if (
    !a ||
    a.format !== "pigeon-archive" ||
    a.version !== 1 ||
    !Array.isArray(a.objects) ||
    a.objects.length !== 10
  )
    throw Error("Not a complete Pigeon archive");
  const bank = unb64(a.bank);
  inspect(bank);
  if (bkCrc(bank) !== a.bank_crc) throw Error("Sample bank checksum mismatch");
  const objects = a.objects.map((o, i) => {
    if (
      !o ||
      o.id !== i ||
      !Number.isInteger(o.size) ||
      o.size < 0 ||
      o.size > 3840 ||
      !Number.isInteger(o.crc) ||
      o.crc < 0 ||
      o.crc > 0xffffffff
    )
      throw Error("Invalid archive object");
    const bytes = unb64(o.data);
    if (bytes.length !== o.size || bkCrc(bytes) !== o.crc)
      throw Error("Project checksum mismatch");
    return { ...o, bytes };
  });
  if (!objects[0].size || !objects[1].size)
    throw Error("Project or settings missing");
  return { bank, objects };
}
export class Device {
  constructor(request, progress = () => {}) {
    this.request = request;
    this.progress = progress;
  }
  async ask(cmd, args = [], timeout = 5000) {
    try { return await this.request([cmd, args], { timeout }); }
    catch(e) { throw Error(`FM1 command ${cmd}: ${e.message || String(e)}`); }
  }
  async negotiate() {
    const a = await this.ask(90);
    if (
      a.length !== 10 ||
      a[0] !== 75 ||
      ![2, 3].includes(a[1]) ||
      !same(a.slice(2, 8), [...u21(SIZE), ...u21(HEAD)]) ||
      a[9] !== 3
    )
      throw Error("Connected firmware does not support Pigeon sample banks");
    return a;
  }
  async ok(cmd, args = [], timeout) {
    const a = await this.ask(cmd, args, timeout);
    if (!same(a, [0]))
      throw Error(
        `Device refused command ${cmd}: ${a.join(", ")}. Stop playback and release all keys.`,
      );
  }
  async readBank() {
    await this.negotiate();
    const read = async (off, n) => {
      const a = await this.ask(94, [...u21(off), n & 127, n >>> 7]);
      if (a[0] !== 0) throw Error("Bank read failed");
      return bkUnpack(a.slice(1), n);
    };
    const header = await read(0, 256),
      v = new DataView(header.buffer),
      used = v.getUint32(8, true);
    if (v.getUint32(0, true) !== 0x32424f4b || used < HEAD || used > SIZE)
      throw Error("Device has no valid Pigeon bank");
    const chunks = [];
    for (let off = 0; off < used; off += 256) {
      chunks.push(await read(off, Math.min(256, used - off)));
      this.progress("Reading samples", off + chunks.at(-1).length, used + 4096);
    }
    for (let off = 0; off < 4096; off += 256) {
      chunks.push(await read(SIZE + off, 256));
      this.progress("Reading chop settings", used + off + 256, used + 4096);
    }
    const image = join(chunks);
    inspect(image);
    return image;
  }
  async writeBank(image) {
    const info = inspect(image),
      cap = await this.negotiate();
    if (info.version === 3 && cap[1] !== 3)
      throw Error("Firmware does not support two songs");
    await this.ok(91, u21(info.used));
    for (let off = HEAD; off < info.used; off += 256) {
      await this.ok(92, [
        ...u21(off),
        ...bkPack(image.subarray(off, Math.min(off + 256, info.used))),
      ]);
      this.progress("Writing samples", off, info.used);
    }
    const n = HEADER * (info.version === 3 ? 2 : 1);
    for (let off = 0; off < n; off += 256)
      await this.ok(92, [
        ...u21(off),
        ...bkPack(image.subarray(off, Math.min(off + 256, n))),
      ]);
    await this.ok(93, [], 30000);
    if (image.length > info.used)
      for (let off = 0; off < 4096; off += 256)
        await this.ok(95, [
          ...u21(off),
          ...bkPack(image.subarray(info.used + off, info.used + off + 256)),
        ]);
    const actual = await this.readBank();
    if (
      !same(actual.subarray(0, info.used), image.subarray(0, info.used)) ||
      (image.length > info.used &&
        !same(actual.subarray(info.used), image.subarray(info.used)))
    )
      throw Error(
        "Device readback differs from the uploaded bank. Retry sending the bank or restore a saved backup.",
      );
    return actual;
  }
  async capture() {
    await this.negotiate();
    const manifest = bkManifest(await this.ask(65)),
      objects = [];
    if (
      !manifest.slice(0, 10).every((o, i) => o.id === i) ||
      manifest.length < 10
    )
      throw Error("Incomplete archive manifest");
    for (const o of manifest.filter((o) => o.id < 32)) {
      const bytes = new Uint8Array(o.size);
      for (let off = 0; off < o.size; off += 256) {
        const n = Math.min(256, o.size - off),
          a = await this.ask(66, [o.id, ...bkU32(off), n & 127, n >>> 7]);
        if (
          a[0] !== o.id ||
          a[1] !== 0 ||
          bkR32(a, 2) !== off ||
          (a[7] | (a[8] << 7)) !== n
        )
          throw Error("Project backup read failed");
        bytes.set(bkUnpack(a.slice(9), n), off);
      }
      if (bkCrc(bytes) !== o.crc) throw Error("Device changed during backup");
      objects.push({ ...o, data: b64(bytes) });
      this.progress("Reading projects", objects.length, 10);
    }
    const image = await this.readBank(),
      a = {
        format: "pigeon-archive",
        version: 1,
        created: new Date().toISOString(),
        objects,
        bank: b64(image),
        bank_crc: bkCrc(image),
      };
    validateArchive(a);
    return a;
  }
  async restore(a) {
    const { bank, objects } = validateArchive(a); // All checks happen before contacting the device.
    await this.writeBank(bank);
    for (const o of [...objects.slice(2), objects[1], objects[0]]) {
      const put = async (args) => {
        const r = await this.ask(67, args);
        if (!same(r, [args[0], o.id, 0]))
          throw Error(`Restore of project object ${o.id} failed`);
      };
      await put([0, o.id, ...bkU32(o.size), ...bkU32(o.crc)]);
      try {
        for (let off = 0; off < o.size; off += 256)
          await put([
            1,
            o.id,
            ...bkU32(off),
            ...bkPack(o.bytes.subarray(off, off + 256)),
          ]);
        await put([2, o.id]);
      } catch (e) {
        await put([3, o.id]).catch(() => {});
        throw e;
      }
      this.progress("Restoring projects", o.id + 1, 10);
    }
    const after = await this.capture(),
      validated = validateArchive(after);
    if (
      !same(validated.bank, bank) ||
      objects.some((o, i) => !same(o.bytes, validated.objects[i].bytes))
    )
      throw Error(
        "Restored session readback differs; recovery backup is retained",
      );
    return after;
  }
}
