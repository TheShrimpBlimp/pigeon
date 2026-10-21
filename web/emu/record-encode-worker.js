// SPDX-License-Identifier: GPL-3.0-only
importScripts('../pigeon/vendor/lame.js');
self.onmessage = ({data: {chunks, sampleRate}}) => {
  try {
    const frames = chunks.reduce((n, c) => n + c.left.length, 0);
    if (!frames) throw Error('No audio was captured.');
    if (frames * 4 > 0xffffffff - 36) throw Error('Recording exceeds the WAV size limit.');
    const header = new ArrayBuffer(44), view = new DataView(header);
    const text = (at, s) => [...s].forEach((c, i) => view.setUint8(at + i, c.charCodeAt(0)));
    text(0, 'RIFF'); view.setUint32(4, 36 + frames * 4, true); text(8, 'WAVEfmt ');
    view.setUint32(16, 16, true); view.setUint16(20, 1, true); view.setUint16(22, 2, true);
    view.setUint32(24, sampleRate, true); view.setUint32(28, sampleRate * 4, true);
    view.setUint16(32, 4, true); view.setUint16(34, 16, true); text(36, 'data'); view.setUint32(40, frames * 4, true);
    const wav = [header], mp3 = [], encoder = new lamejs.Mp3Encoder(2, sampleRate, 192);
    const pcm = value => Math.round(Math.max(-1, Math.min(1, value)) * (value < 0 ? 32768 : 32767));
    for (const chunk of chunks) {
      const left = Int16Array.from(chunk.left, pcm), right = Int16Array.from(chunk.right, pcm);
      const bytes = new Uint8Array(left.length * 4), out = new DataView(bytes.buffer);
      for (let i = 0; i < left.length; i++) { out.setInt16(i * 4, left[i], true); out.setInt16(i * 4 + 2, right[i], true); }
      wav.push(bytes);
      for (let i = 0; i < left.length; i += 1152) {
        const encoded = encoder.encodeBuffer(left.subarray(i, i + 1152), right.subarray(i, i + 1152));
        if (encoded.length) mp3.push(new Uint8Array(encoded));
      }
    }
    const tail = encoder.flush(); if (tail.length) mp3.push(new Uint8Array(tail));
    self.postMessage({wav: new Blob(wav, {type: 'audio/wav'}), mp3: new Blob(mp3, {type: 'audio/mpeg'})});
  } catch (error) { self.postMessage({error: error.message}); }
};
