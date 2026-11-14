/* em/pagine 2.0 (fase 3, 4/11/2026): le pagine SYN dei quattro motori della fase 3 (TERRENO, PULSAR, MANDALA, SPETTRO) sul
 * Digitone II 1.11. Tre pagine per motore, 24 manopole lette sempre tutte (ogni pagina puo' usare anche le altre). Il resto come
 * la 1.0: em_syn(view, canvas) prende il posto della griglia di serie nel caso "altre machine" della pagina SYN (gancio.s), le
 * machine di serie restano identiche. Il disegno e' disegno6.c (solo interi, uguale nella prova sul computer). */
#include "disegno6.c"
#include "motori6.h"

typedef unsigned int u32;
typedef unsigned char u8;

/* ---- firmware 1.12 (13/11: tradotti dalla 1.11 con la mappa di dnfw, tmp_claude/v60/mappa_pagine.py) ---- */
#define GRID       ((void (*)(void *, void *))0x40017428u)
#define SET_PIXEL  ((void (*)(void *, int, int, int))0x40113E70u)     /* (canvas, x, y dal basso, acceso) */
#define GET_VALUE  ((int (*)(void *, u32, u8 *))0x4006538Eu)          /* (view, id, &flag) */
#define PAGE_COUNT ((int (*)(void *))0x400173CAu)
#define PAGE_DOTS  ((void (*)(void *, void *, int, int, int))0x4006598Au)
#define FILL       ((void (*)(void *, int, int, int, int, int))0x40114C34u)   /* (canvas, x0, y0, x1, y1, modo): -1 inverte */
/* 3.10.3: la traccia attiva come la legge la pagina SYN di serie: getter(view+116) (0x40046bfe, uguale su 1.11 e 1.12). Il byte
 * 0x42431a6c usato prima e' un campo di una struct sullo heap: sullo strumento non e' a un indirizzo fisso (dnfw). */
#define TRACK_OF(view) ((int)((u32 (*)(void *))0x40046BFEu)(*(void **)((char *)(view) + 116)) & 15)
#define KIT_POINTER  (*(u8 *volatile *)0x800052A0u)
#define SOUND_BASE 52
#define SOUND_STRIDE 1163
#define SOUND_TYPE 0xDE
#define TICK_PER_S 120u                                         /* dnfw: ~120 al secondo */
#define TICK       (*(volatile u32 *)0x466768B0u)                     /* il millisecondo (interruzione 0x4011f5e4) */
/* v62g: tolto */
/* v62g: tolto */
/* v62g: tolto */
#define TABLE_BASE (*(volatile u32 *)0x400366C4u)                      /* la tabella dei parametri come la legge il testo del valore */

