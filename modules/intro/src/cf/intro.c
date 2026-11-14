/* em/intro: l'animazione di accensione approvata il 3/11 (bozza_intro8.py): un terminale lancia ./BLSKK.SH (righe pescate a caso,
 * motori in ordine a caso, barra SUMMONING/CONJURING/INVOKING), la barra si rompe, tempesta ASCII, la sagoma di "blskk" riempita di
 * caratteri che salta, ogni lettera si solidifica in pixel, poi il cursore lampeggia. A ogni accensione una variante diversa: il seme
 * viene dal contatore del timer DMA 3 (il tick a 30 Hz del sistema, acceso fin dall'avvio) e dal tick globale.
 *
 * Come (firmware 1.11): la routine dell'intro 0x400d39ea(fotogramma, lunghezza, pannello) al fotogramma 0 prepara la bitmap
 * sorgente (logo); dal 5 disegna il tunnel punto per punto con setPixel(pannello, x, y, colore) 0x40113e70 dopo aver pulito il
 * pannello (0x40115074). v22: sul Digitone vero (video dell'utente, v21) l'immagine usciva capovolta in verticale (b -> p),
 * nell'emulatore dritta: si scrive la riga 63 - y. Prove nell'emulatore del 3/11: solo quello che passa da setPixel arriva allo schermo (la copia a blocchi
 * dei fotogrammi 1..4 no). Quindi noi (intro_hook.s): il fotogramma 0 resta di serie; dall'1 puliamo il pannello e scriviamo
 * ogni punto del nostro fotogramma con lo stesso setPixel, e la routine di serie non gira. Il menu STARTUP e il bootloader non
 * si toccano: e' codice del sistema operativo, caricato dal loader di reset di digikit prima dell'intro. */
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#ifndef HOST
#define CLEAR    ((void (*)(void *, int, int))0x40115074u)          /* (pannello, 0, 1) come la routine di serie */
#define SETPIX   ((void (*)(void *, int, int, int))0x40113e70u)     /* (pannello, x, y, colore 0/1) */
#define DTCN3    (*(volatile u32 *)0xfc07c00cu)
#define TICK     (*(volatile u32 *)0x466768b0u)
#define CICLO    (*(volatile u8 *)0x405ce8b2u)                    /* 14/11: byte della coda delle impostazioni (dopo 0x405ce8b1) */
#endif

enum { W = 128, H = 64, CW = 4, CH = 6, COLS = 32, ROWS = 10, OY = 2, PIENA = 1 };
static u8 FB[H][W], TEX[H][W], SOL[H][W], SOG[ROWS][COLS];
static u8 GR[ROWS][COLS];                       /* la griglia: 0 vuota, 1 cella piena, altrimenti il codice ASCII del glifo */
static u32 rs = 1;
static u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static int rn(int n) { return (int)(rnd() % (u32)n); }
static int prob(int permille) { return (int)(rnd() % 1000u) < permille; }

