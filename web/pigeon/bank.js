// SPDX-License-Identifier: GPL-3.0-only
// Local bank construction. Matches tools/ko_bank.py and pigeon_library.py.
import "../ko_bank.js";
import { bkCrc as crc } from "../fm1backup.js";
export const { SIZE, HEAD, HEADER, inspect } = globalThis.KOBank;
export { crc };
export const join = (chunks) => {
  const b = new Uint8Array(chunks.reduce((n, c) => n + c.length, 0));
  let p = 0;
  for (const c of chunks) {
    b.set(c, p);
    p += c.length;
  }
  return b;
};
const view = (b) => new DataView(b.buffer, b.byteOffset, b.byteLength);
export function component(image, slot) {
  const info = inspect(image);
  if (slot === 2 && info.version !== 3) return null;
  const off = slot === 1 ? 0 : HEADER,
    h = info.songs[slot - 1].fields.slice();
  return {
    h,
    tail: image.slice(off + 80, off + HEADER - 4),
    song: image.slice(h[7], h[7] + h[8]),
    wave: image.slice(h[14], h[14] + h[15] * 2),
  };
}
export function repack(original, replacement = null, slot = 1, kit = null) {
  if (![1, 2].includes(slot)) throw Error("Invalid song slot");
  const f = inspect(original),
    songs = [component(original, 1), component(original, 2)];
  if (replacement) songs[slot - 1] = component(replacement, 1);
  const src = kit || original,
    g = inspect(src),
    fields = g.songs[0].fields;
  const kitdata = src.slice(fields[9], fields[9] + fields[10]),
    zones = src.slice(208, 656);
  const chunks = [kitdata, new Uint8Array(-kitdata.length & 255)];
  let length = chunks.reduce((n, c) => n + c.length, 0);
  const headers = [];
  for (let i = 0; i < 2; i++) {
    const s = songs[i];
    if (!s) continue;
    const h = s.h.slice();
    h[1] = songs[1] && !i ? 3 : 2;
    h[9] = HEAD + (i ? length : 0);
    h[10] = i ? 0 : kitdata.length;
    h[11] = i ? 0 : fields[11];
    h[7] = HEAD + length;
    chunks.push(s.song);
    length += s.song.length;
    h[14] = HEAD + length;
    chunks.push(s.wave);
    length += s.wave.length;
    h[17] = h[1] === 3 ? HEADER : 0;
    h[18] = h[19] = 0;
    const tail = s.tail.slice();
    tail.set(i ? new Uint8Array(448) : zones, 128);
    headers.push({ h, tail });
    if (!i && songs[1]) {
      const pad = new Uint8Array(-(HEAD + length) & 255);
      chunks.push(pad);
      length += pad.length;
    }
  }
  const used = HEAD + length;
  if (used > SIZE)
    throw Error(
      `Samples need ${(length / 1024).toFixed(1)} KiB; capacity is 408 KiB. Shorten audio or lower song bitrate.`,
    );
  const payload = join(chunks),
    image = new Uint8Array(used + original.length - f.used);
  image.fill(255, 0, HEAD);
  image.set(payload, HEAD);
  image.set(original.subarray(f.used), used);
  headers.forEach(({ h, tail }, i) => {
    h[2] = used;
    h[3] = crc(payload);
    const raw = image.subarray(i * HEADER, (i + 1) * HEADER),
      v = view(raw);
    h.forEach((n, j) => v.setUint32(j * 4, n, true));
    raw.set(tail, 80);
    v.setUint32(HEADER - 4, crc(raw.subarray(0, HEADER - 4)), true);
  });
  inspect(image);
  return image;
}
const STEPS = [
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
  253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
  1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
  3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
  11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
  32767,
];
const ADJ = [-1, -1, -1, -1, 2, 4, 6, 8],
  clamp = (n, a, b) => Math.max(a, Math.min(b, n));
