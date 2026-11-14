/* em/pagine 2.0 (fase 3, 4/11/2026): tre pagine per motore, MANDALA, LEV in stile Elektron, nomi in inglese.
 * Parti comuni dalla 1.0. em/pagine 1.0 (fase 2, 3/11/2026): le pagine dei quattro motori nuovi, come le bozze approvate (bozze_fase2b, pagine_f2b.py).
 * Solo interi a 32 bit (niente virgola mobile e niente long long sul ColdFire). Disegna in G[64][128] (y = 0 in cima); pagine.c
 * copia nel canvas i punti accesi di x 16..127, y 9..63.
 * Pagina 1 = due gemelli (riga alta e riga bassa uguali, come OSC 1/2 di WaveTone): colonna A il nastro di TUNE di WaveTone
 * (misurato sull'emulatore), B il nome della scelta, poi la grafica. Le grafiche seguono il DSP dei moduli 1.0 (gen_*5.py).
 * w[16] = le parole delle 16 manopole del motore (pagina 1 poi pagina 2): ogni grafica puo' usare anche l'altra pagina. */
#include "tab6.h"
#pragma GCC optimize ("no-jump-tables")

static unsigned char G[64][128];
static int imin(int a, int b) { return b < a ? b : a; }
static int imax(int a, int b) { return b > a ? b : a; }
static int iabs(int a) { return a < 0 ? -a : a; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int rdiv(int a, int b) { return a >= 0 ? (a + b / 2) / b : -((-a + b / 2) / b); }   /* a / b arrotondato, b > 0 */

/* ------------------------------------------------------------------ disegno (come bozze2/bozze3) */
static void px(int x, int y, int v) { if (x >= 0 && x < 128 && y >= 0 && y < 64) G[y][x] = (unsigned char)v; }
/* v63 (rilascio): i caratteri sono quelli del firmware, letti dalla sua memoria mentre si disegna (niente copie di grafica Elektron
 * nella patch). Il carattere delle etichette e' il record a 0x40291ca0 dell'OS 1.12 (anticipo, altezza, larghezze[256],
 * scostamenti[256] (negativo = assente), bitmap: una colonna per byte, bit 3 = riga in alto). ROM8/16/32 nella prova sul computer
 * leggono dal file dell'immagine. */
#ifndef ROM8
#define ROM8(a)  (*(const volatile unsigned char *)(a))
#define ROM16(a) (*(const volatile short *)(a))
#define ROM32(a) (*(const volatile unsigned int *)(a))
#endif
#define LAB_REC 0x40291CA0u
static int fwid(int c)
{
    c &= 255;
    return ROM16(ROM32(LAB_REC + 12) + 2u * (unsigned)c) < 0 ? 2 : ROM8(ROM32(LAB_REC + 8) + (unsigned)c);
}
static int fcol(int c, int q)
{
    c &= 255;
    int o = ROM16(ROM32(LAB_REC + 12) + 2u * (unsigned)c);
    return o < 0 ? 0 : ROM8(ROM32(LAB_REC + 16) + (unsigned)(o + q));
}
static void text(int x, int y, const char *t, int v)
{
    for (; *t; t++) {
        int w = fwid((unsigned char)*t);
        for (int q = 0; q < w; q++) {
            int col = fcol((unsigned char)*t, q);
            for (int r = 0; r < 5; r++) if (col >> (3 + r) & 1) px(x + q, y + r, v);
        }
        x += w + 1;
    }
}
static int tw(const char *t) { int s = 0, n = 0; for (; *t; t++, n++) s += fwid((unsigned char)*t) + 1; return n ? s - 1 : -1; }
static void ctext(int cx, int y, const char *t, int v) { int w = tw(t); text(cx - (w >= 0 ? w / 2 : -1), y, t, v); }
static void fill(int x0, int y0, int x1, int y1, int v) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) px(x, y, v); }
static void line(int x0, int y0, int x1, int y1)
{
    int dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        px(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void tag(int x, int y, const char *t, int right)            /* scritta su fondo nero sopra la grafica */
{
    int w = tw(t), tx = right ? x - w : x;
    fill(tx - 1, y, tx + w, y + 5, 0); text(tx, y, t, 1);
}

/* ------------------------------------------------------------------ numeri: Q12 = 4096 */
static int sinq(int q12) { return ISIN[(q12 >> 2) & 1023] >> 2; }            /* sin(2 pi q), q in Q12 giri -> Q12 */
static int cosq(int q12) { return sinq(q12 + 1024); }
static int qmul(int a, int b) { return (a * b) >> 12; }                      /* a, b sotto 2^15 circa */
static int U(int w) { return clampi(w, 0, 0x7f00) * 4096 / 0x7f00; }         /* manopola 0..1 (knob_unit) -> 0..4096 */
static int BIP(int w) { return clampi((w - 0x4000) * 4096 / 0x3f00, -4096, 4096); }   /* bipolare (knob_bip) -> -4096..4096 */
static int SEMI(int w) { return rdiv(w - 0x4000, 256); }                     /* nastro: semitoni (1 scatto = 256) */
static int STEP(int w, int top) { return clampi((w + 0x80) >> 8, 0, top); }  /* scelta a scatti (knob_step) */
static int exp2q16(int x256)                                                  /* 2^(x/256) in Q16 (x in 1/256 di ottava) */
{
    int i = x256 >> 8;
    unsigned v = E2[x256 & 255];
    return (int)(i >= 0 ? (i > 14 ? 0x7fffffff : v << i) : (-i > 31 ? 0 : v >> -i));
}
static unsigned rnd_s;
static int rnd(void) { rnd_s = rnd_s * 1103515245u + 12345u; return (int)((rnd_s >> 16) & 4095); }   /* 0..4095 */

/* ------------------------------------------------------------------ la griglia */
static const int CX[4] = {33, 59, 86, 112};
static const int ZT[2][2] = {{12, 29}, {39, 56}};
static const int LY[2] = {32, 59};
#define X0C(c) (CX[c] - 11)
#define X1C(c) (CX[c] + 11)
#define Y0R(r) (ZT[r][0])
#define Y1R(r) (ZT[r][1])

/* ------------------------------------------------------------------ widget */
/* il nastro di TUNE di WaveTone: i numeri (ottave) scorrono sotto la tacca, un punto per semitono, puntini ogni 3 punti */
static const unsigned char DIG[10][5] = {   /* 3 x 5, bit 0 = colonna sinistra */
    {7, 5, 5, 5, 7}, {2, 3, 2, 2, 7}, {7, 4, 7, 1, 7}, {7, 4, 7, 4, 7}, {5, 5, 7, 4, 4},
    {7, 1, 7, 4, 7}, {7, 1, 7, 5, 7}, {7, 4, 4, 4, 4}, {7, 5, 7, 5, 7}, {7, 5, 7, 4, 7}};
static void nastro(int col, int row, int semi)
{
    int cx = CX[col], b0 = row ? 41 : 14, b1 = b0 + 14, yb = b0 + 9, xl = cx - 9, xr = cx + 9;
    line(cx - 9, b0, cx + 9, b0); line(cx - 9, b1, cx + 9, b1); line(cx - 10, b0 + 1, cx - 10, b1 - 1); line(cx + 10, b0 + 1, cx + 10, b1 - 1);
    px(cx, b0 + 1, 1); px(cx, b0 + 2, 1); px(cx, b1 - 2, 1); px(cx, b1 - 1, 1);
    for (int n = -5; n <= 5; n++) {
        int p = cx + 12 * n - semi;
        if (p < xl - 16 || p > xr + 16) continue;
        const unsigned char *d = DIG[iabs(n)];
        for (int dy = 0; dy < 5; dy++)
            for (int dx = 0; dx < 3; dx++)
                if ((d[dy] >> dx & 1) && p - 1 + dx >= xl && p - 1 + dx <= xr) px(p - 1 + dx, yb - 4 + dy, 1);
        if (n < 0) for (int k = 4; k >= 3; k--) if (p - k >= xl && p - k <= xr) px(p - k, yb - 2, 1);
        if (n < 5) for (int k = 3; k <= 9; k += 3) if (p + k >= xl && p + k <= xr) px(p + k, yb, 1);
    }
}
static void nome(int col, int row, const char *t, int n, int sel)           /* come TYPE di WaveTone: nome e puntini */
{
    ctext(CX[col], Y0R(row) + 2, t, 1);
    for (int k = 0; k < n; k++) {
        int xx = CX[col] - 2 * (n - 1) + 4 * k;
        if (k == sel) fill(xx - 1, Y1R(row) - 2, xx + 1, Y1R(row), 1); else px(xx, Y1R(row), 1);
    }
}
/* un'onda: n campioni Q12 (-4096..4096) da x0 a x1, y0..y1; asse = puntini sulla riga di mezzo */
static void onda(int x0, int x1, int y0, int y1, const short *ys, int n, int asse)
{
    int cy = (y0 + y1) / 2, a2 = y1 - y0 - 1, prev = 0, W = x1 - x0;
    if (asse) for (int x = x0; x <= x1; x += 2) px(x, cy, 1);
    for (int i = 0; i <= W; i++) {
        int y = clampi(cy - rdiv(a2 * ys[rdiv(i * (n - 1), W)], 8192), y0, y1);
        if (i && iabs(y - prev) > 1) line(x0 + i - 1, prev, x0 + i, y); else px(x0 + i, y, 1);
        prev = y;
    }
}
static short YS[400];
static void curva(int col, int row, int asse) { onda(X0C(col) + 1, X1C(col) - 1, Y0R(row) + 1, Y1R(row) - 1, YS, 100, asse); }
static void normalizza(int n)
{
    int mx = 1;
    for (int i = 0; i < n; i++) mx = imax(mx, iabs(YS[i]));
    for (int i = 0; i < n; i++) YS[i] = (short)(YS[i] * 4096 / mx);
}

/* ================================================================== parti nuove della 2.0 */
/* LEV in stile Elektron, come il LEV a sinistra dello schermo: cornice 7 x 18 con gli angoli tondi, tacche ai lati, barra di 3 */
static void lev_e(int col, int row, int q, int bip)
{
    int cx = CX[col], t = Y0R(row) + 1, b = Y1R(row);
    line(cx - 2, t, cx + 2, t); line(cx - 2, b, cx + 2, b); line(cx - 3, t + 1, cx - 3, b - 1); line(cx + 3, t + 1, cx + 3, b - 1);
    for (int y = t; y <= b; y += 4) { px(cx - 7, y, 1); px(cx - 6, y, 1); px(cx + 6, y, 1); px(cx + 7, y, 1); }
    int lo = b - 2, hi = t + 2;                             /* 11/11: piena a 100, un pixel nero dalla cornice */
    if (!bip) { int h = rdiv((lo - hi + 1) * q, 4096); if (h) fill(cx - 1, lo - h + 1, cx + 1, lo, 1); }
    else {
        int mid = (lo + hi) / 2, h = rdiv((mid - hi) * q, 4096);
        px(cx - 5, mid, 1); px(cx + 5, mid, 1);
        fill(cx - 1, imin(mid, mid - h), cx + 1, imax(mid, mid - h), 1);
    }
}

/* ================================================================== SPETTRO (pag. 1 e 2 come la 1.0, nomi in inglese) */
static int serie_top(const int *w, int s, int H)   /* 11/11: la parziale visibile piu' alta (Q8 della nota) */
{
    int c = exp2q16(SEMI(w[s * 4]) * 256 / 12) >> 8, sp = (c * (exp2q16(U(w[s * 4 + 1]) / 4 - 512) >> 4)) >> 12;
    int N = imin(64, 1 + (U(w[s * 4 + 2]) * 63 + 2048) / 4096), r = BIP(w[s * 4 + 3]), a = 3973 - iabs(r) * 9 / 10;
    int sh = rdiv(SEMI(w[10 + s * 4]) * 256, 131), g = 4096, top = c + sh;
    short gg[64]; gg[0] = 4096; for (int k = 1; k < N; k++) gg[k] = (short)qmul(gg[k - 1], a);
    for (int k = 0; k < N; k++) { g = r > 0 ? gg[N - 1 - k] : gg[k]; if (rdiv(g * H, 4096) >= 1) top = imax(top, c + k * sp + sh); }
    return top;
}
static void spettro_serie(int x0, int y0, int x1, int y1, const int *w, int s, int FMAX)
{
    const int base = y1 - 1, W = x1 - x0;
    int c = exp2q16(SEMI(w[s * 4]) * 256 / 12) >> 8;
    int sp = (c * (exp2q16(U(w[s * 4 + 1]) / 4 - 512) >> 4)) >> 12;
    int N = imin(64, 1 + (U(w[s * 4 + 2]) * 63 + 2048) / 4096);
    int r = BIP(w[s * 4 + 3]), a = 3973 - iabs(r) * 9 / 10;
    int sid = STEP(w[9 + s * 4], 4), sh = rdiv(SEMI(w[10 + s * 4]) * 256, 131);   /* 3.1: 3 PARI, 4 DISPARI */
    short g[64];
    g[0] = 4096;
    for (int k = 1; k < N; k++) g[k] = (short)qmul(g[k - 1], a);
    for (int x = x0; x <= x1; x++) px(x, base, 1);
    for (int h = 1; h <= 16; h++) px(x0 + rdiv(h * W, 16), y1, 1);
    for (int k = 0; k < N; k++) {
        int gg = r > 0 ? g[N - 1 - k] : g[k], hgt = rdiv(gg * (base - y0 - 1), 4096);
        for (int lato = 0; lato < 2; lato++) {
            int f;
            if (lato == 0) { if ((sid == 1 && k) || (sid == 3 && (k & 1)) || (sid == 4 && !(k & 1))) continue; f = c + k * sp + sh; }
            else { if (sid == 0 || sid >= 3 || !k) continue; f = iabs(c - k * sp + sh); }
            if (f < 0 || f > FMAX) continue;
            int x = x0 + rdiv(f * W, FMAX);
            for (int y = base - hgt; y < base; y++) px(x, y, 1);
        }
    }
    int fc = c + sh;
    if (fc >= 0 && fc <= FMAX) { int x = x0 + rdiv(fc * W, FMAX); px(x, y0, 1); px(x - 1, y0 - 1, 1); px(x + 1, y0 - 1, 1); }
}
static void pg_spettro1(const int *w)
{
    int H = Y1R(0) - Y0R(0) - 3;
    for (int r = 0; r < 2; r++) {
        int top = serie_top(w, r, H), FMAX = 16 << 8;
        while (FMAX < top && FMAX < (1024 << 8)) FMAX *= 2;   /* 11/11: x16, x32 .. x1024 la nota: ogni riga mostra tutte le sue parziali */
        nastro(0, r, SEMI(w[r * 4])); spettro_serie(X0C(1), Y0R(r) + 1, X1C(3), Y1R(r), w, r, FMAX); }
}
static void pg_spettro2(const int *w)
{
    static const char *const LATI[5] = {"UP", "DOWN", "BOTH", "EVEN", "ODD"};   /* 11/11: + PARI, DISPARI */
    for (int r = 0; r < 2; r++) {
        lev_e(0, r, U(w[8 + r * 4]), 0);                    /* 2.0: LEV 0..127 */
        int s = STEP(w[9 + r * 4], 4); nome(1, r, LATI[s], 5, s);
        nastro(2, r, SEMI(w[10 + r * 4]));
        int m = qmul(U(w[11 + r * 4]), 1300);               /* 2.0: PM, la fase spinta dall'altra serie */
        for (int i = 0; i < 100; i++) YS[i] = (short)sinq(i * 8192 / 99 + qmul(m, sinq(i * 12288 / 99 + 512)));
        onda(X0C(3) + 1, X1C(3) - 1, Y0R(r) + 2, Y1R(r), YS, 100, 1);
    }
}
static void g_rimbalzi(int col, int row, int q);
static void pg_spettro3(const int *w)                      /* 3.2: FDBK MIRR MDEP TILT | TWST PHAS HARM SPRD */
{
    int fb = U(w[16]), mi = U(w[17]), md = U(w[18]), tl = BIP(w[19]), tw = U(w[20]), ph = BIP(w[21]);
    { /* 2.1: FDBK MIRR MDEP insieme, su 3 caselle: le parziali (piene, larghe 2), l'asse dello specchio (freccia in cima),
       * le parziali riflesse F +- k f (a puntini, alte quanto MDEP), quelle in mezzo che nascono dal feedback (sottili) */
      int x0 = X0C(0) + 1, x1 = X1C(2) - 1, yb = Y1R(0), H = Y1R(0) - Y0R(0) - 3, W = x1 - x0;
      int F = exp2q16(rdiv(mi * 4 * 256, 4096) - 256) >> 4;       /* x0.5 .. x8 la nota, Q12 */
      int xm = x0 + rdiv(F * W, 10 * 4096);
      for (int k = 1; k <= 9; k++) {
          int h = 4096; for (int j = 1; j < k; j++) h = qmul(h, 3400);
          int x = x0 + rdiv(k * W, 10), hh = rdiv(qmul(h, 4096 - md / 2) * H, 4096);
          if (hh) fill(x, yb - hh, x + 1, yb, 1);
          if (fb) { int hf = rdiv(qmul(qmul(h, fb), 1800) * H, 4096); if (hf) line(x + 3, yb - hf, x + 3, yb); }
          if (md) for (int sg = -1; sg <= 1; sg += 2) {
              int fr = F + sg * k * 4096, xr = x0 + rdiv(iabs(fr) * W, 10 * 4096), hr = rdiv(qmul(qmul(h, md), 3000) * H, 4096);
              if (xr > x0 && xr < x1 && hr) for (int y = yb - hr; y <= yb; y += 2) { px(xr, y, 1); }
          }
      }
      line(x0, yb, x1, yb);
      if (xm >= x0 && xm <= x1) { for (int y = Y0R(0) + 3; y < yb; y += 3) px(xm, y, 1); fill(xm - 2, Y0R(0), xm + 2, Y0R(0), 1); fill(xm - 1, Y0R(0) + 1, xm + 1, Y0R(0) + 1, 1); px(xm, Y0R(0) + 2, 1); }
    }
    { int yb = Y1R(0), H = Y1R(0) - Y0R(0) - 3;              /* 3.2 TILT: sette parziali, la pendenza comune */
      line(CX[3] - 10, yb, CX[3] + 10, yb);
      for (int k = 0; k < 7; k++) { int h = clampi(rdiv(H * (2253 + qmul(tl, (k - 3) * 614)), 4096), 1, H), x = CX[3] - 9 + k * 3;
          line(x, yb - h, x, yb); } }
    for (int i = 0; i < 100; i++) { int t = i * 4096 / 99; YS[i] = (short)sinq(2 * t + qmul(qmul(tw, 1638), sinq(t / 2 + 700))); }
    curva(0, 1, 1);                                         /* TWST */
    { int cx = CX[1], cy = (Y0R(1) + Y1R(1)) / 2 + 1, r = 7, a = qmul(ph, 2048);   /* 3.2 PHAS: il quadrante, la lancetta sfasa la serie 2 */
      for (int i = 0; i < 32; i++) px(cx + rdiv(r * cosq(i * 128), 4096), cy + rdiv(r * sinq(i * 128), 4096), 1);
      fill(cx - 1, cy - r - 2, cx + 1, cy - r - 2, 1);
      line(cx, cy, cx + rdiv((r - 2) * sinq(a), 4096), cy - rdiv((r - 2) * cosq(a), 4096)); px(cx, cy, 1); }
    { int s = STEP(w[22], 1); nome(2, 1, s ? "HARM" : "FREE", 2, s); }      /* 3.2 HARM: le spaziature agganciate ai rapporti armonici */
    { int cx = CX[3], yb = Y1R(1) - 1, H = 13, d = rdiv(U(w[23]) * 9, 4096);   /* 3.2 SPRD: un ventaglio che si apre dal punto in basso */
      fill(cx - 1, yb, cx + 1, yb + 1, 1);
      line(cx, yb, cx - d, yb - H); line(cx, yb, cx + d, yb - H);
      if (d) { px(cx - d, yb - H - 1, 1); px(cx + d, yb - H - 1, 1); }
      for (int y = yb - H; y < yb - 1; y += 3) px(cx, y, 1); }
}

/* ================================================================== TERRENO 1.1 (w: BANK TERR WARP GRAN POSX POSY ZOOM MOD | TUNX
 * WAVX SIZX PDX TUNY WAVY SIZY PDY | ERSN RIDG TIDE QUAK SLOP WIND MIRR CURV) */
static int onda_asse(int wav, int ph)
{
    int q = ph & 4095;
    if (q >= 2048) q -= 4096;
    if (wav == 0) return sinq(q);
    if (wav == 1) return 4096 - 4 * iabs(q);
    if (wav == 2) return q >= 0 ? 4096 : -4096;
    return -2 * q;
}
static int pd_ph(int ph, int pd)                            /* PD sulla fase (Q12, s in 0..1): min di due rette, come il DSP */
{
    int s = ((ph & 4095) + 2048) & 4095, d = 2048 - qmul(1966, pd);
    int l1 = s * 2048 / d, l2 = 2048 + (s - d) * 2048 / (4096 - d);
    return imin(l1, l2) - 2048;
}
static int asse_v(const int *w, int s, int t12, int modph)  /* onda dell'asse s (0 x, 1 y) al tempo t (Q12 periodi) */
{
    int tun = SEMI(w[8 + s * 4]), wav = STEP(w[9 + s * 4], 3), pd = BIP(w[11 + s * 4]);   /* 2.0: PD -64..+63 */
    int ph = ((t12 * (exp2q16(tun * 256 / 12) >> 6)) >> 10) + modph;
    if (pd) ph = pd_ph(ph, pd);
    return onda_asse(wav, ph);
}
static int siz(const int *w, int s)                         /* 3.2: SIZ fisso al centro, ASPT (w[16]) apre X o Y */
{
    int a = BIP(w[16]), g = s ? imin(4096, 4096 + a) : imin(4096, 4096 - a);
    return qmul(328 + qmul(U(0x4000), 4506), g);
}
static int surf_g(int idx, int gx, int gy)
{
    gx = clampi(gx, 0, (SNG - 1) * 256 - 1); gy = clampi(gy, 0, (SNG - 1) * 256 - 1);
    int ix = gx >> 8, iy = gy >> 8, ax = gx & 255, ay = gy & 255;
    const signed char *T = SURF[idx] + iy * SNG + ix;
    int z0 = T[0] * (256 - ax) + T[1] * ax, z1 = T[SNG] * (256 - ax) + T[SNG + 1] * ax;
    return (((z0 * (256 - ay) + z1 * ay) >> 8) * 12385) >> 16;
}
static int gq(int v) { return ((clampi(v, -9830, 9830) + 9830) * 10027) >> 14; }
static int terr_z(const int *w, int x, int y)
{
    int bank = STEP(w[0], 7), t = U(w[1]) * 7, a = imin(6, t >> 12), f = t - (a << 12), gx = gq(x), gy = gq(y);
    int z = surf_g(bank * 8 + a, gx, gy);
    if (f) z += qmul(surf_g(bank * 8 + a + 1, gx, gy) - z, f);
    int wp = U(w[2]);
    if (wp) z += qmul(wp, sinq(qmul(clampi(z, -8192, 8192), 2048 + qmul(wp, 6144))) - z);
    return z;
}
static int zoomq(const int *w) { return exp2q16(rdiv(U(w[6]) * 4 - 8192, 16)) >> 4; }   /* 2^(4 ZOOM - 2), Q12 */
static int lab_alto_no;                                     /* (10/11, non piu' usato: resta a 0) */
int em_egg;                                                 /* 11/11: easter egg di TERRENO, 0 spento, 1 UGO, 2 BLSK (FUNC + destra) */
static void pg_terreno1(const int *w)
{
    static const unsigned char BAY[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    static short PX[300], PY[300];
    int bank = STEP(w[0], 7), ti = rdiv(U(w[1]) * 7, 4096), egg = em_egg - 1;   /* 11/11: UGO, BLSK con FUNC + destra */
    const int X0 = X0C(0), X1 = X1C(3), Y0 = Y0R(0) - 1, Y1 = Y1R(0) + 1, DX = 9011, DY = 3482;   /* x +-2.2, y +-0.85 */
    int gn = 12 - rdiv(U(w[3]) * 11, 4096), st = 4096 >> gn;
    if (egg >= 0)                                           /* l'easter egg nella mappa, al posto del terreno */
        for (int y = 0; y < 20; y++) for (int x = 0; x < 102; x++) if (EGGB[egg][y][x >> 3] >> (x & 7) & 1) px(X0 + 1 + x, Y0 + y, 1);
    if (egg < 0)
    for (int y = Y0; y <= Y1; y++)
        for (int x = X0; x <= X1; x++) {
            int sx = -DX + 2 * DX * (x - X0) / (X1 - X0), sy = DY - 2 * DY * (y - Y0) / (Y1 - Y0);
            if (st > 1) { sx = rdiv(sx, st) * st; sy = rdiv(sy, st) * st; }
            int z = clampi(terr_z(w, sx, sy), -4096, 4096);
            if ((z + 4096) * 8 > (BAY[y & 3][x & 3] + 4) * 4096) px(x, y, 1);
        }
    int zm = zoomq(w), szx = qmul(siz(w, 0), zm), szy = qmul(siz(w, 1), zm), cx0 = rdiv(BIP(w[4]) * DX, 4096), cy0 = BIP(w[5]);   /* 12/11 (v58e): POSX su tutta la larghezza della mappa */
    int md = qmul(U(w[7]), 1843), fb = qmul(U(w[23]), 3072), cv = rdiv(BIP(w[19]) * 3, 20);   /* 3.0: FDBK = MOD/2 + FDBK/2, CURV in 19, via MIRR */
    int phx = BIP(w[10]) / 2, phy = BIP(w[14]) / 2;                                     /* 3.2: PHSX PHSY (pag. 2) spostano le onde */
    int ang = BIP(w[17]) / 2, ca = cosq(ang), sa = sinq(ang);                              /* 3.2: ROTA -64..+63, mezzo giro indietro..avanti */
    int z = 0, oy = 0;
    for (int i = 0; i < 300; i++) {
        int t = i * 4096 / 300;
        int ox = asse_v(w, 0, t, qmul(md, oy) + phx);
        oy = asse_v(w, 1, t, phy);                                                    /* 3.2: MOD solo Y su X */
        int ax = qmul(szx, ox), ay = qmul(szy, oy);
        int x = cx0 + qmul(ax, ca) - qmul(ay, sa) + qmul(fb, z), y = cy0 + qmul(ax, sa) + qmul(ay, ca);
        if (cv) { x += qmul(qmul(qmul(cv, x), x), x); y += qmul(qmul(qmul(cv, y), y), y); }
        x = clampi(x, -16384, 16384); y = clampi(y, -16384, 16384);
        if (st > 1) { x = rdiv(x, st) * st; y = rdiv(y, st) * st; }
        z = terr_z(w, x, y);
        YS[i] = (short)clampi(z, -16384, 16384);
        PX[i] = (short)clampi(X0 + rdiv((x + DX) * (X1 - X0), 2 * DX), -10, 140);
        PY[i] = (short)clampi(Y0 + rdiv((DY - y) * (Y1 - Y0), 2 * DY), -10, 80);
    }
    if (egg < 0) {                                        /* con l'easter egg niente percorso e niente cursore sopra */
    for (int i = 0; i < 300; i++)
        for (int d = 0; d < 4; d++) {
            int xx = PX[i] + (d == 0) - (d == 1), yy = PY[i] + (d == 2) - (d == 3);
            if (xx >= X0 && xx <= X1 && yy >= Y0 && yy <= Y1) px(xx, yy, 0);
        }
    for (int i = 0; i < 300; i++) if (PX[i] >= X0 && PX[i] <= X1 && PY[i] >= Y0 && PY[i] <= Y1) px(PX[i], PY[i], 1);
    int cxp = X0 + rdiv((cx0 + DX) * (X1 - X0), 2 * DX), cyp = Y0 + rdiv((DY - cy0) * (Y1 - Y0), 2 * DY);   /* il cursore */
    for (int d = -3; d <= 3; d++) for (int e = -1; e <= 1; e += 2) { px(cxp + d, cyp + e, 0); px(cxp + e, cyp + d, 0); }
    for (int d = -3; d <= 3; d++) if (d) { px(cxp + d, cyp, 1); px(cxp, cyp + d, 1); }
    }
    tag(X0, Y0, BANKN[bank], 0); tag(X1, Y0, egg == 0 ? "UGO" : egg == 1 ? "BLSK" : TERRN[bank][ti], 1);
    normalizza(300);
    onda(X0C(0) + 1, X1C(3) - 1, Y0R(1), Y1R(1) + 1, YS, 300, 1);
}
static void pg_terreno2(const int *w)
{
    static const char *const ONDE[4] = {"SINE", "TRI", "SQR", "SAW"};
    for (int r = 0; r < 2; r++) {
        nastro(0, r, SEMI(w[8 + r * 4]));
        int s = STEP(w[9 + r * 4], 3); nome(1, r, ONDE[s], 4, s);
        int sz = siz(w, r);
        for (int i = 0; i < 200; i++) YS[i] = (short)clampi(qmul(sz, asse_v(w, r, i * 8192 / 199, BIP(w[10 + r * 4]) / 2)) * 4 / 5, -4096, 4096);   /* 3.2: con PHSX/PHSY */
        onda(X0C(2) + 1, X1C(3) - 1, Y0R(r) + 1, Y1R(r), YS, 200, 1);
    }
}
static void g_vento(int col, int row, int wd)                /* WIND di TERRENO (e ECHO di PULSAR): il cerchio spinto via, le copie a puntini */
{
    int cx = CX[col], cy = (Y0R(row) + Y1R(row)) / 2;
    for (int k2 = 0; k2 < 4; k2++) { int dx = rdiv(k2 * wd * 4, 4096) - rdiv(wd * 6, 4096);   /* 11/11: a 0 al centro */
        for (int i = 0; i < 24; i++) if (k2 == 3 || i % 3 == 0) px(cx + dx + rdiv(4 * cosq(i * 4096 / 24), 4096), cy + rdiv(4 * sinq(i * 4096 / 24), 4096), 1); }
}
static void g_poligono(int col, int row, int q);
static void g_blob(int col, int row, int bip);
/* >>> glifi_serie.inc */
/* 12/11 (v56): glifi di serie fotografati nell'emulatore valore per valore (glifi_serie.py) */
static const unsigned int GS_ANIM_I[24][21] = {
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0200000, 0x0100000, 0x0080000, 0x0040000, 0x0020000, 0x0010000, 0x0008000, 0x0004000, 0x0002000, 0x0001000, 0x0000800, 0x0000400, 0x0000200, 0x0000100, 0x0000080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0200000, 0x0100000, 0x0080000, 0x0040000, 0x0020000, 0x0010000, 0x000c000, 0x0004000, 0x0006000, 0x0001000, 0x0000800, 0x0000400, 0x0000200, 0x0000100, 0x0000080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0200000, 0x0100000, 0x0080000, 0x0040000, 0x0020000, 0x0014000, 0x000c000, 0x0004000, 0x0006000, 0x0005000, 0x0000800, 0x0000400, 0x0000200, 0x0000100, 0x0000080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0200000, 0x0100000, 0x0080000, 0x0044000, 0x002c000, 0x0014000, 0x0004000, 0x0004000, 0x0004000, 0x0005000, 0x0006800, 0x0004400, 0x0000200, 0x0000100, 0x0000080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0200000, 0x0100000, 0x0084000, 0x0044000, 0x002c000, 0x0014000, 0x0004000, 0x0004000, 0x0004000, 0x0005000, 0x0006800, 0x0004400, 0x0004200, 0x0000100, 0x0000080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0200000, 0x0100000, 0x0084000, 0x004c000, 0x0034000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0005800, 0x0006400, 0x0004200, 0x0000100, 0x0000080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0200000, 0x0104000, 0x008c000, 0x0054000, 0x0024000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004800, 0x0005400, 0x0006200, 0x0004100, 0x0000080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x0104000, 0x008c000, 0x0054000, 0x0024000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004800, 0x0005400, 0x0006200, 0x0004100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0064000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004000, 0x0004c00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0064000, 0x0004000, 0x0004000, 0x0002000, 0x0004000, 0x0008000, 0x0004000, 0x0004000, 0x0004c00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0064000, 0x0004000, 0x0002000, 0x0002000, 0x0004000, 0x0008000, 0x0008000, 0x0004000, 0x0004c00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0064000, 0x0006000, 0x0001000, 0x0002000, 0x0004000, 0x0008000, 0x0010000, 0x000c000, 0x0004c00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0064000, 0x0007000, 0x0001000, 0x0002000, 0x0004000, 0x0008000, 0x0010000, 0x001c000, 0x0004c00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0064000, 0x0007800, 0x0001000, 0x0002000, 0x0004000, 0x0008000, 0x0010000, 0x003c000, 0x0004c00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0064000, 0x0003800, 0x0001000, 0x0002000, 0x0004000, 0x0008000, 0x0010000, 0x0038000, 0x0004c00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0066000, 0x0001800, 0x0001000, 0x0002000, 0x0004000, 0x0008000, 0x0010000, 0x0030000, 0x000cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0067000, 0x0000800, 0x0001000, 0x0002000, 0x0004000, 0x0008000, 0x0010000, 0x0020000, 0x001cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0067800, 0x0000800, 0x0001000, 0x0002000, 0x0004000, 0x0008000, 0x0010000, 0x0020000, 0x003cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0067c00, 0x0000800, 0x0001000, 0x0002000, 0x0004000, 0x0008000, 0x0010000, 0x0020000, 0x007cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0067c00, 0x0000800, 0x0001000, 0x0002000, 0x000e000, 0x0008000, 0x0010000, 0x0020000, 0x007cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0067c00, 0x0000800, 0x0001000, 0x0002000, 0x001f000, 0x0008000, 0x0010000, 0x0020000, 0x007cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0067c00, 0x0000800, 0x0001000, 0x0001000, 0x003f800, 0x0010000, 0x0010000, 0x0020000, 0x007cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0067c00, 0x0000800, 0x0001000, 0x0000800, 0x007fc00, 0x0020000, 0x0010000, 0x0020000, 0x007cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0204000, 0x010c000, 0x0094000, 0x0067c00, 0x0000800, 0x0001000, 0x0008800, 0x0075c00, 0x0022000, 0x0010000, 0x0020000, 0x007cc00, 0x0005200, 0x0006100, 0x0004080, 0x0000000, 0x0000000}};
static const unsigned char GS_ANIM_V[128] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 22, 22, 22, 22, 23, 23};
static const unsigned int GS_NMOD_I[23][21] = {
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0008000, 0x07f7fc0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0008000, 0x07f7bc0, 0x0000400, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0008000, 0x07f3bc0, 0x0004400, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x008a000, 0x0751ac0, 0x0024500, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x008a200, 0x06518c0, 0x0124500, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0088200, 0x06518c0, 0x0120500, 0x0004000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0088200, 0x06510c0, 0x0100d00, 0x0024000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x000a000, 0x0080200, 0x06510c0, 0x0100900, 0x0024400, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x008a000, 0x0001200, 0x06500c0, 0x0100900, 0x0024400, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x008a000, 0x0201200, 0x04400c0, 0x0110900, 0x0024400, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0088000, 0x0201280, 0x0440040, 0x0110900, 0x0024400, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0088000, 0x0201280, 0x0440040, 0x0110800, 0x0004500, 0x0020000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0088200, 0x0201080, 0x0440040, 0x0010800, 0x0104500, 0x0020000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0082000, 0x0008200, 0x0201080, 0x0440040, 0x0010800, 0x0104100, 0x0020400, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0080000, 0x0008200, 0x0201080, 0x0440040, 0x0010800, 0x0104100, 0x0000400, 0x0020000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0080200, 0x0208000, 0x0001080, 0x0440040, 0x0010800, 0x0100100, 0x0004400, 0x0020000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0082000, 0x0000200, 0x0209000, 0x0000080, 0x0440040, 0x0010800, 0x0100100, 0x0004000, 0x0020400, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0080000, 0x0000200, 0x0209000, 0x0000080, 0x0440040, 0x0000800, 0x0110000, 0x0004100, 0x0020000, 0x0000400, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0082000, 0x0000200, 0x0008000, 0x0201080, 0x0000000, 0x0440040, 0x0000800, 0x0110000, 0x0004100, 0x0000000, 0x0020400, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0080000, 0x0000200, 0x0008080, 0x0201000, 0x0000000, 0x0440040, 0x0000800, 0x0110000, 0x0000100, 0x0004000, 0x0000000, 0x0020400, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0000000, 0x0080200, 0x0000000, 0x0008080, 0x0201000, 0x0040000, 0x0400040, 0x0000000, 0x0010800, 0x0100100, 0x0000000, 0x0004000, 0x0020400, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0080000, 0x0000200, 0x0008080, 0x0000000, 0x0201000, 0x0040000, 0x0400040, 0x0000000, 0x0000800, 0x0110100, 0x0000000, 0x0004000, 0x0000000, 0x0020400, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0002000, 0x0000000, 0x0080200, 0x0008000, 0x0000080, 0x0200000, 0x0001000, 0x0040000, 0x0400040, 0x0000000, 0x0000800, 0x0010100, 0x0100000, 0x0000000, 0x0004000, 0x0000000, 0x0020400, 0x0000000}};
static const unsigned char GS_NMOD_V[128] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22};
static const unsigned int GS_BR_I[63][21] = {
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0100000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0000000, 0x0100000, 0x0100000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0100000, 0x0100000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0111100, 0x04aaa80, 0x0011100, 0x0100000, 0x0100000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0200000, 0x0091100, 0x002aa80, 0x0111100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0511100, 0x0040000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0100000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0100000, 0x0100000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0100000, 0x0100000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0011100, 0x002aa80, 0x0111100, 0x0100000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0011100, 0x012aa80, 0x0111100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0000000, 0x0111100, 0x012aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x02aaa80, 0x0111100, 0x0100000, 0x0111100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0100000, 0x0380000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0080000, 0x0011100, 0x042aa80, 0x0111100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0040000, 0x00e0000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0411100, 0x012aa80, 0x0011100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0020000, 0x0070000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0111100, 0x002aa80, 0x0011100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0000000, 0x0010000, 0x0010000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0010000, 0x0010000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0011100, 0x001aa80, 0x0021100, 0x0010000, 0x0010000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0020000, 0x0005100, 0x0002a80, 0x0011100, 0x0040000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000000, 0x0010000, 0x0010000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0010000, 0x0010000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0010000, 0x0010000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0001100, 0x0002a80, 0x0011100, 0x0010000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0001100, 0x0012a80, 0x0011100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0010000, 0x0038000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0011100, 0x002aa80, 0x0011100, 0x0000000, 0x0011100, 0x0012a80, 0x0001100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0008000, 0x001c000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0051100, 0x000aa80, 0x0021100, 0x0010000, 0x0011100, 0x0002a80, 0x0001100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0004000, 0x000e000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0021100, 0x0002a80, 0x0001100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000000, 0x0001000, 0x0001000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0001000, 0x0001000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0001100, 0x0004680, 0x0001100, 0x0001000, 0x0001000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0001100, 0x0009280, 0x0001100, 0x0001000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0001000, 0x0000100, 0x0011280, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0001000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000000, 0x0001000, 0x0001000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0001000, 0x0001000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0001000, 0x0001000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000100, 0x0000280, 0x0001100, 0x0001000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0002a80, 0x0001100, 0x0000000, 0x0000100, 0x0001280, 0x0001100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0003800, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0001000, 0x0004100, 0x0000280, 0x0001900, 0x0001000, 0x0001100, 0x0000280, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000400, 0x0000e00, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0008000, 0x0000100, 0x0000280, 0x0001100, 0x0001400, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000200, 0x0000700, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000100, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000000, 0x0000100, 0x0000100, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000140, 0x0000000, 0x0000300, 0x0000100, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000080, 0x0000000, 0x0000000, 0x0000100, 0x0000500, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000040, 0x0000000, 0x0000100, 0x0000000, 0x0000000, 0x0000800, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000280, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0001280, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000100, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0001280, 0x0001100, 0x0000800, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000100, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0001280, 0x0001100, 0x0000800, 0x0000400, 0x0000380, 0x0000000, 0x0000100, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0001280, 0x0001100, 0x0000800, 0x0000400, 0x0000280, 0x0000100, 0x0000100, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0001280, 0x0001100, 0x0000800, 0x0000400, 0x0000380, 0x0000100, 0x0000280, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0001100, 0x0001280, 0x0001100, 0x0000800, 0x0000400, 0x0000380, 0x0000000, 0x0000280, 0x0000440, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000100, 0x0000380, 0x0000000, 0x0000000}};
static const unsigned char GS_BR_V[128] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 40, 41, 42, 42, 43, 43, 44, 44, 45, 45, 46, 46, 46, 47, 47, 48, 49, 49, 50, 50, 50, 50, 51, 51, 52, 52, 53, 53, 54, 54, 55, 55, 56, 56, 57, 57, 58, 58, 59, 59, 60, 60, 61, 61, 61, 62, 62};
static const unsigned int GS_FDBK_I[32][21] = {
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x0100100, 0x0200080, 0x0400040, 0x0400040, 0x0400040, 0x0200080, 0x0100100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x0100100, 0x0200080, 0x0400040, 0x0404040, 0x0400040, 0x0200080, 0x0100100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x0100100, 0x0200080, 0x0404040, 0x040a040, 0x0404040, 0x0200080, 0x0100100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x0100100, 0x0200080, 0x040e040, 0x040a040, 0x040e040, 0x0200080, 0x0100100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x0100100, 0x0204080, 0x040a040, 0x0411040, 0x040a040, 0x0204080, 0x0100100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x0100100, 0x020e080, 0x0411040, 0x0411040, 0x0411040, 0x020e080, 0x0100100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x0104100, 0x020a080, 0x0411040, 0x0420840, 0x0411040, 0x020a080, 0x0104100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x0104100, 0x021b080, 0x0411040, 0x0420840, 0x0411040, 0x021b080, 0x0104100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0080200, 0x010e100, 0x0211080, 0x0420840, 0x0420840, 0x0420840, 0x0211080, 0x010e100, 0x0080200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0084200, 0x010a100, 0x0211080, 0x0420840, 0x0440440, 0x0420840, 0x0211080, 0x010a100, 0x0084200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x0084200, 0x011b100, 0x0220880, 0x0420840, 0x0440440, 0x0420840, 0x0220880, 0x011b100, 0x0084200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0040400, 0x008e200, 0x0111100, 0x0220880, 0x0440440, 0x0440440, 0x0440440, 0x0220880, 0x0111100, 0x008e200, 0x0040400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x0044400, 0x008a200, 0x0111100, 0x0220880, 0x0440440, 0x0480240, 0x0440440, 0x0220880, 0x0111100, 0x008a200, 0x0044400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0020800, 0x004e400, 0x0091200, 0x0120900, 0x0240480, 0x0480240, 0x0480240, 0x0480240, 0x0240480, 0x0120900, 0x0091200, 0x004e400, 0x0020800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x0024800, 0x004a400, 0x0091200, 0x0120900, 0x0240480, 0x0480240, 0x0500140, 0x0480240, 0x0240480, 0x0120900, 0x0091200, 0x004a400, 0x0024800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x0140500, 0x0280280, 0x0500140, 0x0500140, 0x0500140, 0x0280280, 0x0140500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x0140500, 0x0280280, 0x0500140, 0x0504140, 0x0500140, 0x0280280, 0x0140500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x0140500, 0x0280280, 0x0504140, 0x050a140, 0x0504140, 0x0280280, 0x0140500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x0140500, 0x0280280, 0x050e140, 0x050a140, 0x050e140, 0x0280280, 0x0140500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x0140500, 0x0284280, 0x050a140, 0x0511140, 0x050a140, 0x0284280, 0x0140500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x0140500, 0x028e280, 0x0511140, 0x0511140, 0x0511140, 0x028e280, 0x0140500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x0144500, 0x028a280, 0x0511140, 0x0520940, 0x0511140, 0x028a280, 0x0144500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x0144500, 0x029b280, 0x0511140, 0x0520940, 0x0511140, 0x029b280, 0x0144500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a0a00, 0x014e500, 0x0291280, 0x0520940, 0x0520940, 0x0520940, 0x0291280, 0x014e500, 0x00a0a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a4a00, 0x014a500, 0x0291280, 0x0520940, 0x0540540, 0x0520940, 0x0291280, 0x014a500, 0x00a4a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00a4a00, 0x015b500, 0x02a0a80, 0x0520940, 0x0540540, 0x0520940, 0x02a0a80, 0x015b500, 0x00a4a00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00aea00, 0x0151500, 0x02a0a80, 0x0540540, 0x0540540, 0x0540540, 0x02a0a80, 0x0151500, 0x00aea00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00aea00, 0x0151500, 0x02a0a80, 0x0540540, 0x0544540, 0x0540540, 0x02a0a80, 0x0151500, 0x00aea00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00aea00, 0x0151500, 0x02a0a80, 0x0544540, 0x054a540, 0x0544540, 0x02a0a80, 0x0151500, 0x00aea00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00aea00, 0x0151500, 0x02a0a80, 0x054e540, 0x054a540, 0x054e540, 0x02a0a80, 0x0151500, 0x00aea00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00aea00, 0x0151500, 0x02a4a80, 0x054a540, 0x0551540, 0x054a540, 0x02a4a80, 0x0151500, 0x00aea00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x000e000, 0x0011000, 0x002e800, 0x0051400, 0x00aea00, 0x0151500, 0x02aea80, 0x0551540, 0x0551540, 0x0551540, 0x02aea80, 0x0151500, 0x00aea00, 0x0051400, 0x002e800, 0x0011000, 0x000e000, 0x0000000}};
static const unsigned char GS_FDBK_V[128] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 23, 23, 23, 23, 24, 24, 24, 24, 25, 25, 25, 25, 26, 26, 26, 26, 27, 27, 27, 27, 28, 28, 29, 29, 29, 29, 30, 30, 30, 30, 31, 31};
static const unsigned int GS_PAN_I[15][21] = {
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x041fc40, 0x0403c40, 0x041fc40, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0780340, 0x07e0c40, 0x07ff040, 0x07f8040, 0x07ff040, 0x07e0c40, 0x0780340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x05e0c40, 0x05ff040, 0x05f8040, 0x05ff040, 0x05e0c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x04e0c40, 0x04ff040, 0x04f8040, 0x04ff040, 0x04e0c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x047f040, 0x0478040, 0x047f040, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x043f040, 0x0438040, 0x043f040, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x041f040, 0x0418040, 0x041f040, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x041f040, 0x0408040, 0x041f040, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x041f040, 0x0404040, 0x041f040, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x041f040, 0x0402040, 0x041f040, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x041f040, 0x0403040, 0x041f040, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460c40, 0x041f840, 0x0403840, 0x041f840, 0x0460c40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460e40, 0x041fe40, 0x0403e40, 0x041fe40, 0x0460e40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x0580340, 0x0460f40, 0x041ff40, 0x0403f40, 0x041ff40, 0x0460f40, 0x0580340, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x06000c0, 0x05803c0, 0x0460fc0, 0x041ffc0, 0x0403fc0, 0x041ffc0, 0x0460fc0, 0x05803c0, 0x06000c0, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000, 0x0000000}};
static const unsigned char GS_PAN_V[128] = {1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 0, 0, 0, 0, 0, 0, 0, 0, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14};
static const unsigned int GS_ENV_I[67][21] = {
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0408040, 0x0410040, 0x0220080, 0x0240080, 0x0180100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0408040, 0x0430040, 0x0240080, 0x0280080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0408040, 0x0430040, 0x0240080, 0x0380080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0418040, 0x0420040, 0x02c0080, 0x0300080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0418040, 0x0460040, 0x0280080, 0x0300080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0418040, 0x0460040, 0x0380080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x040c040, 0x0430040, 0x04c0040, 0x0300080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x040c040, 0x0470040, 0x0780040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x040c040, 0x04f0040, 0x0700040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x041c040, 0x07e0040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x043c040, 0x07c0040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x07fc040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x07c0040, 0x043c040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x07e0040, 0x041c040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0700040, 0x04f0040, 0x040c040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0780040, 0x0470040, 0x040c040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0300080, 0x04c0040, 0x0430040, 0x040c040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0380080, 0x0460040, 0x0418040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0300080, 0x0280080, 0x0460040, 0x0418040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0380080, 0x0240080, 0x0430040, 0x0408040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0280080, 0x0240080, 0x0430040, 0x0408040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0180100, 0x0240080, 0x0220080, 0x0410040, 0x0408040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0140100, 0x0220080, 0x0210080, 0x0410040, 0x0408040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x00c0200, 0x0140100, 0x0220080, 0x0210080, 0x0410040, 0x0408040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x00c0200, 0x0120100, 0x0220080, 0x0210080, 0x0408040, 0x0408040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x00c0200, 0x0120100, 0x0210080, 0x0210080, 0x0408040, 0x0408040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x00a0200, 0x0120100, 0x0210080, 0x0208080, 0x0408040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x00a0200, 0x0110100, 0x0210080, 0x0208080, 0x0408040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0070c00, 0x0090200, 0x0110100, 0x0208080, 0x0208080, 0x0408040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0070c00, 0x0090200, 0x0108100, 0x0208080, 0x0208080, 0x0408040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0068c00, 0x0088200, 0x0108100, 0x0208080, 0x0208080, 0x0404040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0068c00, 0x0088200, 0x0108100, 0x0208080, 0x0204080, 0x0404040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0064c00, 0x0084200, 0x0104100, 0x0204080, 0x0204080, 0x0404040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0084200, 0x0100100, 0x0200080, 0x0204080, 0x0400040, 0x0400040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0062c00, 0x0082200, 0x0102100, 0x0202080, 0x0204080, 0x0404040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0062c00, 0x0082200, 0x0102100, 0x0202080, 0x0202080, 0x0404040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0061c00, 0x0081200, 0x0102100, 0x0202080, 0x0202080, 0x0402040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0061c00, 0x0081200, 0x0101100, 0x0202080, 0x0202080, 0x0402040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080a00, 0x0101100, 0x0201080, 0x0202080, 0x0402040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080a00, 0x0100900, 0x0201080, 0x0202080, 0x0402040, 0x0404040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080600, 0x0100900, 0x0201080, 0x0201080, 0x0402040, 0x0402040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080600, 0x0100900, 0x0200880, 0x0201080, 0x0402040, 0x0402040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080600, 0x0100500, 0x0200880, 0x0201080, 0x0401040, 0x0402040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100500, 0x0200880, 0x0201080, 0x0401040, 0x0402040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100300, 0x0200480, 0x0200880, 0x0401040, 0x0402040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200280, 0x0200480, 0x0401840, 0x0402040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200380, 0x0200480, 0x0401840, 0x0402040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200180, 0x0200680, 0x0400840, 0x0403040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200180, 0x0200280, 0x0400c40, 0x0403040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200380, 0x0400c40, 0x0403040, 0x0404040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200180, 0x0400640, 0x0401840, 0x0406040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x04003c0, 0x0401c40, 0x0406040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x04001c0, 0x0401e40, 0x0406040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400fc0, 0x0407040, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x04007c0, 0x0407840, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0407fc0, 0x0400040, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0407840, 0x04007c0, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0407040, 0x0400fc0, 0x0400040, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0406040, 0x0401e40, 0x04001c0, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0406040, 0x0401c40, 0x04003c0, 0x0200080, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0406040, 0x0401840, 0x0400640, 0x0200180, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0403040, 0x0400c40, 0x0200380, 0x0200080, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0403040, 0x0400c40, 0x0200280, 0x0200180, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0403040, 0x0400840, 0x0200680, 0x0200180, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0402040, 0x0401840, 0x0200480, 0x0200380, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0402040, 0x0401840, 0x0200480, 0x0200280, 0x0100100, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000},
    {0x0000000, 0x0000000, 0x0000000, 0x001f000, 0x0060c00, 0x0080200, 0x0100100, 0x0200080, 0x0200080, 0x0400040, 0x0400040, 0x0404040, 0x0402040, 0x0401040, 0x0200880, 0x0200480, 0x0100300, 0x0080220, 0x1c60c70, 0x001f020, 0x0000000}};
