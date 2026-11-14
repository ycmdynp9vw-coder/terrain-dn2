// TERRAIN's voice for the web page (an AudioWorklet). The 64 lands are not tables here: each one runs the very same SHARC
// instructions as the Digitone's DSP (js/lands-asm.json, from the generator), through a tiny interpreter with 32-bit
// registers. The path is worked out at half rate and interpolated, the height at full rate, then the DSP's high-pass and
// soft clip, as in em/terreno. AGPL-3.0, blskk.
const TAU = Math.PI * 2;
const cl = (v, a, b) => (v < a ? a : v > b ? b : v);

// ---- the SHARC subset the lands use ----
const buf = new ArrayBuffer(16 * 4), R = new Int32Array(buf), F = new Float32Array(buf);
const MEMB = new ArrayBuffer(8192 * 4), MI = new Int32Array(MEMB), MF = new Float32Array(MEMB);
const I = new Int32Array(8); let M1 = 0, FLAG = 0;          // FLAG: result of the last COMP (a - b)
const SIN = 1024, DER = 2048, VN = 4096;                       // byte addresses of the tables
function initMem(vn) {
  for (let i = 0; i < 256; i++) { MF[SIN / 4 + i] = Math.sin((TAU * i) / 256); MF[DER / 4 + i] = (TAU / 256) * Math.cos((TAU * i) / 256); }
  for (let i = 0; i < vn.length; i++) MF[VN / 4 + i] = vn[i];
  MI[22] = VN;                                                 // the state's word 22: where the noise grid is
  I[3] = 0; I[4] = SIN; I[5] = DER;
}
const reg = (s) => +s.slice(1);
function dmAddr(k, ir) { return k === "M1" ? () => (I[ir] >> 2) + M1 : ((n) => () => (I[ir] >> 2) + n)(+k); }
function compileLine(line) {
  let m;
  line = line.trim().replace(/;$/, "");
  if ((m = line.match(/^R(\d+) = (0x[0-9a-f]+)$/i))) { const d = +m[1], v = parseInt(m[2], 16) | 0; return () => { R[d] = v; }; }
  if ((m = line.match(/^R(\d+) = (-?\d+)$/))) { const d = +m[1], v = +m[2] | 0; return () => { R[d] = v; }; }
  if ((m = line.match(/^I(\d+) = R(\d+)$/))) { const d = +m[1], s = +m[2]; return () => { I[d] = R[s]; }; }
  if ((m = line.match(/^M1 = R(\d+)$/))) { const s = +m[1]; return () => { M1 = R[s]; }; }
  if ((m = line.match(/^COMP\(F(\d+), F(\d+)\)$/))) { const a = +m[1], b = +m[2]; return () => { FLAG = F[a] - F[b]; }; }
  if ((m = line.match(/^IF (GE|GT|LT|LE) F(\d+) = (-|PASS )F(\d+)$/))) {
    const c = m[1], d = +m[2], neg = m[3] === "-", s = +m[4];
    const ok = c === "GE" ? () => FLAG >= 0 : c === "GT" ? () => FLAG > 0 : c === "LT" ? () => FLAG < 0 : () => FLAG <= 0;
    return () => { if (ok()) F[d] = neg ? -F[s] : F[s]; };
  }
  let par = null;
  if ((m = line.match(/^(.*), R(\d+) = DM\(([^,]+), I(\d+)\)$/))) { const d = +m[2], a = dmAddr(m[3].trim(), +m[4]); par = { d, a }; line = m[1]; }
  if (!(m = line.match(/^([FR])(\d+) = (.+)$/))) throw new Error("SHARC? " + line);
  const isF = m[1] === "F", d = +m[2], e = m[3];
  let f;
  let k;
  if (isF) {
    if ((k = e.match(/^F(\d+) ([-+*]) F(\d+)$/))) { const a = +k[1], o = k[2], b = +k[3];
      f = o === "+" ? () => F[a] + F[b] : o === "-" ? () => F[a] - F[b] : () => F[a] * F[b]; }
    else if ((k = e.match(/^PASS F(\d+)$/))) { const a = +k[1]; f = () => F[a]; }
    else if ((k = e.match(/^ABS F(\d+)$/))) { const a = +k[1]; f = () => Math.abs(F[a]); }
    else if ((k = e.match(/^-F(\d+)$/))) { const a = +k[1]; f = () => -F[a]; }
    else if ((k = e.match(/^FLOAT R(\d+)$/))) { const a = +k[1]; f = () => R[a]; }
    else if ((k = e.match(/^CLIP F(\d+) BY F(\d+)$/))) { const a = +k[1], b = +k[2]; f = () => { const l = Math.abs(F[b]); return F[a] > l ? l : F[a] < -l ? -l : F[a]; }; }
    else if ((k = e.match(/^(MAX|MIN)\(F(\d+), F(\d+)\)$/))) { const a = +k[2], b = +k[3]; f = k[1] === "MAX" ? () => Math.max(F[a], F[b]) : () => Math.min(F[a], F[b]); }
    else throw new Error("SHARC F? " + e);
    if (par) { const pd = par.d, pa = par.a; return () => { const v = f(), w = MI[pa()]; F[d] = v; R[pd] = w; }; }
    return () => { F[d] = f(); };
  }
  if ((k = e.match(/^FIX F(\d+)$/))) { const a = +k[1]; f = () => Math.round(F[a]) | 0; }
  else if ((k = e.match(/^TRUNC F(\d+)$/))) { const a = +k[1]; f = () => Math.trunc(F[a]) | 0; }
  else if ((k = e.match(/^LSHIFT R(\d+) BY (-?\d+)$/))) { const a = +k[1], n = +k[2]; f = n >= 0 ? () => R[a] << n : () => R[a] >>> -n; }
  else if ((k = e.match(/^R(\d+) \* R(\d+) \(SSI\)$/))) { const a = +k[1], b = +k[2]; f = () => Math.imul(R[a], R[b]); }
  else if ((k = e.match(/^R(\d+) (AND|XOR|OR|\+|-) R(\d+)$/))) { const a = +k[1], o = k[2], b = +k[3];
    f = o === "AND" ? () => R[a] & R[b] : o === "XOR" ? () => R[a] ^ R[b] : o === "OR" ? () => R[a] | R[b] : o === "+" ? () => (R[a] + R[b]) | 0 : () => (R[a] - R[b]) | 0; }
  else if ((k = e.match(/^PASS R(\d+)$/))) { const a = +k[1]; f = () => R[a]; }
  else throw new Error("SHARC R? " + e);
  if (par) { const pd = par.d, pa = par.a; return () => { const v = f(), w = MI[pa()]; R[d] = v; R[pd] = w; }; }
  return () => { R[d] = f(); };
}
function compileLand(lines) {
  const ops = lines.map(compileLine), n = ops.length;
  return (x, y) => { R[8] = 255; F[4] = x; F[5] = y; for (let i = 0; i < n; i++) ops[i](); return F[0]; };
}

