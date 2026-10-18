// SPDX-License-Identifier: GPL-3.0-only
// Pure editors for the existing metadata and FUNA wire formats.
import { inspect, component, crc, join, HEADER, HEAD, onsets } from "./bank.js";
import { b64, unb64, validateArchive } from "./device.js";
const dv = (b) => new DataView(b.buffer, b.byteOffset, b.byteLength);
const SONG_OFF = [256, 1024],
  KIT_OFF = 1792,
  REC_SIZE = 544;
export function withMetadata(image) {
  const info = inspect(image);
  return image.length > info.used
    ? image.slice()
    : join([image, new Uint8Array(4096).fill(255)]);
}
export function songChops(image, slot) {
  const info = inspect(image),
    s = component(image, slot);
  if (!s) return [];
  const meta = image.subarray(info.used),
    off = SONG_OFF[slot - 1],
    v = dv(meta),
    h = s.h;
  if (
    meta.length === 4096 &&
    v.getUint32(off, true) === 0x314f4b46 &&
    v.getUint32(off + 4, true) === crc(s.song) &&
    v.getUint32(off + 540, true) === crc(meta.subarray(off, off + 540))
  ) {
    const n = v.getUint32(off + 12, true);
    if (n > 0 && n <= 32) {
      const chops = [];
      for (let i = 0; i < n; i++) {
        const p = off + 16 + i * 16,
          c = {
            a: v.getUint32(p, true),
            b: v.getUint32(p + 4, true),
            pitch: v.getInt16(p + 12, true),
            flags: v.getUint16(p + 14, true),
          };
        if (
          c.a >= c.b ||
          c.b > h[6] ||
          c.b - c.a < 64 ||
          Math.abs(c.pitch) > 24 ||
          c.flags & ~65535
        )
          throw Error("Invalid stored chop");
        chops.push(c);
      }
      return chops;
    }
  }
  const starts = Array.from({ length: h[16] }, (_, i) =>
    dv(image).getUint32((slot === 2 ? HEADER : 0) + 80 + i * 4, true),
  );
  return starts.map((a, i) => ({
    a,
    b: starts[i + 1] || h[6],
    pitch: 0,
    flags: 0,
  }));
}
export function saveChops(image, slot, chops) {
  if (!chops.length || chops.length > 32) throw Error("Choose 1–32 chops");
  const out = withMetadata(image),
    info = inspect(out),
    s = component(out, slot);
  if (!s) throw Error("Song is empty");
  const meta = out.subarray(info.used),
    off = SONG_OFF[slot - 1],
    v = dv(meta),
    valid =
      v.getUint32(off, true) === 0x314f4b46 &&
      v.getUint32(off + 540, true) === crc(meta.subarray(off, off + 540));
  if (!valid) {
    meta.fill(0, off, off + REC_SIZE);
    meta.set([12, 1, 10, 10, 0, 12, 1, 10, 10, 0], off + 528);
  }
  v.setUint32(off, 0x314f4b46, true);
  v.setUint32(off + 4, crc(s.song), true);
  const f = info.songs[0].fields;
  v.setUint32(off + 8, crc(out.subarray(f[9], f[9] + f[10])), true);
  v.setUint32(off + 12, chops.length, true);
  meta.fill(0, off + 16, off + 528);
  chops.forEach((c, i) => {
    if (
      !Number.isInteger(c.a) ||
      !Number.isInteger(c.b) ||
      c.a < 0 ||
      c.b > s.h[6] ||
      c.b - c.a < 64 ||
      !Number.isInteger(c.pitch) ||
      Math.abs(c.pitch) > 24 ||
      !Number.isInteger(c.flags) ||
      c.flags < 0 ||
      c.flags > 65535 ||
      c.flags & ~65535
    )
      throw Error("Invalid chop boundaries or settings");
    const p = off + 16 + i * 16;
    v.setUint32(p, c.a, true);
    v.setUint32(p + 4, c.b, true);
    v.setInt16(p + 12, c.pitch, true);
    v.setUint16(p + 14, c.flags, true);
  });
  v.setUint32(off + 540, crc(meta.subarray(off, off + 540)), true);
  return out;
}
export function padSettings(image) {
  const info = inspect(image),
    meta = image.subarray(info.used),
    f = info.songs[0].fields,
    out = Array.from({ length: 16 }, () => ({
      pitch: 0,
      level: 127,
      decay: 0,
      choke: 0,
    }));
  if (meta.length !== 4096) return out;
  const v = dv(meta);
  if (
    v.getUint32(KIT_OFF, true) !== 0x3150444b ||
    v.getUint32(KIT_OFF + 4, true) !==
      crc(image.subarray(f[9], f[9] + f[10])) ||
    v.getUint32(KIT_OFF + 72, true) !==
      crc(meta.subarray(KIT_OFF, KIT_OFF + 72))
  )
    return out;
  for (let i = 0; i < 16; i++) {
    const p = KIT_OFF + 8 + i * 4;
    out[i] = {
      pitch: v.getInt8(p),
      level: meta[p + 1],
      decay: meta[p + 2],
      choke: meta[p + 3],
    };
  }
  return out;
}
export function savePadSettings(image, settings) {
  if (settings.length !== 16) throw Error("Kit needs 16 settings");
  const out = withMetadata(image),
    info = inspect(out),
    meta = out.subarray(info.used),
    v = dv(meta),
    f = info.songs[0].fields;
  v.setUint32(KIT_OFF, 0x3150444b, true);
  v.setUint32(KIT_OFF + 4, crc(out.subarray(f[9], f[9] + f[10])), true);
  settings.forEach((c, i) => {
    if (
      !Number.isInteger(c.pitch) ||
      Math.abs(c.pitch) > 24 ||
      ![c.level, c.decay].every(
        (n) => Number.isInteger(n) && n >= 0 && n <= 127,
      ) ||
      !Number.isInteger(c.choke) ||
      c.choke < 0 ||
      c.choke > 4
    )
      throw Error("Invalid pad settings");
    const p = KIT_OFF + 8 + i * 4;
    v.setInt8(p, c.pitch);
    meta.set([c.level, c.decay, c.choke], p + 1);
  });
  v.setUint32(KIT_OFF + 72, crc(meta.subarray(KIT_OFF, KIT_OFF + 72)), true);
  return out;
}
export function renameSong(image, slot, name) {
  const out = image.slice(),
    off = slot === 2 ? HEADER : 0;
  if (!component(out, slot)) throw Error("Song is empty");
  out.fill(0, off + 656, off + 664);
  out.set(
    new TextEncoder().encode(name.replace(/[^\x20-\x7e]/g, "?")).subarray(0, 8),
    off + 656,
  );
  dv(out).setUint32(
    off + HEADER - 4,
    crc(out.subarray(off, off + HEADER - 4)),
    true,
  );
  inspect(out);
  return out;
}
export function scanChops(pcm, sensitivity) {
  if (!Number.isFinite(sensitivity) || sensitivity < 0 || sensitivity > 100)
    throw Error("Invalid sensitivity");
  let peak = 0;
  for (const x of pcm) peak = Math.max(peak, Math.abs(x));
  if (!peak) return [{ a: 0, b: pcm.length, pitch: 0, flags: 0 }];
  const gap = 1102,
    window = 256,
    threshold = peak * (0.02 + (100 - sensitivity) * 0.006),
    starts = [0];
  let env = 0,
    last = 0;
  for (let p = 0; p < pcm.length; p += window) {
    let amplitude = 0;
    for (let i = p; i < Math.min(p + window, pcm.length); i++)
      amplitude = Math.max(amplitude, Math.abs(pcm[i]));
    if (
      amplitude > threshold &&
      amplitude > env * 1.8 &&
      p - last >= gap &&
      starts.length < 32 &&
      pcm.length - p >= 64
    ) {
      starts.push(p);
      last = p;
    }
    env = Math.max(amplitude, env * 0.85);
  }
  return starts.map((a, i) => ({
    a,
    b: starts[i + 1] || pcm.length,
    pitch: 0,
    flags: 0,
  }));
}
export const fnv = (b) => {
  let h = 0x811c9dc5;
  for (const x of b) h = Math.imul(h ^ x, 16777619) >>> 0;
  return h;
};
export function project(bytes) {
  const b = bytes.slice(),
    v = dv(b);
  if (
    b.length !== 3840 ||
    v.getUint32(0, true) !== 0x46554e41 ||
    v.getUint32(4, true) !== 3840 ||
    v.getUint32(3836, true) !== fnv(b.subarray(0, 3836))
  )
    throw Error("Pattern editing requires a valid current-format FUNA project");
  const np = b[66];
  if (np < 104 || np > 115) throw Error("Unsupported project parameter count");
  const stride = np + 578,
    chain = 68 + stride * 4,
    motion = chain + 84;
  if (b[motion] > 128 || b[motion + 1] > 15)
    throw Error("Invalid project motion table");
  if (motion + 388 > 3312) throw Error("Project layout overlaps");
  return { b, np, stride, chain, motion };
}
function stepPitch(p, track, index) {
  const place = track * 64 + index;
  for (let i = 0; i < p.b[p.motion]; i++) {
    const o = p.motion + 4 + i * 3;
    if (p.b[o] === place && p.b[o + 1] === 156) return (p.b[o + 2] << 24) >> 24;
  }
  return 0;
}
function setPitch(p, track, index, pitch) {
  if (!Number.isInteger(pitch) || pitch < -24 || pitch > 24)
    throw Error("Invalid step pitch");
  const place = track * 64 + index,
    b = p.b;
  let i = 0;
  for (; i < b[p.motion]; i++) {
    const o = p.motion + 4 + i * 3;
    if (b[o] === place && (b[o + 1] & 127) === 28) break;
  }
  if (i === b[p.motion] && pitch === 0) return;
  if (i === 128) throw Error("Parameter lock storage is full");
  const o = p.motion + 4 + i * 3;
  b[o] = place;
  b[o + 1] = 156;
  b[o + 2] = pitch & 255;
  if (i === b[p.motion]) b[p.motion]++;
  b[p.motion + 1] |= 1 << track;
}
export function readStep(p, track, index) {
  if (track < 0 || track > 2 || index < 0 || index > 63)
    throw Error("Invalid step");
  const o = 68 + track * p.stride + p.np + 2 + index * 9,
    b = p.b,
    meta = b[o + 4];
  return {
    notes: Array.from(b.subarray(o, o + 4), (n) => n & 127),
    n: meta & 7,
    time: (meta >> 3) & 3,
    velocity: b[o + 5] & 127,
    probability:
      (b[o + 8] & 127) === 0
        ? 100
        : (b[o + 8] & 127) === 101
          ? 0
          : (b[o + 8] & 127) >= 102
            ? 100
            : b[o + 8] & 127,
    pitch: stepPitch(p, track, index),
    condition:
      (b[o + 8] & 127) >= 102 && (b[o + 8] & 127) <= 116
        ? (b[o + 8] & 127) - 101
        : 0,
    nudge:
      ((b[o] & 128) >> 7) |
      ((b[o + 1] & 128) >> 6) |
      ((b[o + 2] & 128) >> 5) |
      (b[o + 3] & 128 ? -8 : 0),
    repeats: 1 + (b[o + 5] >> 7) + ((b[o + 8] >> 7) << 1),
  };
}
export function editStep(bytes, track, index, change) {
  for (const value of Object.values(change))
    if (!Number.isInteger(value)) throw Error("Step values must be integers");
  const p = project(bytes),
    o = 68 + track * p.stride + p.np + 2 + index * 9,
    b = p.b,
    s = readStep(p, track, index);
  if (change.chop !== undefined) {
    const n = change.chop;
    if (!Number.isInteger(n) || n < 0 || n > 32) throw Error("Invalid chop");
    b[o] = (b[o] & 128) | (n ? 59 + n : 0);
    if (n) {
      b[o + 4] = (b[o + 4] & 96) | 1;
      if (!s.velocity) b[o + 5] = (b[o + 5] & 128) | 100;
    } else {
      b.fill(0, o, o + 9);
      b[o + 4] = 16;
    }
  }
  if (change.velocity !== undefined) {
    if (change.velocity < 1 || change.velocity > 127)
      throw Error("Invalid velocity");
    b[o + 5] = (b[o + 5] & 128) | change.velocity;
  }
  if (change.probability !== undefined) {
    if (change.probability < 0 || change.probability > 100)
      throw Error("Invalid probability");
    b[o + 8] =
      (b[o + 8] & 128) |
      (change.probability === 100
        ? 0
        : change.probability === 0
          ? 101
          : change.probability);
  }
  if (change.repeats !== undefined) {
    if (change.repeats < 1 || change.repeats > 4)
      throw Error("Invalid repeats");
    const r = change.repeats - 1;
    b[o + 5] = (b[o + 5] & 127) | ((r & 1) << 7);
    b[o + 8] = (b[o + 8] & 127) | ((r >> 1) << 7);
  }
  if (change.pitch !== undefined) setPitch(p, track, index, change.pitch);
  if (change.condition !== undefined) {
    if (change.condition > 4 && p.np < 115) throw Error("Expanded conditions require a v0.4 project");
    if (change.condition < 0 || change.condition > 15)
      throw Error("Invalid condition");
    if (change.condition)
      b[o + 8] = (b[o + 8] & 128) | (101 + change.condition);
    else if ((b[o + 8] & 127) > 101) b[o + 8] &= 128;
  }
  if (change.nudge !== undefined) {
    if (change.nudge < -8 || change.nudge > 7) throw Error("Invalid nudge");
    const u = change.nudge & 15;
    for (let j = 0; j < 4; j++)
      b[o + j] = (b[o + j] & 127) | (((u >> j) & 1) << 7);
    if (change.nudge) b[68 + track * p.stride + 94] = 64;
  }
  dv(b).setUint32(3836, fnv(b.subarray(0, 3836)), true);
  project(b);
  return b;
}
export function patternLength(bytes, track, length) {
  if (!Number.isInteger(length) || length < 1 || length > 64)
    throw Error("Length must be 1–64");
  const p = project(bytes);
  p.b[68 + track * p.stride + 29] = length + 64;
  dv(p.b).setUint32(3836, fnv(p.b.subarray(0, 3836)), true);
  return p.b;
}
export function updateObject(archive, id, bytes) {
  validateArchive(archive);
  const a = structuredClone(archive);
  a.objects[id] = { id, size: bytes.length, crc: crc(bytes), data: b64(bytes) };
  validateArchive(a);
  return a;
}
