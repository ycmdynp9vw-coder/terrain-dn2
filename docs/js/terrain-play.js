// TERRAIN, playable: page 1 of the machine drawn as the Digitone draws it (the same maths as em/pagine's pg_terreno1),
// and a voice that does what the DSP does with the same knobs. The lands are TERRAIN's own 64 tables (img/lands.bin).
// AGPL-3.0, blskk.
const SNG = 48, DX = 9011, DY = 3482;
const CX = [33, 59, 86, 112], ZT = [[12, 29], [39, 56]], LY = [32, 59];
const X0C = (c) => CX[c] - 11, X1C = (c) => CX[c] + 11, Y0R = (r) => ZT[r][0], Y1R = (r) => ZT[r][1];
const clampi = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);
const rdiv = (a, b) => (a >= 0 ? Math.trunc((a + (b >> 1)) / b) : -Math.trunc((-a + (b >> 1)) / b));
const qmul = (a, b) => Math.floor((a * b) / 4096);
const sinq = (q) => Math.round(Math.sin((q / 4096) * 2 * Math.PI) * 4096);
const cosq = (q) => sinq(q + 1024);
const U = (w) => Math.trunc((clampi(w, 0, 0x7f00) * 4096) / 0x7f00);
const BIP = (w) => clampi(Math.trunc(((w - 0x4000) * 4096) / 0x3f00), -4096, 4096);
const SEMI = (w) => rdiv(w - 0x4000, 256);
const STEP = (w, top) => clampi((w + 0x80) >> 8, 0, top);
export const BANKN = ["CLSC", "HARM", "EDGE", "CHAO", "FM", "FOLD", "BITS", "CHEB"];
export const TERRN = [["SINE", "GRID", "FOLD", "WAVE", "RING", "SADL", "MULT", "NOIS"], ["ORGN", "OCTV", "ODD", "FUND", "CLAR", "EGHT", "CHOR", "BRIT"],
  ["CRST", "RHOM", "PYRA", "STEP", "FOLD", "BLAD", "TRIA", "TETH"], ["SPIR", "INTF", "VORT", "LOGI", "SHRD", "DENS", "STAT", "NOI2"],
  ["FM11", "FM12", "FM32", "FMXY", "FBCK", "RING", "INDX", "NOIS"], ["FLD1", "FLD2", "FLD4", "ASIM", "RADI", "SINF", "CROS", "DEEP"],
  ["STEP", "STRX", "CHEK", "PULS", "XORB", "SAWQ", "STAI", "CRSH"], ["T2", "T3", "T5", "T2T3", "T3T5", "ODD", "EVEN", "FULL"]];
export const WAVN = ["SIN", "TRI", "SQR", "SAW"];

// the knob words, in em/pagine's order for page 1 (w[0..23]); TUNE is kept apart
export const KNOBS = [
  { id: "tune", lab: "TUNE", min: 0x400, max: 0x7c00, def: 0x4000, step: 256, kind: "semi" },
  { id: "bank", lab: "BANK", min: 0, max: 0x700, def: 0, step: 256, kind: "bank", w: 0 },
  { id: "terr", lab: "TERR", min: 0, max: 0x7f00, def: 0, step: 256, kind: "terr", w: 1 },
  { id: "warp", lab: "WARP", min: 0, max: 0x7f00, def: 0, step: 256, kind: "u", w: 2 },
  { id: "posx", lab: "POSX", min: 0, max: 0x7f00, def: 0x4000, step: 256, kind: "b", w: 4 },
  { id: "posy", lab: "POSY", min: 0, max: 0x7f00, def: 0x4000, step: 256, kind: "b", w: 5 },
  { id: "zoom", lab: "ZOOM", min: 0, max: 0x7f00, def: 0x4000, step: 256, kind: "u", w: 6 },
  { id: "mod", lab: "MOD", min: 0, max: 0x7f00, def: 0, step: 256, kind: "u", w: 7 },
  { id: "tunx", lab: "TUNX", min: 0x400, max: 0x7c00, def: 0x4000, step: 256, kind: "semi", w: 8 },
  { id: "wavx", lab: "WAVX", min: 0, max: 0x300, def: 0, step: 256, kind: "wav", w: 9 },
  { id: "phsx", lab: "PHSX", min: 0, max: 0x7f00, def: 0x4000, step: 256, kind: "b", w: 10 },
  { id: "pdx", lab: "PDX", min: 0, max: 0x7f00, def: 0x4000, step: 256, kind: "b", w: 11 },
  { id: "tuny", lab: "TUNY", min: 0x400, max: 0x7c00, def: 0x4000, step: 256, kind: "semi", w: 12 },
  { id: "wavy", lab: "WAVY", min: 0, max: 0x300, def: 0, step: 256, kind: "wav", w: 13 },
  { id: "phsy", lab: "PHSY", min: 0, max: 0x7f00, def: 0x4000, step: 256, kind: "b", w: 14 },
  { id: "pdy", lab: "PDY", min: 0, max: 0x7f00, def: 0x4000, step: 256, kind: "b", w: 15 },
];
export function shown(k, v) {
  if (k.kind === "semi") { const s = SEMI(v); return (s > 0 ? "+" : "") + s; }
  if (k.kind === "bank") return BANKN[STEP(v, 7)];
  if (k.kind === "terr") return String(Math.round(v / 256));
  if (k.kind === "wav") return WAVN[STEP(v, 3)];
  if (k.kind === "b") return String(Math.round((v - 0x4000) / 256));
  return String(Math.round(v / 256));
}