/* GCC chiama memset e memcpy da solo: i moduli C non hanno libc, quindi eccoli, con cicli che GCC non trasforma in se stessi */
void *memset(void *s, int c, __SIZE_TYPE__ n)
{
    volatile unsigned char *p = (volatile unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n)
{
    volatile unsigned char *p = (volatile unsigned char *)d;
    const volatile unsigned char *q = (const volatile unsigned char *)s;
    while (n--) *p++ = *q++;
    return d;
}

static void *method(void *obj, int offset) { return *(void **)(*(char **)obj + offset); }

static int machine_type(void *view)
{
    u8 *kit = KIT_POINTER;
    if (!kit) return -1;
    return (int)(signed char)kit[SOUND_BASE + SOUND_TYPE + SOUND_STRIDE * TRACK_OF(view)];
}

volatile u32 em_dbg[8] = {0x454D4442, 0, 0, 0, 0, 0, 0, 0};   /* [6] chiamate di em_poll, [7] ridisegni chiesti da em_poll */   /* 'EMDB', chiamate, tipo letto, motore (0..3, 4 = di serie), pagina, rifatte */

static int last_e = -1, last_page = -1, last_w[40];

/* 13/11 (3.10): l'encoder premuto (0..7 della pagina, -1 nessuno), fino a quando mostrarne ancora il valore dopo il rilascio,
 * l'ultimo premuto; la modulazione: quando e' cambiata l'ultima volta e quando abbiamo chiesto l'ultimo ridisegno */
static volatile int em_push = -1, em_push_last = -1, em_push_e = -1, em_push_page = -1;   /* 3.10.2: motore e pagina del premuto */
static volatile u32 em_push_until;
static int last_show = -1, last_wk[40], disp[40];
static u32 glide_t;                                   /* v62g: l'ultimo passo dello scivolamento */
/* 3.10.2: SETTINGS > PERSONALIZE > MOD GRAPHICS (menu.s): solo 1 = le grafiche seguono la modulazione */
#define MODVIS     (*(volatile u8 *)0x405CE8B0u)
#define MOD_MS     4u                                   /* 3.11.4: TICK (0x466758b0/0x466768b0) va a ~120 al secondo, NON e' un millisecondo (dnfw modulation-display.md): 4 = ~30 rifacimenti al secondo */
#define IS_DIRTY   ((unsigned char (*)(void *))0x4011D5D4u)
#define SET_DIRTY  ((void (*)(void *))0x4011D5DEu)
static volatile int em_want;
static volatile u32 em_want_at;
static void want_redraw(u32 at) { if (!em_want || (int)(at - em_want_at) < 0) { em_want_at = at; em_want = 1; } }

/* 3.10.1: al posto di isDirty nel giro dell'UI (0x4002e464, isDirty 0x4011d5d4 sulla 1.12): il ridisegno chiesto da em_syn quando e' l'ora */
/* v62g (3.13.0): MOD GRAPHICS non segue piu' l'LFO. Acceso, le grafiche scivolano verso il valore nuovo della manopola (manopola,
 * randomize, FUNC + giro): il valore disegnato rincorre quello impostato (1/4 della distanza ogni 4 tick, ~0,25 s). La richiesta
 * di ridisegno invalida la vista dell'ultimo disegno di una nostra pagina (solo nei 2 s dopo quel disegno). */
static volatile u32 em_wat;
static void *volatile em_view;
#define INVALIDATE ((void (*)(void *))0x4011CA9Au)
static void redraw_now(void *screen)
{
    if (em_view && (int)(TICK - em_wat) < (int)(2 * TICK_PER_S)) INVALIDATE(em_view);
    else SET_DIRTY(screen);
}
unsigned char em_poll(void *screen)
{
    em_dbg[6]++;
    if (em_want && (int)(TICK - em_want_at) >= 0) { em_want = 0; redraw_now(screen); em_dbg[7]++; }
    return IS_DIRTY(screen);
}

static void value_text(int id, int v, char *buf)
{
    u32 base = TABLE_BASE;
    void (*fmt)(int, char *) = *(void (**)(int, char *))(base + 60u * (u32)id + 0x34u);
    buf[0] = 0;
    if (fmt) fmt(v, buf);
    for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);   /* il nostro carattere ha solo maiuscole */
}

void em_syn(void *view, void *canvas)
{
    int t = machine_type(view), e;
    for (e = 0; e < E_N; e++) if (t == (int)(u32)MOT[e].type) break;
    em_dbg[1]++; em_dbg[2] = (u32)t; em_dbg[3] = (u32)e;
    if (e == E_N) { GRID(view, canvas); return; }

    int page = ((int (*)(void *))method(view, 132))(view);
    if (page < 0 || page >= NPAG[e]) page = 0;
    em_dbg[4] = (u32)page;

    u32 now = TICK;
    /* 3.10: valore al posto dell'etichetta. 3.10.2: solo sul motore e sulla pagina dove l'encoder e' stato premuto; su
     * un'altra pagina (o un altro motore) si dimentica, cosi' non ricompare tornando indietro */
    if (em_push_e != e || em_push_page != page) { em_push = -1; em_push_until = now; em_push_e = -1; em_push_page = -1; }
    int show = em_push >= 0 ? em_push : ((int)(em_push_until - now) > 0 ? em_push_last : -1);
    if (e == 0 && page == 4) show = -1;                 /* v62f: la pagina coi numeri (PHASE/SYNC) e' la 5 */                 /* 3.3: TERRAIN pag. 4 mostra sempre i numeri (come FM TONE 4/4) */
    int w[40] = {0}, wm[40] = {0}, locked[8], nk = NPAG[e] * 8, cambiata = e != last_e || page != last_page || show != last_show;
    int glide = MODVIS == 1, nuovo = e != last_e || page != last_page, moving = 0;
    int steps = (int)(now - glide_t) / 4;
    if (steps > 0) glide_t += (u32)steps * 4u;
    if (steps > 8) steps = 8;
    if (nuovo) glide_t = now;
    for (int i = 0; i < nk; i++) {
        u8 flag = 0;
        u32 pid = (u32)MOT[e].p[i];                     /* 3.3: una manopola senza parametro vale il centro */
        w[i] = pid ? GET_VALUE(view, pid, &flag) & 0xFFFF : 0x4000;
        if (i / 8 == page) locked[i % 8] = flag != 0;   /* p-lock sul trig tenuto (come la griglia di serie) */
        if (!glide || nuovo) disp[i] = w[i];
        else
            for (int k = 0; k < steps && disp[i] != w[i]; k++) {
                int d = w[i] - disp[i];
                disp[i] += (d < 128 && d > -128) ? d : d / 4;
            }
        if (disp[i] != w[i]) moving = 1;
        wm[i] = disp[i];
        if (w[i] != last_wk[i] || wm[i] != last_w[i]) cambiata = 1;
    }
    if (cambiata) {
        u32 t0 = TICK;
        em_render6(e, page, wm);
        (void)t0;
        if (show >= 0 && show < 8) {                    /* 3.10: il valore dell'encoder premuto al posto della sua etichetta */
            char buf[32];
            int col = show % 4, row = show / 4, ly = LY[row];
            value_text((int)(u32)MOT[e].p[page * 8 + show], w[page * 8 + show], buf);
            fill(CX[col] - 12, ly - 1, CX[col] + 12, ly + 5, 0);
            ctext(CX[col], ly, buf, 1);
        }
        last_e = e; last_page = page; last_show = show;
        for (int i = 0; i < 40; i++) { last_w[i] = wm[i]; last_wk[i] = w[i]; }
        em_dbg[5]++;
    }
    em_wat = now; em_view = view;
    /* v62g: mentre le grafiche scivolano, un ridisegno ogni 4 tick (~30 al secondo); poi il valore dell'encoder premuto che aspetta
     * di sparire; altrimenti il firmware ridisegna da solo una volta al secondo */
    if (moving) want_redraw(now + MOD_MS);
    else if (em_push < 0 && show >= 0) want_redraw(em_push_until);
    for (int y = 9; y < 64; y++)
        for (int x = 16; x < 128; x++)
            if (G[y][x]) SET_PIXEL(canvas, x, 63 - y, 1);

    /* un parametro con il p-lock sul trig tenuto: l'etichetta in negativo (riquadro di un punto attorno alla scritta) */
    for (int i = 0; i < 8; i++)
        if (locked[i] && i != show) {
            const char *lab = LAB[e][page][i];
            int w0 = tw(lab), x0 = CX[i % 4] - w0 / 2 - 1, ly = LY[i / 4];
            FILL(canvas, x0, 63 - (ly + 5), x0 + w0 + 1, 63 - (ly - 1), -1);
        }

    ((void (*)(void *, void *, int))method(view, 184))(view, canvas, 0);   /* l'indicatore LEV, come la griglia di serie */
    int n = PAGE_COUNT(view);
    if (n > 1) PAGE_DOTS(view, canvas, n, page, 7);
}