/* ---- glifi 3 x 5 (15 bit, riga in alto = bit alti) ---- */
#define G5(a, b, c, d, e) (u16)(((a) << 12) | ((b) << 9) | ((c) << 6) | ((d) << 3) | (e))
static u16 GL[128];
static void glifi(void)
{
    GL['#'] = G5(5, 7, 5, 7, 5); GL['@'] = G5(7, 5, 7, 4, 7); GL['%'] = G5(5, 1, 2, 4, 5); GL['&'] = G5(2, 5, 2, 5, 3);
    GL['*'] = G5(0, 5, 2, 5, 0); GL['+'] = G5(0, 2, 7, 2, 0); GL['='] = G5(0, 7, 0, 7, 0); GL['/'] = G5(1, 1, 2, 4, 4);
    GL['\\'] = G5(4, 4, 2, 1, 1); GL['|'] = G5(2, 2, 2, 2, 2); GL['<'] = G5(1, 2, 4, 2, 1); GL['>'] = G5(4, 2, 1, 2, 4);
    GL['!'] = G5(2, 2, 2, 0, 2); GL['?'] = G5(7, 1, 2, 0, 2); GL['$'] = G5(3, 6, 2, 3, 6); GL['0'] = G5(7, 5, 5, 5, 7);
    GL['1'] = G5(2, 6, 2, 2, 7); GL[';'] = G5(0, 2, 0, 2, 4); GL[':'] = G5(0, 2, 0, 2, 0); GL['-'] = G5(0, 0, 7, 0, 0);
    GL['_'] = G5(0, 0, 0, 0, 7); GL['~'] = G5(0, 3, 6, 0, 0); GL['^'] = G5(2, 5, 0, 0, 0); GL['X'] = G5(5, 5, 2, 5, 5);
    GL['8'] = G5(7, 5, 7, 5, 7); GL['.'] = G5(0, 0, 0, 0, 2); GL['{'] = G5(3, 2, 6, 2, 3); GL['}'] = G5(6, 2, 3, 2, 6);
    GL['A'] = G5(2, 5, 7, 5, 5); GL['B'] = G5(6, 5, 6, 5, 6); GL['C'] = G5(3, 4, 4, 4, 3); GL['D'] = G5(6, 5, 5, 5, 6);
    GL['E'] = G5(7, 4, 6, 4, 7); GL['F'] = G5(7, 4, 6, 4, 4); GL['G'] = G5(3, 4, 5, 5, 3); GL['H'] = G5(5, 5, 7, 5, 5);
    GL['I'] = G5(7, 2, 2, 2, 7); GL['J'] = G5(1, 1, 1, 5, 2); GL['K'] = G5(5, 5, 6, 5, 5); GL['L'] = G5(4, 4, 4, 4, 7);
    GL['M'] = G5(5, 7, 7, 5, 5); GL['N'] = G5(6, 5, 5, 5, 5); GL['O'] = G5(2, 5, 5, 5, 2); GL['P'] = G5(6, 5, 6, 4, 4);
    GL['R'] = G5(6, 5, 6, 5, 5); GL['S'] = G5(3, 4, 2, 1, 6); GL['T'] = G5(7, 2, 2, 2, 2); GL['U'] = G5(5, 5, 5, 5, 7);
    GL['V'] = G5(5, 5, 5, 5, 2); GL['W'] = G5(5, 5, 7, 7, 5); GL['Y'] = G5(5, 5, 2, 2, 2); GL['4'] = G5(5, 5, 7, 1, 1);
    GL['['] = G5(6, 4, 4, 4, 6); GL[']'] = G5(3, 1, 1, 1, 3);
}
static const char TUTTI[] = "#@%&*+=/\\|<>!?$01;:-_~^X8.{}";
static const char LEGGERI[] = "./:;-_~^+=!|<>{}";
static const char PESANTI[] = "#@%&$8X0*";
#define NT (int)(sizeof TUTTI - 1)
#define NL (int)(sizeof LEGGERI - 1)
#define NP (int)(sizeof PESANTI - 1)

static void cella(u8 (*b)[W], int c, int r, int ch)
{
    if (c < 0 || c >= COLS || r < 0 || r >= ROWS || !ch) return;
    int x0 = c * CW, y0 = OY + r * CH;
    if (ch == PIENA) { for (int y = 0; y < CH - 1; y++) for (int x = 0; x < CW - 1; x++) b[y0 + y][x0 + x] = 1; return; }
    u16 g = GL[ch & 127];
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 3; x++)
            if (g >> (14 - 3 * y - x) & 1) b[y0 + y][x0 + x] = 1;
}
static void pulisci(u8 (*b)[W]) { for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) b[y][x] = 0; }
static void griglia_vuota(void) { for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) GR[r][c] = 0; }
static void griglia_caso(int permille, const char *set, int n)
{
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) GR[r][c] = prob(permille) ? (u8)set[rn(n)] : 0;
}
static void ruota_riga(int r, int d)
{
    u8 t[COLS];
    d %= COLS; if (d < 0) d += COLS;
    for (int c = 0; c < COLS; c++) t[(c + d) % COLS] = GR[r][c];
    for (int c = 0; c < COLS; c++) GR[r][c] = t[c];
}
static void strappa(int quante, int amp) { for (int i = 0; i < quante; i++) ruota_riga(rn(ROWS), rn(2 * amp + 1) - amp); }
static void disegna(u8 (*b)[W]) { for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) cella(b, c, r, GR[r][c]); }
static void inverti_riga(int r) { for (int y = OY + r * CH; y < OY + r * CH + CH && y < H; y++) for (int x = 0; x < W; x++) FB[y][x] ^= 1; }