static const unsigned char GS_ENV_V[128] = {0, 0, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7, 8, 9, 9, 10, 10, 11, 11, 11, 11, 11, 12, 12, 13, 13, 13, 14, 15, 16, 16, 17, 17, 17, 18, 18, 19, 19, 19, 20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 26, 27, 28, 28, 29, 29, 30, 30, 31, 32, 32, 32, 33, 32, 32, 34, 35, 35, 36, 36, 37, 37, 38, 39, 39, 40, 40, 41, 41, 42, 42, 43, 43, 44, 45, 45, 46, 46, 47, 48, 48, 49, 49, 50, 50, 51, 52, 52, 53, 53, 54, 54, 54, 55, 55, 55, 55, 56, 56, 57, 57, 58, 59, 59, 60, 60, 61, 61, 62, 63, 63, 63, 64, 65, 65, 66};
static void g_serie(int col, int row, const unsigned int (*I)[21], const unsigned char *V, int w)   /* w: la parola 0..0x7f00 */
{
    const unsigned int *im = I[V[clampi(w >> 8, 0, 127)]];
    for (int y = 0; y < 21; y++) for (int x = 0; x < 27; x++) if (im[y] >> (26 - x) & 1) px(CX[col] - 13 + x, Y0R(row) - 2 + y, 1);
}
/* <<< glifi_serie.inc */
static void g_curv_onda(int col, int row, int bip)          /* CURV: wavefolder, a + l'onda si ripiega, a - si appiattisce */
{
    int g = 4096 + (bip > 0 ? bip * 2 : bip * 3 / 4);                 /* B: wavefolder: a + l'onda si ripiega, a - si appiattisce */
    for (int i = 0; i < 100; i++) {
        int v = qmul(sinq(i * 8192 / 99), g);
        for (int k = 0; k < 6 && (v > 4096 || v < -4096); k++) v = v > 4096 ? 8192 - v : -8192 - v;
        YS[i] = (short)(v * 4 / 5);
    }
    curva(col, row, 1);
}
/* 13/11 (3.3): pagine 4 e 5 di TERRAIN. Pagina 4 nello stile di FM TONE 4/4 (variante A scelta: parentesi sopra, riquadri sotto,
 * angoli smussati, celle simmetriche 17..121 larghe 21, X e Y disegnate 5x5, titoli per coppia con linee uguali). */
