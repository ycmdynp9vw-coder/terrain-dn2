// The little screens next to each parameter: one knob of TERRAIN turning by itself, drawn like the Digitone draws it.
// The map and the sound come from the same code as FIG. 1 (terrain-play.js); the rest are small drawings of what the knob
// does. AGPL-3.0, blskk.
import * as T from "./terrain-play.js";
const W = 102, H = 40;                                   // map (rows 0..19) and sound (rows 20..39), cropped from the page
const tri = (t) => 0.5 - 0.5 * Math.cos(t * Math.PI * 2);   // 0..1..0

// knob index in the drawing's words, and its range
const PAGE = {
  bank: [0, 0, 0x700, "step"], terr: [1, 0, 0x7f00], warp: [2, 0, 0x7f00], posx: [4, 0, 0x7f00], posy: [5, 0, 0x7f00],
  zoom: [6, 0, 0x7f00], mod: [7, 0, 0x7f00], tunx: [8, 0x4000 - 12 * 256, 0x4000 + 19 * 256, "step"], wavx: [9, 0, 0x300, "step"],
  phsx: [10, 0, 0x7f00], pdx: [11, 0, 0x7f00], tuny: [12, 0x4000 - 12 * 256, 0x4000 + 19 * 256, "step"], wavy: [13, 0, 0x300, "step"],
  phsy: [14, 0, 0x7f00], pdy: [15, 0, 0x7f00], aspt: [16, 0, 0x7f00], rota: [17, 0, 0x7f00], gran: [3, 0, 0x7f00],
  curv: [19, 0, 0x7f00], fdbk: [23, 0, 0x7f00],
  pfnx: [10, 0x3000, 0x5000], pfny: [14, 0x3000, 0x5000], ptkx: [10, 0x2000, 0x6000], ptky: [14, 0x2000, 0x6000],
  dtkx: [11, 0x2000, 0x6000], dtky: [15, 0x2000, 0x6000],
};
// a nicer starting point than the defaults, so that every knob has something to move
function base() { const w = T.defaultWords(); w[1] = 0x1200; w[6] = 0x4c00; w[12] = 0x4000 + 12 * 256; w[14] = 0x4800; return w; }

const G = new Uint8Array(128 * 64);
function pageFrame(out, w) {
  T.render(G, w);
  for (let y = 0; y < 20; y++) for (let x = 0; x < W; x++) out[y * W + x] = G[(11 + y) * 128 + 22 + x];
  for (let y = 0; y < 20; y++) for (let x = 0; x < W; x++) out[(20 + y) * W + x] = G[(39 + y) * 128 + 22 + x];
}
// small pixel helpers for the drawings
function clear(o) { o.fill(0); }
function px(o, x, y) { x |= 0; y |= 0; if (x >= 0 && x < W && y >= 0 && y < H) o[y * W + x] = 1; }
function line(o, x0, y0, x1, y1) { const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0), 1); for (let i = 0; i <= n; i++) px(o, x0 + ((x1 - x0) * i) / n, y0 + ((y1 - y0) * i) / n); }
function wave(o, ys, y0, h) { let p = null; ys.forEach((v, i) => { const y = y0 + h / 2 - (v * (h - 2)) / 2; if (p !== null) line(o, i - 1, p, i, y); p = y; }); }
function dots(o, y) { for (let x = 0; x < W; x += 2) px(o, x, y); }
// the sound of the default page, one lap, as numbers -1..1
function lap(w, n = W, cycles = 2) {
  const ys = []; let z = 0, oy = 0;
  for (let i = 0; i < n; i++) { const p = T.point(w, Math.trunc((i * 4096 * cycles) / n), oy, z); oy = p.oy; z = p.z; ys.push(z); }
  const m = Math.max(1, ...ys.map(Math.abs)); return ys.map((v) => v / m);
}
let seed = 1; const rnd = () => ((seed = (seed * 1103515245 + 12345) >>> 0) / 4294967296) * 2 - 1;