/* ---- la scritta "blskk": caratteri 7 x 9 ingranditi x 3, con le varianti glitch 8 ! $ X ---- */
static const char *const LG[4][9] = {
    {"##.....", "##.....", "##.....", "######.", "##...##", "##...##", "##...##", "##...##", "######."},
    {"##", "##", "##", "##", "##", "##", "##", "##", "##"},
    {".......", ".......", ".......", ".######", "##.....", ".#####.", ".....##", ".....##", "######."},
    {"##....", "##....", "##....", "##..##", "##.##.", "####..", "##.##.", "##..##", "##..##"}};
static const char *const LV[4][9] = {
    {".#####.", "##...##", "##...##", ".#####.", "##...##", "##...##", "##...##", "##...##", ".#####."},
    {"##", "##", "##", "##", "##", "##", "..", "##", "##"},
    {"...#...", "...#...", "...#...", ".######", "##.#...", ".#####.", "...#.##", "...#.##", "######."},
    {"##..##", "##..##", "##..##", ".####.", "..##..", ".####.", "##..##", "##..##", "##..##"}};
static const u8 LETT[5] = {0, 1, 2, 3, 3};     /* b l s k k */
static const u8 LARG[4] = {7, 2, 7, 6};
enum { SC = 3, GAP = 2, NLET = 5 };
static int XL[NLET], X0, Y0, XCUR;
static void posizioni(void)
{
    int tot = 0;
    for (int i = 0; i < NLET; i++) tot += LARG[LETT[i]] + (i ? GAP : 0);
    X0 = (W - tot * SC) / 2; Y0 = (H - 9 * SC) / 2;
    int x = X0;
    for (int i = 0; i < NLET; i++) { XL[i] = x; x += (LARG[LETT[i]] + GAP) * SC; }
    XCUR = X0 + tot * SC + 4;
}
/* modo 0: pieno; 1: tessitura (TEX); 2: pieno dove SOL < q, tessitura altrove; prima si cancella la sagoma */
static void lettera(int i, int var, int dx, int dy, int modo, int q)
{
    const char *const *g = var ? LV[LETT[i]] : LG[LETT[i]];
    int w = LARG[LETT[i]];
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < w; c++) {
            if (g[r][c] != '#') continue;
            for (int yy = 0; yy < SC; yy++)
                for (int xx = 0; xx < SC; xx++) {
                    int x = XL[i] + dx + c * SC + xx, y = Y0 + dy + r * SC + yy;
                    if (x < 0 || x >= W || y < 0 || y >= H) continue;
                    u8 v = modo == 0 ? 1 : modo == 1 ? TEX[y][x] : (SOL[y][x] < q ? 1 : TEX[y][x]);
                    FB[y][x] = v;
                }
        }
}

/* ---- il terminale ---- */
static const char *const MAZZO[6] = {"LOADING HEXES...", "CASTING SPELLS...", "BINDING DAEMONS...", "OPENING THE GATE...",
                                     "DECODING SIGILS...", "WAKING THE OSCILLATORS..."};
static const char *const MOTORI[4] = {"SPETTRO", "TERRENO", "MANDALA", "PULSAR"};
static const char *const BARRE[3] = {"SUMMONING [", "CONJURING [", "INVOKING  ["};
static const char CMD[] = "$ ./BLSKK.SH";
enum { LCMD = sizeof CMD - 1, NRIGHE = 7 };    /* comando, 2 righe del mazzo, 4 motori */
static char RIGHE[NRIGHE][32];
static const char *BARRA;
static int T_PRO, T_TEMP, T_LETT, T_SOLI, T_FINE, VAR;