/* 13/11 (v62b): pagina 4 di TERRAIN identica a FM TONE 4/4 (misurata pixel per pixel nell'emulatore, 1.12 di serie) ma
 * allargata a tutta la pagina: celle larghe 23 invece di 17, 4 punti fra l'una e l'altra. Il carattere piccolo e' quello
 * della ROM (A..Z a 0x29279e del mainos 1.12, cifre a 0x2927f5: 3 colonne, bit 3 = riga in alto). */
/* il carattere piccolo (FM TONE 4/4): A..Z e 0..9 letti dalla memoria del firmware (1.12: 0x40292b9e e 0x40292bf5, 3 colonne
 * ciascuno, bit 3 = riga in alto); '-' e '.' sono nostri */
#define F3L_A 0x40292B9Eu
#define F3D_A 0x40292BF5u
static void f3g(char c, int *w, unsigned char g[3])
{
    g[0] = g[1] = g[2] = 0;
    if (c >= 'A' && c <= 'Z') { *w = 3; for (int q = 0; q < 3; q++) g[q] = ROM8(F3L_A + 3u * (unsigned)(c - 'A') + (unsigned)q); return; }
    if (c >= '0' && c <= '9') { *w = 3; for (int q = 0; q < 3; q++) g[q] = ROM8(F3D_A + 3u * (unsigned)(c - '0') + (unsigned)q); return; }
    if (c == '-') { *w = 3; g[0] = g[1] = g[2] = 0x20; return; }
    if (c == '.') { *w = 1; g[0] = 0x80; return; }
    *w = 1;
}
static int t3w(const char *t) { int s = 0, w; unsigned char g[3]; for (; *t; t++) { f3g(*t, &w, g); s += w + 1; } return s ? s - 1 : 0; }
static void t3(int x, int y, const char *t)
{
    for (; *t; t++) {
        int w; unsigned char g[3]; f3g(*t, &w, g);
        for (int q = 0; q < w; q++) for (int r = 0; r < 5; r++) if (g[q] >> (3 + r) & 1) px(x + q, y + r, 1);
        x += w + 1;
    }
}
static void c3(int cx, int y, const char *t) { t3(cx - t3w(t) / 2, y, t); }     /* come FM TONE: "0.00" (13) da cx-6 */
static void num_txt(char *b, int v, int dec)                 /* v intero; dec: due decimali (v in centesimi) */
{
    int i = 0, a = v < 0 ? -v : v;
    if (v < 0) b[i++] = '-';
    int ip = dec ? a / 100 : a, k = 0; char t[8];
    do { t[k++] = (char)('0' + ip % 10); ip /= 10; } while (ip);
    while (k) b[i++] = t[--k];
    if (dec) { b[i++] = '.'; b[i++] = (char)('0' + a / 10 % 10); b[i++] = (char)('0' + a % 10); }
    b[i] = 0;
}
#define C4W 23
static const int C4X[4] = {18, 45, 72, 99};                  /* 18..121: le stesse proporzioni di FM TONE (17 + 3) su tutta la pagina */
static void titolo3(int y, int l, int r, const char *t)       /* titolo piccolo fra due linee uguali (FM TONE: RATIO OFFSET) */
{
    int w = t3w(t), side = (r - l + 1 - w - 6) / 2, tx = l + side + 3;
    t3(tx, y, t); line(l, y + 2, l + side - 1, y + 2); line(r - side + 1, y + 2, r, y + 2);
}
static void pg_terreno4(const int *w)                        /* PFNX PFNY SYNX SYNY | PTKX PTKY DTKX DTKY */
{
    char b[12];
    for (int c = 0; c < 4; c++) {
        int x0 = C4X[c], x1 = x0 + C4W - 1, cx = x0 + C4W / 2;
        const char *xy = (c & 1) ? "Y" : "X";
        /* in alto, la parentesi di FM TONE: nome nel bordo, angoli di un punto */
        c3(cx, 11, xy);
        line(x0 + 1, 13, cx - 4, 13); line(cx + 4, 13, x1 - 1, 13);
        line(x0, 14, x0, 17); line(x1, 14, x1, 17);
        if (c < 2) {                                                         /* fine: come RATIO OFFSET, da -1.00 a +1.00 */
            int d = clampi(w[32 + c], 0, 0x7f00) - 0x4000;
            num_txt(b, d < 0 ? d * 100 / 0x4000 : d * 100 / 0x3f00, 1);
        }
        else num_txt(b, w[32 + c] >> 8, 0);                                /* sync 0..127 */
        c3(cx, 19, b);
        line(x0, 25, x0, 28); line(x1, 25, x1, 28); line(x0 + 1, 29, x1 - 1, 29);
        /* in basso, il riquadro di KEY TRACK */
        line(x0 + 1, 40, x1 - 1, 40); line(x0 + 1, 56, x1 - 1, 56);
        line(x0, 41, x0, 55); line(x1, 41, x1, 55);
        c3(cx, 42, xy);
        num_txt(b, (w[36 + c] - 0x4000) >> 8, 0);                          /* key track -64..63 */
        c3(cx, 49, b);
    }
    titolo3(32, 19, 66, "PHASE"); titolo3(32, 73, 120, "SYNC");
    titolo3(59, 19, 66, "KEY PHS"); titolo3(59, 73, 120, "KEY PD");
}
/* pagina 5 DRUM (v62b): i glifi VERI di FM DRUM, copiati dall'emulatore (drum6.h): STIM e SDEP da FM DRUM 1/4, N.HLD,
 * N.DEC, BASE/WDTH, GRAN e N.LEV da FM DRUM 4/4, spostati nelle nostre colonne. Ogni glifo cambia stato come l'originale. */
