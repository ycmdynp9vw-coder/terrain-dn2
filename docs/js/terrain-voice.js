// TERRAIN's voice for the web page (an AudioWorklet): the point runs over the land at the note's pitch, its height is the sound.
// The same knobs and the same maths as the page drawing, in floating point, one sample at a time. AGPL-3.0, blskk.
const TAU = Math.PI * 2;
const cl = (v, a, b) => (v < a ? a : v > b ? b : v);
class TerrainVoice extends AudioWorkletProcessor {
  constructor() {
    super();
    this.S = null; this.k = null; this.t = 0; this.oy = 0; this.env = 0; this.gate = 0; this.f = 130.81;
    this.dcx = 0; this.dcy = 0;
    this.port.onmessage = (e) => {
      const d = e.data;
      if (d.lands) this.S = new Int8Array(d.lands);
      if (d.k) this.k = d.k;
      if (d.on !== undefined) { if (d.on) { this.f = d.freq; this.gate = 1; } else this.gate = 0; }
    };
  }
  land(i, gx, gy) {
    const S = this.S, N = 48; gx = cl(gx, 0, 47 * 256 - 1); gy = cl(gy, 0, 47 * 256 - 1);
    const ix = gx >> 8, iy = gy >> 8, ax = (gx - ix * 256) / 256, ay = (gy - iy * 256) / 256, o = i * N * N + iy * N + ix;
    const a = S[o] + (S[o + 1] - S[o]) * ax, b = S[o + N] + (S[o + N + 1] - S[o + N]) * ax;
    return (a + (b - a) * ay) * 48.38;            // = the device's ((z << 8) * 12385) >> 16
  }
  z(k, x, y) {
    const gx = ((cl(x, -9830, 9830) + 9830) * 10027) / 16384, gy = ((cl(y, -9830, 9830) + 9830) * 10027) / 16384;
    let z = this.land(k.bank * 8 + k.a, gx, gy);
    if (k.f) z += (this.land(k.bank * 8 + k.a + 1, gx, gy) - z) * k.f;
    if (k.wp) z += k.wp * (4096 * Math.sin((TAU * (cl(z, -8192, 8192) * (0.5 + 1.5 * k.wp))) / 4096) - z);
    return z;
  }
  wave(wav, ph) {
    let q = ((ph % 4096) + 4096) % 4096; if (q >= 2048) q -= 4096;
    if (wav === 0) return 4096 * Math.sin((TAU * q) / 4096);
    if (wav === 1) return 4096 - 4 * Math.abs(q);
    if (wav === 2) return q >= 0 ? 4096 : -4096;
    return -2 * q;
  }
  pd(ph, pd) {
    const s = ((((ph % 4096) + 4096) % 4096) + 2048) % 4096, d = 2048 - 1966 * pd;
    return Math.min((s * 2048) / d, 2048 + ((s - d) * 2048) / (4096 - d)) - 2048;
  }
  process(_, outputs) {
    const out = outputs[0][0], k = this.k;
    if (!this.S || !k) { out.fill(0); return true; }
    const inc = (4096 * this.f) / sampleRate, att = 1 - Math.exp(-1 / (0.004 * sampleRate)), rel = 1 - Math.exp(-1 / (0.25 * sampleRate));
    for (let i = 0; i < out.length; i++) {
      this.t = (this.t + inc) % (4096 * 64);
      let phx = this.t * k.rx + k.md * this.oy + k.phx, phy = this.t * k.ry + k.phy;
      if (k.pdx) phx = this.pd(phx, k.pdx);
      if (k.pdy) phy = this.pd(phy, k.pdy);
      const ox = this.wave(k.wx, phx) / 4096, oy = this.wave(k.wy, phy) / 4096;
      this.oy = oy * 4096;                       // MOD: Y pushes the phase of X, one sample later as on the page
      const x = k.cx + k.sx * ox, y = k.cy + k.sy * oy;
      let s = this.z(k, cl(x, -16384, 16384), cl(y, -16384, 16384)) / 4096;
      const hp = s - this.dcx + 0.995 * this.dcy; this.dcx = s; this.dcy = hp;   // the high-pass of the DSP
      this.env += ((this.gate ? 1 : 0) - this.env) * (this.gate ? att : rel);
      out[i] = Math.tanh(hp * 1.6) * this.env * 0.7;
    }
    for (let c = 1; c < outputs[0].length; c++) outputs[0][c].set(out);
    return true;
  }
}
registerProcessor("terrain-voice", TerrainVoice);