static void scrivi(int r, const char *s, int n)
{
    for (int c = 0; c < n && s[c] && c < COLS; c++) GR[r][c] = s[c] == ' ' ? 0 : (u8)s[c];
}
static void prepara(u32 seme)
{
    rs = seme ? seme : 0x2C9E4F17u;
    for (int i = 0; i < 8; i++) rnd();
    glifi(); posizioni();
    int a = rn(6), b = rn(5); if (b >= a) b++;
    if (b < a) { int t = a; a = b; b = t; }
    int ord[4];                                 /* (assegnati uno a uno: un inizializzatore diventerebbe una chiamata a memcpy) */
    for (int i = 0; i < 4; i++) ord[i] = i;
    for (int i = 3; i > 0; i--) { int j = rn(i + 1), t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    for (int r = 0; r < NRIGHE; r++) for (int c = 0; c < 32; c++) RIGHE[r][c] = 0;
    for (int c = 0; c < LCMD; c++) RIGHE[0][c] = CMD[c];
    for (int c = 0; MAZZO[a][c]; c++) RIGHE[1][c] = MAZZO[a][c];
    for (int c = 0; MAZZO[b][c]; c++) RIGHE[2][c] = MAZZO[b][c];
    for (int i = 0; i < 4; i++) {
        const char *m = MOTORI[ord[i]]; int c = 0;
        for (; m[c]; c++) RIGHE[3 + i][c] = m[c];
        for (; c < 11; c++) RIGHE[3 + i][c] = ' ';
        RIGHE[3 + i][11] = '['; RIGHE[3 + i][12] = 'O'; RIGHE[3 + i][13] = 'K'; RIGHE[3 + i][14] = ']';
    }
    BARRA = BARRE[rn(3)];
    T_PRO = 2 * (4 + (LCMD - 2) + 2 + 2 * (NRIGHE - 1) + 11);   /* fine del prologo (4/11: terminale a meta' velocita', piu' lungo) */
    T_TEMP = T_PRO + 4; T_LETT = T_TEMP + 12; T_SOLI = T_LETT + 16; T_FINE = T_SOLI + 20;   /* glitch piu' corto */
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) SOL[y][x] = (u8)rnd();
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) SOG[r][c] = (u8)rnd();
    VAR = rn(3);                                /* 4/11: quale intro dopo il terminale: 0 glitch, 1 RITO, 2 SEGNALE */
}
static void prologo(int t)
{
    griglia_vuota();
    if (t < 4) { scrivi(0, "$", 1); if (t % 2 == 0) GR[0][2] = PIENA; return; }
    if (t < 4 + LCMD - 2) { int n = 2 + (t - 4) + 1; scrivi(0, CMD, n); GR[0][n] = PIENA; return; }
    scrivi(0, CMD, LCMD);
    int k = t - (4 + LCMD - 2), righe = 1;
    if (k < 2) { GR[1][0] = PIENA; return; }
    for (int i = 1; i < NRIGHE; i++) if (k >= 2 + 2 * (i - 1)) { scrivi(i, RIGHE[i], 32); righe = i + 1; }
    int j = k - (2 + 2 * (NRIGHE - 1));
    if (j >= 0) {
        int piene = j < 10 ? j : 10, c = 0;
        for (; BARRA[c]; c++) if (BARRA[c] != ' ') GR[righe][c] = (u8)BARRA[c];
        for (int i = 0; i < 10; i++) GR[righe][c + i] = i < piene ? '#' : '.';
        GR[righe][c + 10] = ']';
        righe++;
    }
    if ((t / 2) % 2 == 0 && righe < ROWS) GR[righe][0] = PIENA;
}


/* ---- 4/11: due intro in piu' (bozze_intro_nuove, approvate): dopo il terminale, a caso, il glitch (0), RITO (1) o SEGNALE (2);
 * il finale (blskk a pixel e il cursore) e' lo stesso. Solo interi. ---- */