struct gdrum { unsigned char x0, y0, w, h, n, nb; const unsigned char *thr, *bits; };
#include "drum6.h"
static void gdrum(const struct gdrum *g, int v)              /* v 0..127 */
{
    int k = 0;
    while (k + 1 < g->n && v >= g->thr[k + 1]) k++;
    const unsigned char *b = g->bits + k * g->nb;
    for (int i = 0; i < g->w * g->h; i++) if (b[i >> 3] >> (7 - (i & 7)) & 1) px(g->x0 + i % g->w, g->y0 + i / g->w, 1);
}
static int gd_x(int v)                                       /* la x del ginocchio per un valore 0..127 (tabella di FM DRUM) */
{
    int k = 0;
    if (v > 127) v = 127;
    while (k + 1 < (int)sizeof GD_base_v && v >= GD_base_v[k + 1]) k++;
    int x = GD_base_x[k];
    if (k + 1 < (int)sizeof GD_base_v && GD_base_v[k + 1] > GD_base_v[k])
        x += (v - GD_base_v[k]) * (GD_base_x[k + 1] - GD_base_x[k]) / (GD_base_v[k + 1] - GD_base_v[k]);
    return x;
}
static void gd_base(int base, int wdth)                      /* BASE/WDTH di FM DRUM: il riquadro, il ginocchio sinistro (BASE) e
                                                                quello destro (BASE + WDTH), con le pendenze e i tratteggi */
{
    int xk = gd_x(base), xr = gd_x(base + wdth);
    line(22, 40, 70, 40); line(22, 56, 70, 56); line(21, 41, 21, 55); line(71, 41, 71, 55);
    line(xk, 46, xr, 46);
    if (xk > 22) {
        for (int y = 42; y < 56; y += 2) px(xk, y, 1);
        for (int y = 47, x = xk - 1; y < 56 && x >= 22; y++, x--) px(x, y, 1);
    }
    if (xr < 71) {
        for (int y = 42; y < 56; y += 2) px(xr, y, 1);
        for (int y = 47, x = xr + 1; y < 56 && x <= 70; y++, x++) px(x, y, 1);
    }
}
static void pg_terreno5(const int *w)                        /* STIM SDEP N.HLD N.DEC | BASE WDTH GRAN N.LEV */
{
#define V7(i) (clampi(w[i], 0, 0x7f00) >> 8)
    gdrum(&GD_stim, V7(24)); gdrum(&GD_sdep, V7(25)); gdrum(&GD_nhld, V7(26)); gdrum(&GD_ndec, V7(27));
    gd_base(V7(28), V7(29)); gdrum(&GD_gran, V7(30)); gdrum(&GD_nlev, V7(31));
#undef V7
}
static void pg_terreno3(const int *w)                      /* TERRAIN 3.2 (12/11): ASPT ROTA GRAN CURV | ERSN RIDG WIND FDBK */
{
    g_serie(0, 0, GS_PAN_I, GS_PAN_V, w[16]);               /* ASPT: PAN della pagina AMP, come di serie (-64 solo X, +63 solo Y) */
    g_serie(1, 0, GS_ENV_I, GS_ENV_V, w[17]);               /* ROTA: la manopola ENV della pagina FILTER, come di serie */
    g_serie(2, 0, GS_BR_I, GS_BR_V, w[18]);                 /* GRAN: BR (bit reduction) della pagina FX, come di serie */
    g_curv_onda(3, 0, BIP(w[19]));                          /* CURV: il wavefolder */
    g_serie(0, 1, GS_NMOD_I, GS_NMOD_V, w[20]);             /* ERSN: N.MOD di SWARMER, come di serie */
    g_serie(1, 1, GS_ANIM_I, GS_ANIM_V, w[21]);             /* RIDG: ANIM di SWARMER, come di serie */
    g_vento(2, 1, U(w[22]));                                /* WIND */
    g_serie(3, 1, GS_FDBK_I, GS_FDBK_V, w[23]);             /* FDBK: come FM DRUM / FM TONE, 32 disegni */
}