let SURF = null;
export async function loadLands(url) { SURF = new Int8Array(await (await fetch(url)).arrayBuffer()); return SURF; }

// ---- the maths of pg_terreno1 (integers where the device uses them) ----
function surfG(idx, gx, gy) {
  gx = clampi(gx, 0, (SNG - 1) * 256 - 1); gy = clampi(gy, 0, (SNG - 1) * 256 - 1);
  const ix = gx >> 8, iy = gy >> 8, ax = gx & 255, ay = gy & 255, o = idx * SNG * SNG + iy * SNG + ix;
  const z0 = SURF[o] * (256 - ax) + SURF[o + 1] * ax, z1 = SURF[o + SNG] * (256 - ax) + SURF[o + SNG + 1] * ax;
  return Math.floor((Math.floor((z0 * (256 - ay) + z1 * ay) / 256) * 12385) / 65536);
}
const gq = (v) => Math.floor(((clampi(v, -9830, 9830) + 9830) * 10027) / 16384);
function terrZ(w, x, y) {
  const bank = STEP(w[0], 7), t = U(w[1]) * 7, a = Math.min(6, t >> 12), f = t - (a << 12), gx = gq(x), gy = gq(y);
  let z = surfG(bank * 8 + a, gx, gy);
  if (f) z += qmul(surfG(bank * 8 + a + 1, gx, gy) - z, f);
  const wp = U(w[2]);
  if (wp) z += qmul(wp, sinq(qmul(clampi(z, -8192, 8192), 2048 + qmul(wp, 6144))) - z);
  return z;
}
function ondaAsse(wav, ph) {
  let q = ph & 4095; if (q >= 2048) q -= 4096;
  if (wav === 0) return sinq(q);
  if (wav === 1) return 4096 - 4 * Math.abs(q);
  if (wav === 2) return q >= 0 ? 4096 : -4096;
  return -2 * q;
}
function pdPh(ph, pd) {
  const s = (((ph & 4095) + 2048) & 4095), d = 2048 - qmul(1966, pd);
  const l1 = Math.trunc((s * 2048) / d), l2 = 2048 + Math.trunc(((s - d) * 2048) / (4096 - d));
  return Math.min(l1, l2) - 2048;
}
function asseV(w, s, t12, modph) {
  const tun = SEMI(w[8 + s * 4]), wav = STEP(w[9 + s * 4], 3), pd = BIP(w[11 + s * 4]);
  let ph = Math.floor(t12 * Math.pow(2, tun / 12)) + modph;
  if (pd) ph = pdPh(ph, pd);
  return ondaAsse(wav, ph);
}
function siz(w, s) { const a = BIP(w[16]), g = s ? Math.min(4096, 4096 + a) : Math.min(4096, 4096 - a); return qmul(328 + qmul(U(0x4000), 4506), g); }
const zoomq = (w) => Math.round(4096 * Math.pow(2, (4 * U(w[6])) / 4096 - 2));