/* ---- 11/11: l'easter egg di TERRENO. Il gestore dei tasti della pagina SYN (0x40016480, nella vtable a 0x401d49a8) passa da
 * qui: su TERRENO pag. 1, FUNC + destra (premuto) gira l'immagine: spento -> UGO -> BLSK -> spento; tutto il resto va al gestore di
 * serie. Evento: codice a +12 (DESTRA = 15), bit a +16: 0 premuto, 1 FUNC tenuto, 3 ripetizione (dn2_firmware_explore). */
#define STOCK_KEY  ((int (*)(void *, void *))0x40016480u)
#define REDRAW     ((void (*)(void *))0x4011CA9Au)
#define EV_CODE(ev)  (*(volatile int *)((char *)(ev) + 12))
#define EV_FLAGS(ev) (*(volatile u32 *)((char *)(ev) + 16))

int em_syn_key(void *view, void *ev)
{
    int t = machine_type(view);
    /* 3.10: encoder A..H premuti (codici 41..48) su un nostro motore: il valore al posto dell'etichetta; poi come di serie */
    if (EV_CODE(ev) >= 41 && EV_CODE(ev) <= 48) {
        int ours = -1;
        for (int e = 0; e < E_N; e++) if (t == (int)(u32)MOT[e].type) ours = e;
        if (ours >= 0) {
            u32 f = EV_FLAGS(ev);
            if (f & 0x10) { em_push = -1; em_push_until = TICK + 3 * TICK_PER_S / 2; }        /* rilasciato: ancora 1,5 s */
            else if ((f & 1) && !(f & 8)) {
                int pg = ((int (*)(void *))method(view, 132))(view);
                if (pg < 0 || pg >= NPAG[ours]) pg = 0;
                em_push_e = ours; em_push_page = pg;      /* 3.10.2: il valore resta su questa pagina */
                em_push = EV_CODE(ev) - 41; em_push_last = em_push;
            }
            REDRAW(view);
        }
    }
    if (t == (int)(u32)MOT[E_TERRENO].type && EV_CODE(ev) == 15) {
        u32 f = EV_FLAGS(ev);
        int page = ((int (*)(void *))method(view, 132))(view);
        if (page == 0 && (f & 2)) {                         /* FUNC tenuto: la combinazione e' nostra (anche il rilascio) */
            if ((f & 1) && !(f & 8)) { em_egg = (em_egg + 1) % 3; last_e = -1; REDRAW(view); }
            return 1;
        }
    }
    return STOCK_KEY(view, ev);
}