/* ================================================================== widget nuovi (12/11)
 * dial A-B come il MIX di FM TONE: un solo cursore che scorre da A a B (q 0..4096) */
static void dial_ab(int col, int row, int q)                /* il MIX di FM TONE, pixel per pixel: X e Y sopra, la pastiglia, il blocco dal centro */
{
    int cx = CX[col], y0 = Y0R(row) + 2;
    for (int k = 0; k < 5; k++) { px(cx - 8 + k, y0 + k, 1); px(cx - 4 - k, y0 + k, 1); }                      /* X */
    px(cx + 4, y0, 1); px(cx + 8, y0, 1); px(cx + 5, y0 + 1, 1); px(cx + 7, y0 + 1, 1); line(cx + 6, y0 + 2, cx + 6, y0 + 4);   /* Y */
    int py = y0 + 8;                                                                                            /* la pastiglia */
    line(cx - 7, py, cx + 7, py); line(cx - 7, py + 4, cx + 7, py + 4); line(cx - 8, py + 1, cx - 8, py + 3); line(cx + 8, py + 1, cx + 8, py + 3);
    px(cx, py - 1, 1); px(cx, py + 5, 1);
    int f = 2 * q - 4096;
    if (f < 0) fill(cx - 2 - rdiv(-f * 5, 4096), py + 1, cx, py + 3, 1);
    else fill(cx, py + 1, cx + 2 + rdiv(f * 3, 4096), py + 3, 1);
}
/* inviluppo come FM TONE: riquadro, attacco su, decadimento fino a END (q 0..4096 ciascuno), nodi 3 x 3, puntini sotto il picco */