// one point of the path at time t12 (Q12 turns of the note); st: GRAN step
export function point(w, t12, oyPrev, zPrev) {
  const zm = zoomq(w), szx = qmul(siz(w, 0), zm), szy = qmul(siz(w, 1), zm), cx0 = rdiv(BIP(w[4]) * DX, 4096), cy0 = BIP(w[5]);
  const md = qmul(U(w[7]), 1843), fb = qmul(U(w[23]), 3072), cv = rdiv(BIP(w[19]) * 3, 20);
  const phx = Math.trunc(BIP(w[10]) / 2), phy = Math.trunc(BIP(w[14]) / 2), ang = Math.trunc(BIP(w[17]) / 2), ca = cosq(ang), sa = sinq(ang);
  const ox = asseV(w, 0, t12, qmul(md, oyPrev) + phx), oy = asseV(w, 1, t12, phy);
  const ax = qmul(szx, ox), ay = qmul(szy, oy);
  let x = cx0 + qmul(ax, ca) - qmul(ay, sa) + qmul(fb, zPrev), y = cy0 + qmul(ax, sa) + qmul(ay, ca);
  if (cv) { x += qmul(qmul(qmul(cv, x), x), x); y += qmul(qmul(qmul(cv, y), y), y); }
  x = clampi(x, -16384, 16384); y = clampi(y, -16384, 16384);
  return { x, y, oy, z: terrZ(w, x, y) };
}
export function words(vals) { // vals: knob id -> word; the rest at their defaults
  const w = new Array(40).fill(0x4000);
  for (const k of KNOBS) if (k.w !== undefined) w[k.w] = vals[k.id];
  w[3] = 0; w[16] = 0x4000; w[17] = 0x4000; w[19] = 0x4000; w[23] = 0;
  return w;
}

// ---- a small 3x5 font of our own for the labels and the tags ----
const F3 = { A: "010101111101101", B: "110101110101110", C: "011100100100011", D: "110101101101110", E: "111100110100111", F: "111100110100100",
  G: "011100101101011", H: "101101111101101", I: "111010010010111", J: "001001001101010", K: "101101110101101", L: "100100100100111",
  M: "101111111101101", N: "110101101101101", O: "010101101101010", P: "110101110100100", Q: "010101101110011", R: "110101110101101",
  S: "011100010001110", T: "111010010010010", U: "101101101101111", V: "101101101101010", W: "101101111111101", X: "101101010101101",
  Y: "101101010010010", Z: "111001010100111", 0: "111101101101111", 1: "010110010010111", 2: "110001010100111", 3: "110001010001110",
  4: "101101111001001", 5: "111100110001110", 6: "011100110101010", 7: "111001010010010", 8: "010101010101010", 9: "010101011001110",
  ".": "000000000000010", "+": "000010111010000", "-": "000000111000000" };
const tw = (t) => (t.length ? t.length * 4 - 1 : 0);