static int sn(int a) { a &= 1023; int x = a & 511, p = x * (512 - x), v = 4096 * p / (1310720 - 4 * p); return a < 512 ? v : -v; }   /* onda sinusoide (Bhaskara), 1024 = un giro, +-256 */
static int cs(int a) { return sn(a + 256); }
static void pt(int x, int y) { if (x >= 0 && x < W && y >= 0 && y < H) FB[y][x] = 1; }
static void lin(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, n = dx > dy ? dx : dy;
    if (!n) { pt(x0, y0); return; }
    for (int i = 0; i <= n; i++) pt(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
}
static void cerchio(int cx, int cy, int r, int per1000, int passo)
{
    int n = 256 * per1000 / 1000;
    for (int i = 0; i < n; i += passo) pt(cx + r * cs(i * 4) / 256, cy - r * sn(i * 4) / 256);
}
static void stella(int cx, int cy, int r, int rot, int per1000)          /* il pentagramma {5/2}, tracciato fino a per1000 */
{
    int tot = 5 * per1000;
    for (int j = 0; j < 5 && tot > 0; j++, tot -= 1000) {
        int a0 = rot + 256 + j * 2048 / 5, a1 = a0 + 2048 / 5;
        int x0 = cx + r * cs(a0) / 256, y0 = cy - r * sn(a0) / 256, x1 = cx + r * cs(a1) / 256, y1 = cy - r * sn(a1) / 256;
        int f = tot >= 1000 ? 1000 : tot;
        lin(x0, y0, x0 + (x1 - x0) * f / 1000, y0 + (y1 - y0) * f / 1000);
    }
}
static void glifo(int x0, int y0, int ch)
{
    u16 g = GL[ch & 127];
    for (int y = 0; y < 5; y++) for (int x = 0; x < 3; x++) if (g >> (14 - 3 * y - x) & 1) pt(x0 + x, y0 + y);
}
static void scritta_appare(int q)                /* la scritta a punti: q 0..256 */
{
    pulisci(TEX);
    for (int i = 0; i < NLET; i++) lettera(i, 0, 0, 0, 2, q);
}
static void rito(int s, int D)
{
    int k = s * 1000 / D, cx = W / 2, cy = H / 2;
    if (k < 230) {                               /* i punti cardinali e il cerchio che si traccia */
        cerchio(cx, cy, 28, k * 1000 / 230, 3);
        for (int a = 0; a < 1024; a += 256) { int x = cx + 30 * cs(a) / 256, y = cy - 30 * sn(a) / 256; for (int d = -1; d <= 1; d++) { pt(x + d, y - 1); pt(x + d, y); pt(x + d, y + 1); } }
    } else if (k < 460) {                        /* il cerchio chiuso, il pentagramma si traccia */
        cerchio(cx, cy, 28, 1000, 1); cerchio(cx, cy, 20, 1000, 4);
        stella(cx, cy, 28, 0, (k - 230) * 1000 / 230);
    } else if (k < 690) {                        /* la stella gira, l'anello esterno, i glifi attorno */
        int rot = (k - 460) * 2;
        cerchio(cx, cy, 28, 1000, 1); cerchio(cx, cy, 31, 1000, 6); stella(cx, cy, 28, rot, 1000);
        for (int i = 0; i < 6; i++) { int a = -rot * 2 + i * 1024 / 6; glifo(cx + 38 * cs(a) / 256 - 1, cy - 27 * sn(a) / 256 - 2, "BLSKK*"[i]); }
    } else if (k < 850) {                        /* la figura collassa al centro, la scritta appare */
        int p = (k - 690) * 1000 / 160;
        stella(cx, cy, 28 - 22 * p / 1000, 300 + p, 1000); cerchio(cx, cy, 31, 1000, 9);
        scritta_appare(p * 256 / 1000);
    } else {                                     /* blskk e l'anello sottile */
        for (int i = 0; i < NLET; i++) lettera(i, 0, 0, 0, 0, 0);
        cerchio(cx, cy, 31, 1000, 12);
    }
}
static void segnale(int s, int D)
{
    int k = s * 1000 / D, cy = H / 2;
    for (int x = 0; x < W; x += 8) pt(x, cy);
    for (int y = 0; y < H; y += 8) pt(W / 2, y);
    if (k < 230) {                               /* onda sinusoide pulita che scorre */
        for (int x = 1; x < W; x++) lin(x - 1, cy - 14 * sn((x - 1) * 24 + s * 40) / 256, x, cy - 14 * sn(x * 24 + s * 40) / 256);
    } else if (k < 460) {                        /* entrano le armoniche: dente di sega */
        int nh = 1 + (k - 230) * 6 / 230, py = cy;
        for (int x = 0; x < W; x++) {
            int v = 0; for (int h = 1; h <= nh; h++) v += sn((x * 24 + s * 40) * h) / h;
            int y = cy - 11 * v / 256; if (x) lin(x - 1, py, x, y); py = y;
        }
    } else if (k < 650) {                        /* si rompe: tagli, salti, righe spostate */
        int py = cy;
        for (int x = 0; x < W; x++) {
            int y = cy - 18 * sn(x * 24 + s * 40 + (x % 17 < 4 ? rn(700) : 0)) / 256 + (x % 23 < 3 ? rn(17) - 8 : 0);
            y = y < 0 ? 0 : y > H - 1 ? H - 1 : y;
            if (x && (py - y < 20 && y - py < 20)) lin(x - 1, py, x, y); else pt(x, y);
            py = y;
        }
        for (int i = 0; i < 3; i++) { int yy = rn(H), d = rn(25) - 12; u8 tmp[W]; for (int x = 0; x < W; x++) tmp[(x + d + W) % W] = FB[yy][x]; for (int x = 0; x < W; x++) FB[yy][x] = tmp[x]; }
    } else if (k < 850) {                        /* l'onda si raccoglie nella scritta */
        int p = (k - 650) * 1000 / 200, a = 4 + 10 * (1000 - p) / 1000;
        for (int x = 1; x < W; x++) lin(x - 1, cy - a * sn((x - 1) * 85 + s * 60) / 256, x, cy - a * sn(x * 85 + s * 60) / 256);
        scritta_appare(p * 256 / 1000);
    } else {                                     /* blskk sulla linea piatta */
        for (int i = 0; i < NLET; i++) lettera(i, 0, 0, 0, 0, 0);
        for (int x = 10; x < W - 10; x++) pt(x, Y0 + 9 * SC + 5);
    }
}

static void fotogramma(int t)
{
    pulisci(FB);
    if (t < T_PRO) { prologo(t / 2); disegna(FB); return; }
    if (VAR && t < T_FINE) { if (VAR == 1) rito(t - T_PRO, T_FINE - T_PRO); else segnale(t - T_PRO, T_FINE - T_PRO); return; }
    if (t < T_TEMP) {                           /* la barra si rompe: caratteri sbagliati e righe che scivolano */
        int q = (t - T_PRO) * 1000 / 4;
        prologo(T_PRO / 2 - 1);
        for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (prob(q * 8 / 10)) GR[r][c] = (u8)TUTTI[rn(NT)];
        strappa(2 + 6 * q / 1000, 2 + 10 * q / 1000);
        disegna(FB);
        if (q > 500) inverti_riga(rn(ROWS));
        return;
    }
    if (t < T_LETT) {                           /* tempesta di caratteri */
        int s = t - T_TEMP;
        griglia_caso(650 - 100 * s / 12, TUTTI, NT);
        strappa(5, 10);
        if (prob(300)) { int d = rn(13) - 6; for (int r = 0; r < ROWS; r++) ruota_riga(r, d); }
        disegna(FB);
        for (int i = rn(3); i > 0; i--) inverti_riga(rn(ROWS));
        if (s == 3 || s == 8) for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) FB[y][x] ^= 1;
        return;
    }
    if (t < T_SOLI) {                           /* la sagoma di blskk riempita di caratteri, che salta e cambia lettera */
        int s = t - T_LETT, p = s * 1000 / 16;
        griglia_caso(320 * (1000 - p) / 1000 + 50, LEGGERI, NL); disegna(FB);
        griglia_caso(950, PESANTI, NP); pulisci(TEX); disegna(TEX);
        int amp = 6 * (1000 - p) / 1000 + 1;
        for (int i = 0; i < NLET; i++)
            lettera(i, prob(350 * (1000 - p) / 1000 + 100), rn(2 * amp + 1) - amp, p < 600 ? rn(5) - 2 : 0, 1, 0);
        if (prob(400)) {
            int y = rn(H - 4), d = rn(21) - 10; u8 tmp[W];
            for (int yy = y; yy < y + 3; yy++) {
                for (int x = 0; x < W; x++) tmp[(x + d + W) % W] = FB[yy][x];
                for (int x = 0; x < W; x++) FB[yy][x] = tmp[x];
            }
        }
        return;
    }
    if (t < T_FINE) {                           /* ogni lettera si solidifica punto per punto, il rumore se ne va */
        int s = t - T_SOLI, p = s * 255 / 20;
        griglia_caso(120, LEGGERI, NL);
        for (int r = 0; r < ROWS; r++) for (int c = 0; c < COLS; c++) if (SOG[r][c] < p) GR[r][c] = 0;
        disegna(FB);
        griglia_caso(950, PESANTI, NP); pulisci(TEX); disegna(TEX);
        for (int i = 0; i < NLET; i++) {
            int t0 = 1 + 3 * i, q = (s - t0) * 255 / 6;
            if (q <= 0) lettera(i, prob(200), rn(3) - 1, 0, 1, 0);
            else lettera(i, 0, 0, 0, 2, q > 255 ? 256 : q);
        }
        return;
    }
    int s = t - T_FINE;                         /* finale: blskk a pixel e il cursore */
    for (int i = 0; i < NLET; i++) lettera(i, 0, 0, 0, 0, 0);
    if (s == 14 || s == 15) {
        u8 tmp[W];
        for (int y = 30; y < 36; y++) {
            for (int x = 0; x < W; x++) tmp[(x + 5) % W] = FB[y][x];
            for (int x = 0; x < W; x++) FB[y][x] = tmp[x];
        }
    }
    if (s >= 24 || (s / 6) % 2 == 0)           /* 14/11: due lampeggi, poi il cursore resta acceso fino alla fine */
        for (int y = Y0; y < Y0 + 9 * SC; y++) for (int x = XCUR; x < XCUR + 2 && x < W; x++) FB[y][x] = 1;
}