/* ================================================================== PULSAR 1.1 (w: FUND BRST SKEW SUBH DENS SPAR DISF DURA | FRM1 WAV1
 * WIN1 LEV1 FRM2 WAV2 WIN2 LEV2 | CHRP TAIL GRIT REVS ECHO STUT ACNT DRFT) */
static int tab(int k, int u) { int p = imin(64, u >> 6), f = (u & 63) << 6; return PTAB[k][p] + qmul(PTAB[k][p + 1] - PTAB[k][p], f); }
static int wt(int u, int wav)                               /* la wavetable: 7 fotogrammi sfumati */
{
    int p = U(wav) * 6, i = imin(5, p >> 12), f = p - (i << 12), a = tab(i, u);
    return a + qmul(tab(i + 1, u) - a, f);
}
static int pulsaret(int u, int wav, int win, int giri)
{
    int x = win * 4, k = imin(3, x >> 12), fx = x - (k << 12), wa = tab(7 + k, u);
    return qmul(wt(giri == 2 ? (u * 2) & 4095 : u, wav), wa + qmul(tab(8 + k, u) - wa, fx));
}
static const unsigned char BURSTS[8][2] = {{1, 1}, {2, 1}, {3, 2}, {3, 1}, {4, 3}, {4, 2}, {5, 3}, {8, 5}};
static void pg_pulsar1(const int *w)
{
    static const char *const BR[8] = {"OFF", "1/2", "2/3", "1/3", "3/4", "2/4", "3/5", "5/8"};
    int br = STEP(w[1], 7);
    nastro(0, 0, SEMI(w[0]));
    nome(1, 0, BR[br], 0, 0);
    int sk = BIP(w[2]);
    for (int i = 0; i < 100; i++) { int u = i * 4096 / 99; YS[i] = (short)pulsaret(clampi(u - qmul(qmul(sk, u), 4096 - u), 0, 4096), w[9], U(w[10]), 1); }
    curva(2, 0, 1);
    int sh = U(w[3]), x0 = X0C(3), y0 = Y0R(0), y1 = Y1R(0);
    for (int k = 0; k < 5; k++) { int h = rdiv((y1 - y0 - 2) * (k % 2 ? 4096 - sh : 4096), 4096), x = x0 + 3 + k * 4; line(x, y1, x, y1 - h); px(x + 1, y1 - h, 1); }
    /* la nuvola: 5 periodi; la tacca sotto e' il posto dell'impulso; spento = cerchietto; SPAR lo stacca, DISF la larghezza, DURA doppio */
    const int P = 80, X0 = X0C(0) + 1, Y0 = Y0R(1), Y1 = Y1R(1) + 1, cyl = (Y0 + Y1) / 2;
    int dens = U(w[4]), spar = U(w[5]), disf = U(w[6]), dura = U(w[7]), fund = SEMI(w[0]);
    int on[5];
    for (int i = 0; i < 400; i++) YS[i] = 0;
    rnd_s = 3;
    for (int k = 0; k < 5; k++) {
        int r1 = rnd(), r2 = rnd() * 2 - 4096, r3 = rnd() * 2 - 4096, r4 = rnd();
        on[k] = !(k % BURSTS[br][0] >= BURSTS[br][1] || r1 >= dens);
        if (!on[k]) continue;
        int amp = k % 2 ? 4096 - sh : 4096, st = k * P + 2 + rdiv(qmul(spar, r2) * P * 45 / 100, 4096);
        int e = qmul(disf, r3) * 384 / 4096, giri = r4 < dura ? 2 : 1;
        for (int fm = 0; fm < 2; fm++) {
            int lv = qmul(amp, U(w[11 + fm * 4]));
            int len = clampi(giri * P * 4096 / imax(1, exp2q16((SEMI(w[8 + fm * 4]) - fund) * 256 / 12 + e) >> 4), 2, 4 * P);
            for (int j = 0; j < len; j++) { int x = st + j; if (x >= 0 && x < 400) YS[x] = (short)clampi(YS[x] + qmul(lv, pulsaret(j * 4096 / len, w[9 + fm * 4], U(w[10 + fm * 4]), giri)), -16384, 16384); }
        }
    }
    normalizza(400);
    onda(X0, X1C(3) - 1, Y0, Y1, YS, 400, 1);
    int Wd = X1C(3) - 1 - X0;
    for (int k = 0; k < 5; k++) {
        int X = X0 + rdiv((k * P + 2) * Wd, 399);
        px(X, Y1, 1); px(X, Y1 + 1, 1);
        if (!on[k]) { fill(X - 1, cyl - 1, X + 1, cyl + 1, 0); px(X - 1, cyl, 1); px(X + 1, cyl, 1); px(X, cyl - 1, 1); px(X, cyl + 1, 1); }
    }
}
static void wt_cella(int col, int row, int wav)
{
    static const char *const WTN[7] = {"SINE", "TRI", "SAW", "SQR", "PULS", "FORM", "NOIS"};
    int x0 = X0C(col), y0 = Y0R(row), x1 = X1C(col), y1 = Y1R(row), pos = U(wav);
    ctext(CX[col], y0 + 1, WTN[clampi(rdiv(pos * 6, 4096), 0, 6)], 1);
    for (int i = 0; i < 60; i++) YS[i] = (short)wt(i * 4096 / 59, wav);
    onda(x0 + 2, x1 - 2, y0 + 7, y1 - 3, YS, 60, 0);
    line(x0 + 2, y1, x1 - 2, y1);
    int k = x0 + 2 + rdiv((x1 - x0 - 4) * pos, 4096); fill(k - 1, y1 - 2, k + 1, y1, 1);
}
static void pg_pulsar2(const int *w)
{
    for (int r = 0; r < 2; r++) {
        nastro(0, r, SEMI(w[8 + r * 4]));
        wt_cella(1, r, w[9 + r * 4]);
        for (int i = 0; i < 100; i++) YS[i] = (short)pulsaret(i * 4096 / 99, w[9 + r * 4], U(w[10 + r * 4]), 1);
        onda(X0C(2) + 1, X1C(2) - 1, Y0R(r) + 2, Y1R(r), YS, 100, 1);
        lev_e(3, r, U(w[11 + r * 4]), 0);
    }
}
/* 11/11 (v49): pagina 3: CHRP TAIL GRIT dalla pagina EXTRA di PULSAR 4.0, REVS l'onda della 1.1, ECHO il WIND di TERRENO,
 * STUT le barrette di ECHO/STUT della 4.0, ACNT gli scalini a caso (da "_______" a "_-=_=_-"), DRFT lo stelo a Y (DRIF di WAVETONE).
 * La finestra qui e' fissa (Hann): la 1.1 non ha WIN/SKEW/TAIL della 4.0. */
