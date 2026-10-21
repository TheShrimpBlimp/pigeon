// SPDX-License-Identifier: GPL-3.0-only
class Capture extends AudioWorkletProcessor {
  constructor() {
    super(); this.active = false; this.used = 0;
    this.left = new Float32Array(4096); this.right = new Float32Array(4096);
    this.port.onmessage = ({data}) => {
      if (data === 'start') { this.used = 0; this.active = true; }
      if (data === 'stop') { this.active = false; this.flush(); this.port.postMessage({stopped: true}); }
    };
  }
  flush() {
    if (!this.used) return;
    const left = this.left.slice(0, this.used), right = this.right.slice(0, this.used);
    this.port.postMessage({left, right}, [left.buffer, right.buffer]); this.used = 0;
  }
  process(inputs) {
    const channels = inputs[0];
    if (this.active && channels.length) {
      for (let i = 0; i < channels[0].length; i++) {
        this.left[this.used] = channels[0][i];
        this.right[this.used++] = (channels[1] || channels[0])[i];
        if (this.used === this.left.length) this.flush();
      }
    }
    return true;
  }
}
registerProcessor('pigeon-capture', Capture);