#ifdef HOST
#include <stdio.h>
int main(int argc, char **argv)
{
    u32 seme = argc > 1 ? (u32)atoi(argv[1]) : 1;
    int n = argc > 2 ? atoi(argv[2]) : 175;
    prepara(seme);
    if (argc > 3) VAR = atoi(argv[3]);
    for (int t = 0; t < n; t++) {
        fotogramma(t);
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) putchar('0' + FB[y][x]);
        putchar('\n');
    }
    return 0;
}
#else
static int pronto = 0;
volatile u32 intro_dbg[4] __attribute__((section(".data"))) = {0x494E5452, 0, 0, 0};   /* 'INTR', chiamate, ultimo fotogramma, seme */

/* al posto della routine di serie (gancio in intro_hook.s): 1 = disegnato, 0 = lascia l'intro di serie (fotogramma 0) */
int intro_frame(int frame, void *panel)
{
    intro_dbg[1]++; intro_dbg[2] = (u32)frame;
    if (!panel || frame < 1) return 0;
    if (!pronto) {
        u32 seme = DTCN3 ^ (TICK << 16) ^ 0x9E3779B9u;
        intro_dbg[3] = seme;
        prepara(seme);
        VAR = CICLO % 3;                         /* 14/11: a turno GLITCH, RITO, FOURIER; em/pagine salva il prossimo dopo l'avvio */
        intro_dbg[3] = (u32)CICLO << 8 | (u32)VAR;
        pronto = 1;
    }
    fotogramma(frame - 1);
    CLEAR(panel, 0, 1);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            SETPIX(panel, x, H - 1 - y, FB[y][x] ? 1 : 0);   /* sullo strumento le righe del pannello sono capovolte */
    return 1;
}
#endif