static void g_scalini(int col, int row, int q)              /* ACNT: a 0 una riga piatta, su fino a scalini a caso uniti, come un LFO random */
{
    static const unsigned char H[7] = {0, 5, 10, 1, 9, 0, 5};
    int x0 = CX[col] - 10, yb = (Y0R(row) + Y1R(row)) / 2 + 6, yp = 0;
    for (int k = 0; k < 7; k++) { int y = yb - rdiv(H[k] * q, 4096); line(x0 + k * 3, y, x0 + k * 3 + 2, y); if (k) line(x0 + k * 3, yp, x0 + k * 3, y); yp = y; }
}
static void g_drif(int col, int row, int q)                  /* DRIF di WAVETONE copiato dai fotogrammi di serie (0, 16, ... 112, 127):
                                                               a 0 lo stelo a puntini, poi i punti in alto si spostano a zig-zag */
{
    static const signed char DR[7][11] = {                    /* righe yb-14 .. yb-4, spostamento in x, fotogrammi 32 .. 127 */
        {-1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {-2, 2, 1, -1, 1, -1, 0, 0, 0, 0, 0}, {-2, 2, 1, -1, 1, -1, 0, 0, 0, 0, 0},
        {-4, 4, 2, -3, 3, -2, 0, 1, -1, 0, 1}, {-6, 6, 3, -4, 4, -3, 0, 2, -2, 0, 1}, {-6, 6, 3, -4, 4, -3, 0, 2, -2, 0, 1},
        {-7, 7, 4, -5, 5, -3, 0, 2, -2, 0, 1}};
    int cx = CX[col], yb = Y1R(row) - 1, f = (rdiv(q * 127, 4096) + 8) / 16;   /* la base 3 righe sopra la scritta, come di serie */
    line(cx - 8, yb, cx + 8, yb);
    px(cx, yb - 1, 1); px(cx, yb - 2, 1);
    for (int k = 0; k < 11; k++) {
        if (f < 2) { if (!(k & 1)) px(cx, yb - 14 + k, 1); }
        else px(cx + DR[imin(f, 8) - 2][k], yb - 14 + k, 1);
    }
}
static void cerchio(int cx, int cy, int r, int pieno, int punt);
static void g_uccellino(int col, int row, int q)            /* CHRP: l'uccellino tondo; piu' CHRP, apre il becco e canta note sempre piu' in alto */
{
    static const char *const B[12] = {
        "....#####.......",
        "..##.....##.....",
        ".#.........#....",
        ".#......##..#...",
        "#.......##..#...",
        "#...........#...",
        "#..###.....#....",
        "#...###...#.....",
        ".#.......#......",
        "..##...##.......",
        "....###.........",
        "....#.#........."};
    int n = rdiv(q * 2, 4096), bx = CX[col] - 11 + (n ? 0 : 3), by = Y0R(row) + 3;
    for (int y = 0; y < 12; y++) for (int x = 0; x < 16; x++) if (B[y][x] == '#') px(bx + x, by + y, 1);
    if (!q) { px(bx + 13, by + 4, 1); px(bx + 14, by + 4, 1); }                                   /* becco chiuso */
    else { px(bx + 13, by + 3, 1); px(bx + 14, by + 2, 1); px(bx + 13, by + 5, 1); px(bx + 14, by + 5, 1); }   /* becco aperto */
    for (int j = 0; j < n; j++) { int x = bx + 16 + 4 * j, y = by + 5 - 4 * j;               /* le note: testa 2x2 e gambo */
        fill(x, y, x + 1, y + 1, 1); line(x + 1, y - 3, x + 1, y); px(x + 2, y - 3, 1); }
}
static void coda(int col, int row, int q, int rov)          /* TAIL (e REVS al contrario): il colpo e la coda che si allunga */
{
    int L = 12 + rdiv(q * 86, 4096);                          /* lunghezza della coda, su 100 punti */
    for (int i = 0; i < 100; i++) {
        int k = rov ? 99 - i : i, a;
        if (k < 3) a = 4096 * (k + 1) / 3; else a = k - 3 < L ? 4096 - (k - 3) * 4096 / L : 0;
        YS[i] = (short)qmul(a, sinq(k * 4096 * 9 / 100));
    }
    curva(col, row, 0);
}
static void pg_pulsar3(const int *w)                       /* CHRP TAIL GRIT REVS | ECHO STUT ACNT DRFT */
{
    int ch = U(w[16]), tl = U(w[17]), gr = U(w[18]), rv = U(w[19]), ec = U(w[20]), su = STEP(w[21], 3), ac = U(w[22]), dr = U(w[23]);
    g_uccellino(0, 0, ch);
    coda(1, 0, tl, 0);                                      /* TAIL */
    { int lv = 12 - rdiv(gr * 9, 4096), ns = 24 - rdiv(gr * 18, 4096);   /* GRIT: l'onda sinusoide a gradini, meno gradini e meno livelli */
      for (int i = 0; i < 100; i++) { int j = (i * ns / 100 * 2 + 1) * 4096 / (2 * ns), v = sinq(j); YS[i] = (short)(rdiv(v * lv, 4096) * 4096 / lv); }
      curva(2, 0, 0); }
    coda(3, 0, rv, 1);                                      /* REVS: la coda al contrario */
    g_vento(0, 1, ec);                                      /* ECHO: il WIND di TERRENO */
    { static const unsigned char PER[4] = {20, 10, 6, 4};   /* STUT: l'onda quadra pulita, piu' ripetizioni = piu' alta */
      int p = PER[su], x0 = CX[1] - 10, x1 = CX[1] + 10, yh = (Y0R(1) + Y1R(1)) / 2 - 4, yl = yh + 8, y = yl;
      for (int x = x0; x < x1; x += p / 2) { int ny = (y == yl) ? yh : yl, xe = imin(x + p / 2, x1);
          line(x, y, x, ny); line(x, ny, xe, ny); y = ny; } }
    g_scalini(2, 1, ac);
    g_drif(3, 1, dr);
}

/* ================================================================== MATRICE (bozze 13/11, 4a: glifi giocosi come quelli di serie)
 * Studio dei fotogrammi di serie (stock_anim.png): FDBK = rombi che si moltiplicano, GRAN = cerchio che si sbriciola, SDEP = piramide
 * che cresce, ANIM = fulmine sempre piu' rotto, TRAN = manopola, PH.C = tenda, DRIF = asta che si apre, ALGO = cifra grande.
 * w = TUNE NODO SHAP DPTH RATI SHFT DETN DAMP | (portante) SKEW MIRR STEP WRAP SUBH ALTR HARM PHAS |
 *     (modulatore) SKEW MIRR STEP WRAP SUBH ALTR FIXD CURV | ORDR XPM FBK FDMP SYNC RING SPLT BLND */
static const char *const MNODI[10] = {"PM", "ASM", "STR", "SHF", "FRM", "SNC", "PLS", "XPM", "MIR", "STP"};   /* 11/11: + SPECCHIO, SCATTI */
static const char *const MRATN[7] = {"0.50", "1.00", "2.00", "3.00", "4.00", "5.00", "7.00"};
static const char *const MSUBN[4] = {"1", "1/2", "1/3", "1/4"};
static void numero(char *t, int v, int dec)
{
    int n = 0, d[8], k = 0, a = iabs(v);
    do { d[k++] = a % 10; a /= 10; } while (a || k <= dec);
    if (v < 0) t[n++] = '-';
    while (k) { t[n++] = (char)('0' + d[--k]); if (k == dec && dec) t[n++] = '.'; }
    t[n] = 0;
}
static void ctext_clip(int cx, int y, const char *t, int c0, int c1)   /* testo centrato, solo fra le righe c0..c1 */
{
    int w = tw(t), x = cx - (w >= 0 ? w / 2 : -1);
    for (; *t; t++) { int ww = fwid((unsigned char)*t);
        for (int q = 0; q < ww; q++) { int col = fcol((unsigned char)*t, q);
            for (int r = 0; r < 5; r++) if (y + r >= c0 && y + r <= c1 && (col >> (3 + r) & 1)) px(x + q, y + r, 1); }
        x += ww + 1; }
}
static void g_rullo(int col, int row, int q, int n, const char *const *nomi)   /* NODO (11/11): il NOME rotola nella finestra (contachilometri), sotto un puntino per nodo */
{
    int f = q * (n - 1), a = imin(n - 1, f >> 12), fr = f - (a << 12), cx = CX[col], y0 = Y0R(row) + 2, y1 = y0 + 10;
    line(cx - 10, y0 - 1, cx + 10, y0 - 1); line(cx - 10, y1 + 1, cx + 10, y1 + 1); line(cx - 11, y0, cx - 11, y1); line(cx + 11, y0, cx + 11, y1);
    int ty = y0 + 3 - rdiv(fr * 9, 4096);
    ctext_clip(cx, ty, nomi[a], y0, y1);
    if (a + 1 < n) ctext_clip(cx, ty + 9, nomi[a + 1], y0, y1);
    int k = imin(n - 1, (f + 2048) >> 12), sp = n > 7 ? 2 : 3, xs = cx - (n - 1) * sp / 2;
    for (int i = 0; i < n; i++) { px(xs + i * sp, y1 + 4, 1); if (i == k) { px(xs + i * sp, y1 + 3, 1); px(xs + i * sp, y1 + 5, 1); } }
}
static void g_piramide(int col, int row, int q)              /* DPTH: la piramide di SDEP */
{
    int cx = CX[col], yb = Y1R(row) - 1, n = rdiv(q * 7, 4096);
    for (int y = Y0R(row) + 2; y <= yb; y += 2) px(cx, y, 1);
    for (int k = 0; k < n; k++) { int y = yb - 2 * k, hw = 1 + (n - k) * 8 / 7; line(cx - hw, y, cx + hw, y); }
}
static void g_tenda(int col, int row, int bip)               /* SKEW come in PULSAR: la campana dell'impulso che pende */
{
    int p = 2048 + qmul(bip, 1638);
    for (int i = 0; i < 100; i++) { int u = i * 4096 / 99, v = u < p ? u * 2048 / imax(1, p) : 2048 + (u - p) * 2048 / imax(1, 4096 - p);
        int s = sinq(v / 2); YS[i] = (short)(qmul(s, s) * 2 - 4096); }
    curva(col, row, 0);
    int x = CX[col] - 10 + rdiv(p * 20, 4096); for (int y = Y0R(row) + 4; y < Y1R(row); y += 2) px(x, y, 1);
}
static void g_molla(int col, int row, int q)                 /* WRAP: la molla, piu' giri */
{
    int cx = CX[col], cy = (Y0R(row) + Y1R(row)) / 2 + 1, n = 1 + rdiv(q * 3, 4096), W = 18;
    for (int k = 0; k < n; k++) { int ex = cx - W / 2 + (2 * k + 1) * W / (2 * n), rx = imax(2, W / (2 * n) + 1);
        for (int i = 0; i < 24; i++) px(ex + rdiv(rx * cosq(i * 171), 4096), cy + rdiv(6 * sinq(i * 171), 4096), 1); }
}
static void g_quadri(int col, int row, int alt)              /* ALTR: i domino, un giro su due si ribalta */
{
    int x0 = CX[col] - 9, cy = (Y0R(row) + Y1R(row)) / 2 + 1, d = rdiv(alt * 5, 4096);
    for (int k = 0; k < 4; k++) { int x = x0 + k * 5, up = (k & 1) ? d : 0;
        line(x, cy - 5 + up, x + 3, cy - 5 + up); line(x, cy + 5 - up + 0, x + 3, cy + 5 - up); line(x, cy - 5 + up, x, cy + 5 - up); line(x + 3, cy - 5 + up, x + 3, cy + 5 - up);
        fill(x + 1, (k & 1) ? cy - 4 + up : cy - 4, x + 2, (k & 1) ? cy - 4 + up + 2 : cy - 2, 1); }
}
static void g_manopola(int col, int row, int bip)            /* PHAS: la luna, l'ombra scorre con la fase */
{
    int cx = CX[col], cy = (Y0R(row) + Y1R(row)) / 2 + 1, r = 7, k = bip;                  /* -4096 nuova, 0 piena, +4096 nuova */
    int t = 4096 - iabs(k);                                                                 /* quanto e' piena */
    for (int y = -r; y <= r; y++) { int hw = 0; while ((hw + 1) * (hw + 1) + y * y <= r * r) hw++;
        int edge = rdiv(hw * (2 * t - 4096), 4096);                                          /* il terminatore */
        for (int x = -hw; x <= hw; x++) { int lit = k >= 0 ? x >= -edge : x <= edge; if (lit) px(cx + x, cy + y, 1); else if (x == -hw || x == hw) px(cx + x, cy + y, 1); } }
}
static void g_base(int col, int row, int bip)                /* CURV: il riquadro di BASE con la diagonale che si piega */
{
    int x0 = CX[col] - 9, x1 = CX[col] + 9, y0 = Y0R(row) + 2, y1 = Y1R(row) - 1, px0 = x0, py0 = y1;
    line(x0, y0, x1, y0); line(x0, y1, x1, y1); line(x0, y0, x0, y1); line(x1, y0, x1, y1);
    for (int i = 1; i <= 18; i++) { int t = i * 4096 / 18, c = t + qmul(bip, qmul(t, 4096 - t)); int X = x0 + i, Y = y1 - rdiv(c * (y1 - y0), 4096); line(px0, py0, X, Y); px0 = X; py0 = Y; }
}
static void g_ripartenze(int col, int row, int q)            /* SYNC: bandierine, il modulatore riparte piu' spesso */
{
    int x0 = CX[col] - 9, yb = Y1R(row) - 1, y0 = Y0R(row) + 3, n = 1 + rdiv(q * 6, 4096);
    line(x0, yb, x0 + 18, yb);
    for (int k = 0; k < n; k++) { int x = x0 + 18 * k / n; line(x, y0 + 2, x, yb); line(x, y0 + 2, x + 2, y0 + 3); px(x + 1, y0 + 4, 1); }
}
static void g_anelli(int col, int row, int q)                /* RING: i due anelli che si incastrano */
{
    int cy = (Y0R(row) + Y1R(row)) / 2 + 1, d = 6 - rdiv(q * 6, 4096);
    for (int i = 0; i < 28; i++) { px(CX[col] - d / 2 - 3 + rdiv(5 * cosq(i * 146), 4096), cy + rdiv(5 * sinq(i * 146), 4096), 1); px(CX[col] + d / 2 + 3 + rdiv(5 * cosq(i * 146), 4096), cy + rdiv(5 * sinq(i * 146), 4096), 1); }
}
static void g_blob(int col, int row, int bip)                /* SHAP: una goccia di pongo, schiacciata (-) o a stella a 4 punte (+) */
{
    int cx = CX[col], cy = (Y0R(row) + Y1R(row)) / 2 + 1, a = iabs(bip);
    for (int y = -8; y <= 8; y++) for (int x = -11; x <= 11; x++) {
        if (bip < 0) { int rx = 7 + rdiv(a * 4, 4096), ry = 6 - rdiv(a * 4, 4096); if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry) px(cx + x, cy + y + rdiv(a * 3, 4096), 1); continue; }
        int d2 = x * x + y * y;
        if (!d2) { px(cx, cy, 1); continue; }
        int c2 = x * x * 4096 / d2, c4 = 8 * qmul(c2, c2) - 8 * c2 + 4096;            /* cos(4 theta) */
        int R = 6 * 4096 + qmul(a * 6, qmul(c4, 2048));
        if (d2 * 4096 <= qmul(R, R)) px(cx + x, cy + y, 1);
    }
}
static void g_rimbalzi(int col, int row, int q)              /* DAMP: la pallina che rimbalza sempre meno */
{
    int x0 = CX[col] - 11, yb = Y1R(row) - 1, H = Y1R(row) - Y0R(row) - 4, x = x0, h = H, bx = x0, by = yb - 2;
    line(x0, yb, x0 + 22, yb);
    for (int k = 0; k < 4 && x < x0 + 20; k++) {
        int w = 4 + h / 3;
        for (int i = 0; i <= w; i += 1) { int t = i * 4096 / w, y = yb - 2 - rdiv(qmul(4 * t, 4096 - t) * h, 4096); if (i % 2 == 0) px(x + i, y, 1); }
        x += w; bx = x; by = yb - 2;
        h = rdiv(h * (4096 - q), 4096); if (h < 1) break;
    }
    fill(imin(bx, x0 + 20) - 1, by - 1, imin(bx, x0 + 20) + 1, by + 1, 1);
}
static void g_farfalla(int col, int row, int q)              /* MIRR: l'ala destra compare allo specchio (la sega diventa triangolo) */
{
    int cx = CX[col], cy = (Y0R(row) + Y1R(row)) / 2 + 1;
    line(cx, cy - 6, cx, cy + 6); px(cx - 1, cy - 8, 1); px(cx + 1, cy - 8, 1); px(cx, cy - 7, 1);
    for (int y = -7; y <= 7; y++) {                           /* le ali: lobo grande sopra, piccolo sotto */
        int w = y < 0 ? 9 - (y + 4) * (y + 4) / 3 : 6 - (y - 3) * (y - 3) / 2; if (w < 1) continue;
        for (int x = 2; x <= w + 1; x++) px(cx - x, cy + y, 1);                         /* sinistra, piena */
        int wr = rdiv(w * q, 4096); if (!wr) continue;                                  /* destra: cresce col valore */
        for (int x = 2; x <= wr + 1; x++) if (q > 3500 || x == wr + 1 || y == -7 || ((x + y) & 1) == 0) px(cx + x, cy + y, 1);
    }
}
static void g_poligono(int col, int row, int q)              /* STEP: il cerchio della fase diventa un poligono (meno passi) */
{
    int cx = CX[col], cy = (Y0R(row) + Y1R(row)) / 2 + 1, n = q ? 3 + rdiv((4096 - q) * 13, 4096) : 32;
    int px0 = 0, py0 = 0;
    for (int i = 0; i <= n; i++) { int a = i * 4096 / n - 1024, x = cx + rdiv(9 * cosq(a), 4096), y = cy + rdiv(7 * sinq(a), 4096);
        if (i) line(px0, py0, x, y);
        if (q && i < n && n <= 8) fill(x - 1, y - 1, x + 1, y + 1, 1);
        px0 = x; py0 = y; }
}
static void g_scaletta(int col, int row, int s)              /* HARM: la scaletta delle armoniche, il pioletto sul gradino s (1..8) */
{
    int cx = CX[col], yb = Y1R(row) - 1, y0 = Y0R(row) + 1;
    line(cx - 4, y0, cx - 4, yb); line(cx + 4, y0, cx + 4, yb);
    for (int k = 0; k < 8; k++) { int y = yb - 1 - 2 * k; if (k + 1 == s) { line(cx - 7, y, cx + 7, y); fill(cx + 6, y - 3, cx + 8, y - 1, 1); } else line(cx - 3, y, cx + 3, y); }
}
static void g_ancora(int col, int row, int f)                /* FIXD: l'ancora (frequenza fissa); spenta a puntini */
{
    int cx = CX[col], y0 = Y0R(row) + 1, yb = Y1R(row);
    int ay = y0 + (f ? 7 : 3);
    static const char *A[9] = {"..###..", "..#.#..", "..###..", "...#...", "#######", "...#...", "#..#..#", ".#.#.#.", "..###.."};
    for (int r = 0; r < 9 && ay + r <= yb; r++) for (int c = 0; c < 7; c++) if (A[r][c] == '#' && (f || ((r + c) & 1) == 0)) px(cx - 3 + c, ay + r, 1);
    if (f) { char t[8]; numero(t, (exp2q16(rdiv(f * 8 * 256, 4096)) >> 4) * 20 / 4096, 0); ctext(cx, y0, t, 1); }
}
static void g_valore(int col, int row, const char *tab, const char *t)   /* RATIO di FM DRUM, pixel per pixel: 17 x 15, centrato */
{
    int cx = CX[col], y0 = Y0R(row) + 2, y1 = y0 + 14, g = tab ? (tw(tab) + 1) / 2 + 1 : 0;
    if (tab) { line(cx - 7, y0, cx - 1 - g, y0); line(cx + 1 + g, y0, cx + 7, y0); ctext(cx, y0 - 2, tab, 1); }
    else line(cx - 7, y0, cx + 7, y0);
    px(cx - 8, y0 + 1, 1); px(cx - 8, y0 + 2, 1); px(cx + 8, y0 + 1, 1); px(cx + 8, y0 + 2, 1);
    px(cx - 8, y1 - 2, 1); px(cx - 8, y1 - 1, 1); px(cx + 8, y1 - 2, 1); px(cx + 8, y1 - 1, 1);
    line(cx - 7, y1, cx + 7, y1);
    int w = tw(t); text(cx - w / 2, y0 + 5, t, 1);
}
static void cerchio(int cx, int cy, int r, int pieno, int punt)
{
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) { int d = x * x + y * y; if (d > r * r + r) continue;
        int bordo = d > (r - 1) * (r - 1) + (r - 1); if ((pieno || bordo) && (!punt || ((x + y) & 1) == 0)) px(cx + x, cy + y, 1); }
}
static void g_pianeti(int c0, int c1, int row, int ord, int xp)  /* ORDR XPM: chi gira attorno a chi; XPM = la forza che li lega */
{
    int cx = (X0C(c0) + X1C(c1)) / 2, cy = (Y0R(row) + Y1R(row)) / 2 + 1;
    for (int i = 0; i < 48; i++) if (i & 1) px(cx + rdiv(16 * cosq(i * 85), 4096), cy + rdiv(6 * sinq(i * 85), 4096), 1);   /* l'orbita */
    int pull = rdiv(xp * 6, 4096);
    if (ord == 2) {                                            /* MXC: stella doppia, girano insieme */
        int d = 16 - pull, ax = cx - rdiv(d * 3482, 4096), ay = cy - rdiv(6 * 2152, 4096) + 2, bx = cx + rdiv(d * 3482, 4096), by = cy + rdiv(6 * 2152, 4096) - 2;
        cerchio(ax, ay, 3, 1, 0); cerchio(bx, by, 3, 0, 0); text(ax - 1, ay - 9 < Y0R(row) ? ay + 5 : ay - 9, "M", 1); text(bx - 1, by - 9, "C", 1);
        for (int k = 1; k < 8; k++) px(ax + (bx - ax) * k / 8, ay + (by - ay) * k / 8, xp > 0 || (k & 1));
    } else {                                                   /* MC: la luna M gira attorno a C; CM: al contrario */
        int mx = cx + rdiv((16 - pull) * 3482, 4096), my = cy - rdiv((6 - pull / 3) * 2152, 4096);
        cerchio(cx, cy, 5, 1, 0); cerchio(mx, my, 2, 1, 0);
        fill(cx - 1, cy - 2, cx + 1, cy + 2, 0); text(cx - 1, cy - 2, ord ? "M" : "C", 0);
        text(mx + 4, my - 3, ord ? "C" : "M", 1);
        if (xp) for (int k = 1; k < 5; k++) px(cx + (mx - cx) * k / 5, cy + (my - cy) * k / 5, 1);   /* il filo che tira */
    }
}
static void g_rombi_br(int c0, int c1, int row, int fb, int fd)  /* FBK FDMP: rombi uno dentro l'altro, il bordo si sbriciola */
{
    int cx = (X0C(c0) + X1C(c1)) / 2, cy = (Y0R(row) + Y1R(row)) / 2 + 1, n = 1 + rdiv(fb * 3, 4096); rnd_s = 31;
    for (int k = 0; k < n; k++) { int r = 8 - 2 * k;
        for (int y = -r; y <= r; y++) { int hw = r - iabs(y);
            for (int s = -1; s <= 1; s += 2) { int x = cx + s * hw, keep = rnd() >= qmul(fd, 4096 - k * 1200);
                if (keep) px(x, cy + y, 1); else if (fd > 1500) px(x + s * (1 + rnd() * 3 / 4096), cy + y + 1, 1); } } }
    if (fb > 3800) fill(cx - 1, cy - 1, cx + 1, cy + 1, 1);
}
static void g_binario(int c0, int c1, int row, int sh, int dt)  /* SHFT DETN: la pallina esce dalla buca; DETN la sdoppia */
{
    int x0 = X0C(c0) + 1, x1 = X1C(c1) - 1, cx = (x0 + x1) / 2, yb = Y1R(row) - 2;
    for (int x = x0; x <= x1; x++) { int k = ((x - cx) % 10 + 10) % 10, dip = k == 0 ? 2 : (k == 1 || k == 9) ? 1 : 0; px(x, yb + dip, 1); }
    int bx = cx + rdiv(sh * 9, 4096), d = rdiv(dt * 7, 4096);
    int k = ((bx - cx) % 10 + 10) % 10, by = yb - 3 + (k == 0 ? 2 : (k == 1 || k == 9) ? 1 : 0);
    if (d) { cerchio(bx - d, by, 2, 0, 1); cerchio(bx + d, by, 2, 0, 1); for (int x = bx - d + 3; x <= bx + d - 3; x += 2) px(x, by - 4, 1); }
    cerchio(bx, by, 2, 1, 0);
    if (sh) { int s = sh > 0 ? -1 : 1; for (int j = 0; j < 3; j++) px(bx + s * (5 + j * 2), by - 1 + j, 1); }
}
static void g_fetta(int col, int row, int bip)               /* SPLT: il giro e' una torta, la meta' destra salta su (o giu') */
{
    int cx = CX[col], cy = (Y0R(row) + Y1R(row)) / 2 + 1, a = iabs(bip), off = rdiv(a * 4, 4096), s = bip > 0 ? -1 : 1, r = 6;
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) { int d = x * x + y * y; if (d > r * r + r) continue;
        int bordo = d > (r - 1) * (r - 1) + (r - 1) || x == 0;
        if (x <= 0) { if (bordo) px(cx + x - (a ? 1 : 0), cy + y, 1); }
        else { int X = cx + x + (a ? 1 : 0), Y = cy + y + s * off; if (bordo || (a && ((x + y) & 1) == 0 && a > 2048)) px(X, Y, 1); } }
    if (a) for (int j = 0; j < 2; j++) px(cx + 3 + j * 2, cy - s * (r + 1) + s * off - s * 2 * j - s, 1);   /* il guizzo */
}
static void pg_matrice1(const int *w)
{
    nastro(0, 0, SEMI(w[0]));
    g_rullo(1, 0, U(w[1]), 10, MNODI);
    g_blob(2, 0, BIP(w[2]));
    g_piramide(3, 0, U(w[3]));
    g_valore(0, 1, "M", U(w[22]) ? "FIX" : MRATN[STEP(w[4], 6)]);
    g_binario(1, 2, 1, BIP(w[5]), U(w[6]));
    g_rimbalzi(3, 1, U(w[7]));
}
static void g_sotto(int col, int row, int s)                 /* SUBH: sopra i giri veloci, sotto il giro lungo che ne abbraccia s */
{
    int x0 = CX[col] - 10, y0 = Y0R(row) + 3, yb = Y1R(row) - 1, n = 4, w = 20 / n;
    for (int k = 0; k < n; k++) { int x = x0 + k * w; line(x, y0 + 4, x, y0); line(x, y0, x + w - 1, y0); }
    line(x0 + 20, y0, x0 + 20, y0 + 4);
    int L = 20 * s / n; fill(x0, yb - 1, x0 + L - 1, yb, 1);
    for (int x = x0 + L + 1; x <= x0 + 20; x += 2) px(x, yb, 1);
    char t[4] = {'1', s > 1 ? '/' : 0, (char)('0' + s), 0}; ctext(CX[col] + 0, y0 + 6, s > 1 ? t : "1", 1);
}
static void mt_fasi(const int *w, int b)
{
    g_tenda(0, 0, BIP(w[b])); g_farfalla(1, 0, U(w[b + 1])); g_poligono(2, 0, U(w[b + 2])); g_molla(3, 0, U(w[b + 3]));
    g_sotto(0, 1, STEP(w[b + 4], 3) + 1); g_quadri(1, 1, U(w[b + 5]));
}
static void pg_matrice2(const int *w)
{
    mt_fasi(w, 8);
    g_scaletta(2, 1, STEP(w[14], 7) + 1);
    g_manopola(3, 1, BIP(w[15]));
}
static void pg_matrice3(const int *w)
{
    mt_fasi(w, 16);
    g_ancora(2, 1, U(w[22]));
    g_base(3, 1, BIP(w[23]));
}
static void pg_matrice4(const int *w)
{
    g_pianeti(0, 1, 0, STEP(w[24], 2), U(w[25]));
    g_rombi_br(2, 3, 0, U(w[26]), U(w[27]));
    g_ripartenze(0, 1, U(w[28]));
    g_anelli(1, 1, U(w[29]));
    g_fetta(2, 1, BIP(w[30]));
    dial_ab(3, 1, U(w[31]));
}

