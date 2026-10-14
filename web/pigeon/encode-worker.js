// SPDX-License-Identifier: GPL-3.0-only
// The entire encoder is bundled locally, not fetched from a CDN.
importScripts("vendor/lame.js");
self.onmessage = async ({ data: { pcm, bitrate, name, preserveGain } }) => {
  try {
    const { normalize, join, indexedMp3, makeSong } = await import("./bank.js");
    if (![24, 32, 40, 48, 56, 64].includes(bitrate))
      throw Error("Unsupported bitrate");
    const samples = preserveGain ? Int16Array.from(pcm,n=>Math.round(Math.max(-1,Math.min(1,n))*32767)) : normalize(pcm),
      encoder = new lamejs.Mp3Encoder(1, 22050, bitrate),
      chunks = [];
    for (let i = 0; i < samples.length; i += 1152) {
      chunks.push(
        new Uint8Array(encoder.encodeBuffer(samples.subarray(i, i + 1152))),
      );
      if (i % 11520 === 0) self.postMessage({ progress: i / samples.length });
    }
    chunks.push(new Uint8Array(encoder.flush()));
    const image = makeSong(
      samples,
      indexedMp3(join(chunks), samples.length),
      name,
    );
    self.postMessage({ image }, [image.buffer]);
  } catch (e) {
    self.postMessage({ error: e.message });
  }
};