export function render(G, w) { // G: Uint8Array(128 * 64)
  G.fill(0);
  const px = (x, y, v) => { if (x >= 0 && x < 128 && y >= 0 && y < 64) G[y * 128 + x] = v; };
  const text = (x, y, t, v) => { for (const ch of t) { const g = F3[ch]; if (g) for (let r = 0; r < 5; r++) for (let q = 0; q < 3; q++) if (g[r * 3 + q] === "1") px(x + q, y + r, v); x += 4; } };
  const fill = (x0, y0, x1, y1, v) => { for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) px(x, y, v); };
  const tag = (x, y, t, right) => { const ww = tw(t), tx = right ? x - ww : x; fill(tx - 1, y, tx + ww, y + 5, 0); text(tx, y, t, 1); };
  const BAY = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];
  const bank = STEP(w[0], 7), ti = rdiv(U(w[1]) * 7, 4096);
  const X0 = X0C(0), X1 = X1C(3), Y0 = Y0R(0) - 1, Y1 = Y1R(0) + 1;
  for (let y = Y0; y <= Y1; y++) for (let x = X0; x <= X1; x++) {
    const sx = -DX + Math.trunc((2 * DX * (x - X0)) / (X1 - X0)), sy = DY - Math.trunc((2 * DY * (y - Y0)) / (Y1 - Y0));
    const z = clampi(terrZ(w, sx, sy), -4096, 4096);
    if ((z + 4096) * 8 > (BAY[y & 3][x & 3] + 4) * 4096) px(x, y, 1);
  }
  const PX = [], PY = [], YS = [];
  let z = 0, oy = 0;
  for (let i = 0; i < 300; i++) {
    const p = point(w, Math.trunc((i * 4096) / 300), oy, z); oy = p.oy; z = p.z;
    YS.push(clampi(z, -16384, 16384));
    PX.push(clampi(X0 + rdiv((p.x + DX) * (X1 - X0), 2 * DX), -10, 140));
    PY.push(clampi(Y0 + rdiv((DY - p.y) * (Y1 - Y0), 2 * DY), -10, 80));
  }
  for (let i = 0; i < 300; i++) for (let d = 0; d < 4; d++) {
    const xx = PX[i] + (d === 0) - (d === 1), yy = PY[i] + (d === 2) - (d === 3);
    if (xx >= X0 && xx <= X1 && yy >= Y0 && yy <= Y1) px(xx, yy, 0);
  }
  for (let i = 0; i < 300; i++) if (PX[i] >= X0 && PX[i] <= X1 && PY[i] >= Y0 && PY[i] <= Y1) px(PX[i], PY[i], 1);
  const cx0 = rdiv(BIP(w[4]) * DX, 4096), cy0 = BIP(w[5]);
  const cxp = X0 + rdiv((cx0 + DX) * (X1 - X0), 2 * DX), cyp = Y0 + rdiv((DY - cy0) * (Y1 - Y0), 2 * DY);   // the cursor
  for (let d = -3; d <= 3; d++) for (let e = -1; e <= 1; e += 2) { px(cxp + d, cyp + e, 0); px(cxp + e, cyp + d, 0); }
  for (let d = -3; d <= 3; d++) if (d) { px(cxp + d, cyp, 1); px(cxp, cyp + d, 1); }
  tag(X0, Y0, BANKN[bank], 0); tag(X1, Y0, TERRN[bank][ti], 1);
  let mx = 1; for (const v of YS) mx = Math.max(mx, Math.abs(v));
  for (let i = 0; i < YS.length; i++) YS[i] = Math.trunc((YS[i] * 4096) / mx);
  { // onda(): the sound under the map
    const x0 = X0C(0) + 1, x1 = X1C(3) - 1, y0 = Y0R(1), y1 = Y1R(1) + 1, cy = (y0 + y1) >> 1, a2 = y1 - y0 - 1, W = x1 - x0;
    for (let x = x0; x <= x1; x += 2) px(x, cy, 1);
    let prev = 0;
    for (let i = 0; i <= W; i++) {
      const y = clampi(cy - rdiv(a2 * YS[rdiv(i * 299, W)], 8192), y0, y1);
      if (i && Math.abs(y - prev) > 1) { const s = y > prev ? 1 : -1; for (let yy = prev; yy !== y; yy += s) px(x0 + i - (Math.abs(yy - prev) < Math.abs(y - yy) ? 1 : 0), yy, 1); px(x0 + i, y, 1); }
      else px(x0 + i, y, 1);
      prev = y;
    }
  }
  for (let y = 0; y < 64; y++) for (let x = 0; x < 128; x++) if (y < 10 || x < 16 || x > 123) G[y * 128 + x] = 0;
  const labs = ["TUNE", "BANK", "TERR", "WARP", "POSX", "POSY", "ZOOM", "MOD"];
  for (let i = 0; i < 8; i++) { const t = labs[i]; text(CX[i % 4] - (tw(t) >> 1), LY[i >> 2], t, 1); }
  return { cxp, cyp, X0, X1, Y0, Y1 };
}
export function posFromPixel(x, y) { // map pixel -> POSX, POSY words
  const X0 = X0C(0), X1 = X1C(3), Y0 = Y0R(0) - 1, Y1 = Y1R(0) + 1;
  const sx = -DX + (2 * DX * (x - X0)) / (X1 - X0), sy = DY - (2 * DY * (y - Y0)) / (Y1 - Y0);
  const bx = clampi((sx * 4096) / DX, -4096, 4096), by = clampi(sy, -4096, 4096);
  return [clampi(Math.round(0x4000 + (bx * 0x3f00) / 4096), 0, 0x7f00), clampi(Math.round(0x4000 + (by * 0x3f00) / 4096), 0, 0x7f00)];
}

// the voice's settings from the knobs (sent to the worklet), in surface units (Q12 / 4096) and turns
export function voiceParams(w) {
  const zm = zoomq(w), t = U(w[1]) * 7, a = Math.min(6, t >> 12);
  return {
    bank: STEP(w[0], 7), a, f: (t - (a << 12)) / 4096, wp: U(w[2]) / 4096, fb: qmul(U(w[23]), 3072) / 4096,
    cx: rdiv(BIP(w[4]) * DX, 4096) / 4096, cy: BIP(w[5]) / 4096, sx: qmul(siz(w, 0), zm) / 4096, sy: qmul(siz(w, 1), zm) / 4096,
    md: qmul(U(w[7]), 1843) / 4096,
    rx: Math.pow(2, SEMI(w[8]) / 12), wx: STEP(w[9], 3), phx: Math.trunc(BIP(w[10]) / 2) / 4096, pdx: BIP(w[11]) / 4096,
    ry: Math.pow(2, SEMI(w[12]) / 12), wy: STEP(w[13], 3), phy: Math.trunc(BIP(w[14]) / 2) / 4096, pdy: BIP(w[15]) / 4096,
  };
}
export const tuneSemi = (v) => SEMI(v);