export function normalize(x) {
  let peak = 1e-9;
  for (const n of x) {
    if (!Number.isFinite(n)) throw Error("Invalid audio sample");
    peak = Math.max(peak, Math.abs(n));
  }
  return Int16Array.from(x, (n) =>
    clamp(Math.trunc((n / peak) * 30000), -32768, 32767),
  );
}
export function imaEncode(samples) {
  const out = new Uint8Array(Math.ceil(samples.length / 2));
  let pred = 0,
    idx = 0;
  for (let i = 0; i < samples.length; i++) {
    const step = STEPS[idx];
    let diff = samples[i] - pred,
      code = diff < 0 ? 8 : 0,
      d = step >> 3;
    diff = Math.abs(diff);
    if (diff >= step) {
      code |= 4;
      diff -= step;
      d += step;
    }
    if (diff >= step >> 1) {
      code |= 2;
      diff -= step >> 1;
      d += step >> 1;
    }
    if (diff >= step >> 2) {
      code |= 1;
      d += step >> 2;
    }
    pred = clamp(pred + (code & 8 ? -d : d), -32768, 32767);
    idx = clamp(idx + ADJ[code & 7], 0, 88);
    out[i >> 1] |= code << ((i & 1) * 4);
  }
  return out;
}
export function imaDecode(data, n) {
  const out = new Float32Array(n);
  let pred = 0,
    idx = 0;
  for (let i = 0; i < n; i++) {
    const c = (data[i >> 1] >> ((i & 1) * 4)) & 15,
      s = STEPS[idx],
      d =
        (s >> 3) +
        (c & 4 ? s : 0) +
        (c & 2 ? s >> 1 : 0) +
        (c & 1 ? s >> 2 : 0);
    pred = clamp(pred + (c & 8 ? -d : d), -32768, 32767);
    idx = clamp(idx + ADJ[c & 7], 0, 88);
    out[i] = pred / 32768;
  }
  return out;
}
export function onsets(x, rate = 22050) {
  let maxd = 0,
    maxr = 0;
  for (let i = 0; i < x.length; i++) {
    maxd = Math.max(maxd, Math.abs(x[i] - (i ? x[i - 1] : 0)));
    maxr = Math.max(maxr, Math.abs(x[i]));
  }
  const td = Math.floor(maxd / 12),
    tr = Math.floor(maxr / 8),
    starts = [0],
    gap = Math.floor(rate / 20);
  let ed = 0,
    er = 0,
    last = 0,
    previous = 0;
  for (let pos = 0; pos < x.length; pos += 32) {
    let hd = 0,
      hr = 0;
    for (let j = pos; j < Math.min(pos + 32, x.length); j++) {
      hd = Math.max(hd, Math.abs(x[j] - (j ? x[j - 1] : 0)));
      hr = Math.max(hr, Math.abs(x[j]));
    }
    if (
      pos &&
      ((hd > td && hd * 4 > ed * 7) || (hr > tr && hr * 4 > er * 7)) &&
      previous >= last + gap &&
      starts.length < 32
    ) {
      starts.push(previous);
      last = previous;
    }
    ed = hd > ed ? hd : ed - Math.floor(ed / 16);
    er = hr > er ? hr : er - Math.floor(er / 64);
    previous = pos;
  }
  return starts;
}
export function indexedMp3(encoded, samples) {
  const offsets = [0];
  while (offsets.at(-1) < encoded.length) {
    const p = offsets.at(-1),
      a = encoded[p],
      b = encoded[p + 1],
      c = encoded[p + 2],
      d = encoded[p + 3],
      br = c >> 4;
    if (
      p + 13 > encoded.length ||
      a !== 255 ||
      (b & 254) !== 242 ||
      c & 12 ||
      br < 1 ||
      br > 8 ||
      d >> 6 !== 3
    )
      throw Error("Encoder emitted unsupported MP3");
    const n =
      Math.floor((72000 * [0, 8, 16, 24, 32, 40, 48, 56, 64][br]) / 22050) +
      ((c >> 1) & 1);
    if (encoded[p + 4 + (b & 1 ? 0 : 2)] || p + n > encoded.length)
      throw Error("MP3 reservoir or truncated frame");
    offsets.push(p + n);
  }
  const frames = offsets.length - 1,
    skip = 1105;
  if (frames < 3 || frames >= 16000 || samples + skip > frames * 576)
    throw Error("MP3 duration exceeds firmware limits");
  const table = new Uint8Array(offsets.length * 4),
    v = view(table);
  offsets.forEach((n, i) => v.setUint32(i * 4, n + table.length, true));
  return { data: join([table, encoded]), frames, skip };
}
export function makeSong(samples, encoded, name = "UNTITLED") {
  if (!samples.length || samples.length > 8000000)
    throw Error("Empty or excessive song length");
  const { data, frames, skip } = encoded,
    bins = Math.min(4096, samples.length),
    wave = new Uint8Array(bins * 2);
  for (let i = 0; i < bins; i++) {
    let lo = 32767,
      hi = -32768;
    for (
      let p = Math.floor((i * samples.length) / bins);
      p < Math.floor(((i + 1) * samples.length) / bins);
      p++
    ) {
      lo = Math.min(lo, samples[p]);
      hi = Math.max(hi, samples[p]);
    }
    wave[i * 2] = (lo >> 9) & 255;
    wave[i * 2 + 1] = (hi >> 9) & 255;
  }
  const used = HEAD + data.length + wave.length;
  if (used > SIZE) throw Error("Song exceeds the 408 KiB sample pool");
  const payload = join([data, wave]),
    image = new Uint8Array(used);
  image.fill(255, 0, HEAD);
  image.fill(0, 0, HEADER);
  image.set(payload, HEAD);
  const v = view(image),
    starts = onsets(samples);
  const h = [
    0x32424f4b,
    2,
    used,
    crc(payload),
    2,
    22050,
    samples.length,
    HEAD,
    data.length,
    HEAD,
    0,
    0,
    frames,
    skip,
    HEAD + data.length,
    bins,
    starts.length,
    0,
    0,
    0,
  ];
  h.forEach((n, i) => v.setUint32(i * 4, n, true));
  starts.forEach((n, i) => v.setUint32(80 + i * 4, n, true));
  image.set(
    new TextEncoder().encode(name.replace(/[^\x20-\x7e]/g, "?")).subarray(0, 8),
    656,
  );
  v.setUint32(HEADER - 4, crc(image.subarray(0, HEADER - 4)), true);
  inspect(image);
  return image;
}
export function replaceKit(image, pads) {
  if (
    pads.length > 16 ||
    pads.some((p) =>
      p.encoded
        ? !p.count || p.encoded.length !== Math.ceil(p.count / 2)
        : !p.length,
    )
  )
    throw Error("Kit needs 0–16 nonempty pads");
  const first = component(image, 1),
    zones = new Uint8Array(448),
    v = view(zones),
    chunks = [];
  let size = 0;
  pads.forEach((pcm, i) => {
    const data = pcm.encoded || imaEncode(pcm),
      count = pcm.encoded ? pcm.count : pcm.length,
      p = i * 28;
    [size, count, 0, count - 1, 32768].forEach((n, j) =>
      v.setUint32(p + j * 4, n, true),
    );
    v.setInt16(p + 20, (60 + i) * 16, true);
    zones[p + 25] = zones[p + 26] = 60 + i;
    chunks.push(data);
    size += data.length;
  });
  // Build a valid temporary one-song bank, then use the common relocation path.
  const original = first.h,
    h = original.slice(),
    kit = join(chunks),
    pad = new Uint8Array(-size & 255),
    payload = join([kit, pad, first.song, first.wave]);
  h[1] = 2;
  h[2] = HEAD + payload.length;
  h[3] = crc(payload);
  h[9] = HEAD;
  h[10] = size;
  h[11] = pads.length;
  h[7] = HEAD + size + pad.length;
  h[14] = h[7] + first.song.length;
  h[17] = h[18] = h[19] = 0;
  if (h[2] > SIZE) throw Error("Kit and Song 1 exceed sample capacity");
  const temp = new Uint8Array(h[2]);
  temp.fill(255, 0, HEAD);
  const dv = view(temp);
  h.forEach((n, i) => dv.setUint32(i * 4, n, true));
  temp.set(first.tail, 80);
  temp.set(zones, 208);
  dv.setUint32(HEADER - 4, crc(temp.subarray(0, HEADER - 4)), true);
  temp.set(payload, HEAD);
  return repack(image, null, 1, temp);
}
export function padAudio(image, pad) {
  const f = inspect(image).songs[0].fields;
  if (pad < 0 || pad >= f[11]) return null;
  const v = view(image),
    p = 208 + pad * 28,
    n = v.getUint32(p + 4, true),
    off = f[9] + v.getUint32(p, true);
  return imaDecode(image.subarray(off, off + Math.ceil(n / 2)), n);
}

export function encodedPad(image, pad) {
  const f = inspect(image).songs[0].fields;
  if (pad < 0 || pad >= f[11]) return null;
  const v = view(image),
    p = 208 + pad * 28,
    count = v.getUint32(p + 4, true),
    off = f[9] + v.getUint32(p, true);
  return { count, encoded: image.slice(off, off + Math.ceil(count / 2)) };
}
