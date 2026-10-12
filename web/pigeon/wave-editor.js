// SPDX-License-Identifier: GPL-3.0-only
export class WaveEditor {
  constructor(canvas, onChange) {
    this.canvas = canvas;
    this.onChange = onChange;
    this.pcm = null;
    this.a = 0;
    this.b = 0;
    this.zoom = 0;
    this.scroll = 0;
    this.drag = null;
    this.cache = null;
    canvas.addEventListener("pointerdown", (e) => {
      if (!this.pcm) return;
      canvas.setPointerCapture(e.pointerId);
      const p = this.position(e),
        [start, span] = this.window(),
        t = (span * 10) / canvas.getBoundingClientRect().width;
      this.drag =
        Math.abs(p - this.a) < t
          ? "start"
          : Math.abs(p - this.b) < t
            ? "end"
            : "range";
      if (this.drag === "range") {
        this.anchor = p;
        this.a = p;
        this.b = p + 64;
      }
      this.move(e);
    });
    canvas.addEventListener("pointermove", (e) => {
      if (this.drag) this.move(e);
    });
    canvas.addEventListener("pointerup", (e) => {
      if (!this.drag) return;
      this.move(e);
      this.drag = null;
    });
    canvas.addEventListener("pointercancel", () => (this.drag = null));
    canvas.style.touchAction = "none";
  }
  window() {
    const n = this.pcm?.length || 1,
      span = Math.max(64, Math.floor(n / 2 ** this.zoom));
    return [Math.floor(this.scroll * (n - span)), span];
  }
  position(e) {
    const [a, span] = this.window(),
      r = this.canvas.getBoundingClientRect();
    return Math.max(
      0,
      Math.min(
        this.pcm.length,
        Math.round(a + ((e.clientX - r.left) / r.width) * span),
      ),
    );
  }
  move(e) {
    const p = this.position(e),
      n = this.pcm.length;
    if (this.drag === "start") this.a = Math.min(p, this.b - 64);
    else if (this.drag === "end") this.b = Math.max(p, this.a + 64);
    else {
      this.a = Math.min(p, this.anchor);
      this.b = Math.max(p, this.anchor);
      if (this.b - this.a < 64) this.b = Math.min(n, this.a + 64);
      if (this.b - this.a < 64) this.a = this.b - 64;
    }
    this.a = Math.max(0, this.a);
    this.b = Math.min(n, this.b);
    this.onChange?.(this.a, this.b);
    this.draw();
  }
  set(pcm, a = 0, b = pcm?.length || 0) {
    if (pcm !== this.pcm) this.cache = null;
    this.pcm = pcm;
    this.a = a;
    this.b = b;
    this.draw();
  }
  draw() {
    const c = this.canvas,
      w = (this.canvas.width = 1000),
      h = (this.canvas.height = 140),
      ctx = c.getContext("2d");
    ctx.fillStyle = "#11150d";
    ctx.fillRect(0, 0, w, h);
    if (!this.pcm) return;
    const [start, span] = this.window();
    if (!this.cache || this.cache.start !== start || this.cache.span !== span) {
      const peaks = [];
      for (let x = 0; x < w; x++) {
        let lo = 1,
          hi = -1;
        for (
          let i = start + Math.floor((x * span) / w);
          i <
          Math.min(
            this.pcm.length,
            start +
              Math.max(
                Math.floor(((x + 1) * span) / w),
                Math.floor((x * span) / w) + 1,
              ),
          );
          i++
        ) {
          lo = Math.min(lo, this.pcm[i]);
          hi = Math.max(hi, this.pcm[i]);
        }
        peaks.push([lo, hi]);
      }
      this.cache = { start, span, peaks };
    }
    ctx.fillStyle = "#b9ef77";
    this.cache.peaks.forEach(([lo, hi], x) =>
      ctx.fillRect(x, ((1 - hi) * h) / 2, 1, Math.max(1, ((hi - lo) * h) / 2)),
    );
    const a = ((this.a - start) / span) * w,
      b = ((this.b - start) / span) * w;
    ctx.fillStyle = "#b9ef7726";
    ctx.fillRect(a, 0, b - a, h);
    ctx.strokeStyle = "#fff";
    for (const x of [a, b]) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }
  }
}