class TerrainVoice extends AudioWorkletProcessor {
  constructor() {
    super();
    this.lands = null; this.k = null; this.env = 0; this.gate = 0; this.f = 130.81;
    this.px = 0; this.py = 0; this.qx = 0; this.qy = 0; this.wy = 0; this.z = 0; this.dcx = 0; this.dcy = 0; this.odd = false;
    this.phx = 0; this.phy = 0;
    this.port.onmessage = (e) => {
      const d = e.data;
      if (d.asm) { initMem(d.asm.vn); this.lands = d.asm.banks.flat().map((s) => compileLand(s.asm)); }
      if (d.k) this.k = d.k;
      if (d.on !== undefined) { if (d.on) { this.f = d.freq; if (!this.gate) { this.phx = this.phy = 0; } this.gate = 1; } else this.gate = 0; }
    };
  }
  wave(w, ph) {                                                   // ph in turns
    let q = ph - Math.floor(ph); if (q >= 0.5) q -= 1;              // -0.5..0.5
    if (w === 0) return Math.sin(TAU * q);
    if (w === 1) return 1 - 4 * Math.abs(q);
    if (w === 2) return q >= 0 ? 1 : -1;
    return -2 * q;
  }
  pd(ph, pd) {                                                    // CZ-style bend, min of two lines (as on the page)
    const s = ((ph % 1) + 1.5) % 1, d = 0.5 - 0.48 * pd;
    return Math.min(s * 0.5 / d, 0.5 + ((s - d) * 0.5) / (1 - d)) - 0.5;
  }
  point(k) {                                                      // one point of the path, at half rate
    const sr2 = sampleRate / 2;
    this.phx += (this.f * k.rx) / sr2; this.phy += (this.f * k.ry) / sr2;
    this.phx -= Math.floor(this.phx); this.phy -= Math.floor(this.phy);
    let ax = this.phx + k.md * this.wy + k.phx, ay = this.phy + k.phy;
    if (k.pdx) ax = this.pd(ax, k.pdx);
    if (k.pdy) ay = this.pd(ay, k.pdy);
    const wx = this.wave(k.wx, ax), wy = this.wave(k.wy, ay); this.wy = wy;
    return [k.cx + k.sx * wx, k.cy + k.sy * wy];
  }
  height(k, x, y) {
    x = cl(x + k.fb * this.z, -4, 4); y = cl(y, -4, 4);
    const L = this.lands, i = k.bank * 8 + k.a;
    let z = L[i](x, y);
    if (k.f > 0 && k.a < 7) z += (L[i + 1](x, y) - z) * k.f;
    if (k.wp) z += (Math.sin(TAU * z * (0.5 + 1.5 * k.wp)) - z) * k.wp;
    this.z = z;
    return z;
  }
  process(_, outputs) {
    const out = outputs[0][0], k = this.k;
    if (!this.lands || !k) { out.fill(0); return true; }
    const att = 1 - Math.exp(-1 / (0.003 * sampleRate)), rel = 1 - Math.exp(-1 / (0.2 * sampleRate));
    for (let i = 0; i < out.length; i++) {
      let x, y;
      if (!this.odd) { [this.qx, this.qy] = [this.px, this.py]; [this.px, this.py] = this.point(k); x = (this.px + this.qx) / 2; y = (this.py + this.qy) / 2; }
      else { x = this.px; y = this.py; }
      this.odd = !this.odd;
      const s = this.height(k, x, y);
      const hp = s - this.dcx + 0.995 * this.dcy; this.dcx = s; this.dcy = hp;   // the DSP's DC high-pass
      const c = cl(hp * 0.45, -1.5, 1.5), v = c - (4 / 27) * c * c * c;           // its level and soft clip (level fitted to the DSP)
      this.env += ((this.gate ? 1 : 0) - this.env) * (this.gate ? att : rel);
      out[i] = v * this.env * 1.6;
    }
    for (let c = 1; c < outputs[0].length; c++) outputs[0][c].set(out);
    return true;
  }
}
registerProcessor("terrain-voice", TerrainVoice);