const CUSTOM = {
  tune: (o, t) => { clear(o); const k = Math.pow(2, (tri(t) * 24 - 12) / 12); dots(o, 20); wave(o, lap(base(), W, 2 * k), 2, 36); },
  ersn: (o, t) => { clear(o); const a = 1 - 0.95 * tri(t); let s = 0; const ys = lap(base(), W, 3).map((v) => (s += (v - s) * a)); dots(o, 20); wave(o, ys, 2, 36); },
  ridg: (o, t) => { clear(o); const r = tri(t); const ys = lap(base(), W, 3).map((v) => v + r * (1 - 2 * Math.abs(v) - v)); dots(o, 20); wave(o, ys, 2, 36); },
  wind: (o, t) => { const w = base(); w[4] = 0x4000 + Math.round(Math.sin(t * Math.PI * 2) * 0x3000); w[5] = 0x4000 + Math.round(Math.sin(t * Math.PI * 4) * 0x1800); pageFrame(o, w); },
  stim: (o, t) => drum(o, 0.15 + 0.8 * tri(t), 0.8),
  sdep: (o, t) => drum(o, 0.4, tri(t)),
  nhld: (o, t) => noiseEnv(o, 0.05 + 0.5 * tri(t), 0.25, 1),
  ndec: (o, t) => noiseEnv(o, 0.1, 0.05 + 0.6 * tri(t), 1),
  nlev: (o, t) => { clear(o); seed = 3; const ys = lap(base(), W, 3).map((v) => v * 0.7 + rnd() * 0.6 * tri(t)); dots(o, 20); wave(o, ys, 2, 36); },
  base: (o, t) => band(o, 0.05 + 0.6 * tri(t), 0.3),
  wdth: (o, t) => band(o, 0.2, 0.05 + 0.7 * tri(t)),
  ngrn: (o, t) => { clear(o); seed = 5; const step = 1 + Math.round(tri(t) * 9); let v = 0; const ys = []; for (let i = 0; i < W; i++) { if (i % step === 0) v = rnd(); ys.push(v * 0.9); } dots(o, 20); wave(o, ys, 2, 36); },
  synx: (o, t) => sync(o, 1 + 3 * tri(t)),
  syny: (o, t) => sync(o, 1 + 3 * tri(t)),
};
function drum(o, time, depth) {                      // the pitch drop: a curve, and the sound squeezed where the pitch is high
  clear(o); let ph = 0, p = null; const ys = [];
  for (let x = 0; x < W; x++) { const f = 1 + 6 * depth * Math.exp(-x / (W * time * 0.4)); const y = 18 - (f - 1) * 2.6; if (p !== null) line(o, x - 1, p, x, y); p = y; ph += f * 0.12; ys.push(Math.sin(ph)); }
  dots(o, 20); wave(o, ys, 21, 19);
}
function noiseEnv(o, hold, dec, lev) {
  clear(o); seed = 9; const hx = W * hold;
  for (let x = 0; x < W; x++) { const e = x < hx ? 1 : Math.exp(-(x - hx) / (W * dec * 0.5)); const a = e * lev * 17; const v = rnd() * a; line(o, x, 20 - v, x, 20 + v); px(o, x, 20 - a); px(o, x, 20 + a); }
}
function band(o, lo, w) {
  clear(o); seed = 11; for (let x = 0; x < W; x += 2) px(o, x, 38);
  const a = W * lo, b = Math.min(W - 1, a + W * w);
  for (let x = Math.round(a); x <= b; x++) { const h = 6 + Math.abs(rnd()) * 26; line(o, x, 37, x, 37 - h); }
  line(o, a, 2, a, 37); line(o, b, 2, b, 37);
}
function sync(o, r) {
  clear(o); dots(o, 20); const ys = []; for (let x = 0; x < W; x++) { const ph = ((x / W) * 2) % 1; ys.push(Math.sin(Math.PI * 2 * ((ph * r) % 1))); } wave(o, ys, 2, 36);
  for (let k = 1; k < 2; k++) for (let y = 0; y < H; y += 3) px(o, (W * k) / 2, y);
}

export function start(root = document) {
  const still = matchMedia("(prefers-reduced-motion: reduce)").matches;
  const items = [];
  root.querySelectorAll("canvas[data-demo]").forEach((c) => {
    const id = c.dataset.demo, spec = PAGE[id], draw = CUSTOM[id];
    if (!spec && !draw) return;
    c.width = W; c.height = H;
    const x = c.getContext("2d"), img = x.createImageData(W, H), o = new Uint8Array(W * H);
    const item = { c, visible: false, t0: Math.random(), frame(t) {
      const tt = (t / (spec && spec[3] ? 6 : 4)) + this.t0;
      if (draw) draw(o, tt % 1);
      else {
        const w = base(); const [i, lo, hi, step] = spec;
        let v = lo + (hi - lo) * tri(tt % 1);
        if (step) v = Math.round(v / 256) * 256;
        w[i] = Math.round(v); pageFrame(o, w);
      }
      for (let k = 0; k < o.length; k++) { const p = k * 4, on = o[k]; img.data[p] = on ? 236 : 0; img.data[p + 1] = on ? 236 : 0; img.data[p + 2] = on ? 230 : 0; img.data[p + 3] = 255; }
      x.putImageData(img, 0, 0);
    } };
    items.push(item);
  });
  const io = new IntersectionObserver((es) => es.forEach((e) => { const it = items.find((i) => i.c === e.target); if (it) it.visible = e.isIntersecting; }), { rootMargin: "80px" });
  items.forEach((i) => { io.observe(i.c); i.frame(0); });
  if (still) return;
  let last = 0;
  (function loop(now) {
    if (now - last > 90) { last = now; const t = now / 1000; for (const i of items) if (i.visible) i.frame(t); }
    requestAnimationFrame(loop);
  })(0);
}