/* ================================================================== le pagine */
enum { E_TERRENO, E_PULSAR, E_MATRICE, E_SPETTRO, E_N };
static const unsigned char NPAG[E_N] = {5, 3, 4, 3};   /* 13/11 (3.3): TERRAIN a 5 pagine */   /* 11/11 (v49): PULSAR 1.1 (3 pagine); SPETTRO e MATRICE fuori dal firmware */
static const char *const LAB[E_N][5][8] = {
    {{"TUNE", "BANK", "TERR", "WARP", "POSX", "POSY", "ZOOM", "MOD"}, {"TUNX", "WAVX", "PHSX", "PDX", "TUNY", "WAVY", "PHSY", "PDY"},
     {"ASPT", "ROTA", "GRAN", "CURV", "ERSN", "RIDG", "WIND", "FDBK"}, {"STIM", "SDEP", "N.HLD", "N.DEC", "BASE", "WDTH", "GRAN", "N.LEV"},
     {"", "", "", "", "", "", "", ""}},                     /* v62f: DRUM e' la pagina 4, PHASE/SYNC la 5 */
    {{"FUND", "BRST", "SKEW", "SUBH", "DENS", "SPAR", "DISF", "DURA"}, {"FRM1", "WAV1", "WIN1", "LEV1", "FRM2", "WAV2", "WIN2", "LEV2"},
     {"CHRP", "TAIL", "GRIT", "REVS", "ECHO", "STUT", "ACNT", "DRFT"}, {0}},
    {{"TUNE", "NODO", "SHAP", "DPTH", "RATI", "SHFT", "DETN", "DAMP"}, {"SKEW", "MIRR", "STEP", "WRAP", "SUBH", "ALTR", "HARM", "PHAS"},
     {"SKEW", "MIRR", "STEP", "WRAP", "SUBH", "ALTR", "FIXD", "CURV"}, {"ORDR", "XPM", "FBK", "FDMP", "SYNC", "RING", "SPLT", "BLND"}},
    {{"TUN1", "SPA1", "NUM1", "ROL1", "TUN2", "SPA2", "NUM2", "ROL2"}, {"LEV1", "SID1", "SHF1", "PM1", "LEV2", "SID2", "SHF2", "PM2"},
     {"FDBK", "MIRR", "MDEP", "TILT", "TWST", "PHAS", "HARM", "SPRD"}, {0}},
};

static void em_render6(int e, int page, const int *w0)     /* w0: 32 parole (le pagine 1..4; i motori a 3 pagine ne usano 24) */
{
    int wt[40];                                             /* 2.0: TERRENO ha TUNE in 0 e GRAN in 19 */
    const int *w = w0;
    if (e == E_TERRENO) { for (int i = 0; i < 40; i++) wt[i] = w0[i]; wt[0] = w0[1]; wt[1] = w0[2]; wt[2] = w0[3]; wt[3] = w0[18]; w = wt; }   /* 3.0: GRAN in 18 */
    if (page >= NPAG[e]) page = 0;
    lab_alto_no = 0;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++) G[y][x] = 0;
    switch (e * 8 + page) {                                 /* 3.3: 8 pagine per motore (TERRAIN ne ha 5) */
    case 0: pg_terreno1(w); break;
    case 1: pg_terreno2(w); break;
    case 2: pg_terreno3(w); break;
    case 3: pg_terreno5(w); break;                           /* v62f: DRUM a pagina 4 */
    case 4: pg_terreno4(w); break;                           /* PHASE / SYNC / KEY a pagina 5 */
    case 8: pg_pulsar1(w); break;
    case 9: pg_pulsar2(w); break;
    case 10: pg_pulsar3(w); break;
    case 16: pg_matrice1(w); break;
    case 17: pg_matrice2(w); break;
    case 18: pg_matrice3(w); break;
    case 19: pg_matrice4(w); break;
    case 24: pg_spettro1(w); break;
    case 25: pg_spettro2(w); break;
    case 26: pg_spettro3(w); break;
    default: break;
    }
    for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++) if (y < 10 || x < 16 || x > 123) G[y][x] = 0;
    for (int i = lab_alto_no ? 4 : 0; i < 8; i++) ctext(CX[i % 4], LY[i / 4], LAB[e][page][i], 1);
}
