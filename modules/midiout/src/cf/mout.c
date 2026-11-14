/* em/midiout 0.2 (v20): MIDI OUT per traccia, una riga nel menu SETUP della traccia (FUNC+SETUP, la lista di CONTROL ALL CONFIG,
 * TRACK LAYERING, PLAY MODE, ... RENAME). Valori: OFF, TRIG (esce la nota del trig), oppure una nota fissa C-1 .. G9 (la stessa a
 * ogni colpo: per il VJing ogni traccia la sua nota). Su una traccia MIDI la riga dice N/A (le tracce MIDI mandano gia' il MIDI).
 * La nota esce sul canale che la traccia ha in SETTINGS > MIDI CONFIG > CHANNELS (TRACK n CHANNEL); OFF la' = niente.
 *
 * Come (firmware 1.11, indirizzi letti dal MAIN OS e ricontrollati, vedi LEGGIMI):
 *  - il menu: il costruttore 0x400b8b00 aggiunge le righe con MenuItem(0x401173b8: item, etichetta, azione (YES), disegno del
 *    valore, cambio del valore, tipo, 8) e list->add (vtable +8); (prova nell'emulatore del 3/11: l'ordine giusto e' questo, non
 *    etichetta-disegno-cambio); una riga con valore e' di tipo 2 come PLAY MODE (disegno 0x400b774c, cambio 0x400b782c). Il gancio
 *    alla fine del costruttore (gancio_menu in midiout.s) chiama mout_menu(list) che aggiunge la riga MIDI OUT;
 *  - le note: i ganci dell'ISR di layermidi (note_hook / release_hook) mandano un record MIDI anche per le tracce audio con MIDI OUT
 *    acceso (traccia = la traccia audio, nota = quella del trig o quella fissa);
 *  - il canale: il compito MIDI chiede il canale a 0x4012a3b2(traccia), che per una traccia audio risponde -1 (niente nota).
 *    mout_chan lo sostituisce: per le tracce MIDI identico, per una traccia audio con MIDI OUT il canale di TRACK n CHANNEL.
 * Per ora il valore resta in memoria fino allo spegnimento (non e' salvato nel progetto). */
typedef unsigned int u32;

/* ---- firmware 1.11 ---- */
#define OP_NEW    ((void *(*)(u32))0x40120544u)
#define ITEM_NEW  ((void (*)(void *, void *, void *, void *, void *, int, int))0x401173b8u)
#define FN_FREE   ((void (*)(void *))0x40188366u)                 /* ~std::function */
#define MGR1      0x400b5dfeu                                    /* il gestore di una closure vuota di 1 byte (etichetta di CONTROL ALL CONFIG) */
#define TEXT      ((void (*)(void *, const void *, int, int, int, int, int, const char *))0x40115948u)
#define FONT      ((const void *)0x40294710u)                    /* il carattere dei valori del menu (PLAY MODE) */
#define APP       ((char *(*)(void))0x4018ac5au)
#define ACTIVE    ((int (*)(void *))0x40046bfeu)                  /* (app + 48) -> traccia attiva */
#define IS_MIDI   ((char (*)(void *, int))0x40031274u)            /* (app + 244, traccia) */
#define TRK_CH    ((int (*)(void *, int))0x40047e78u)             /* (app + 48, traccia) -> canale 0..15, -1 = OFF */
#define INVALIDA  ((void (*)(void *))0x4011ca9au)                 /* vista: da ridisegnare (+20 = 1) */
#define TRK_SET   ((void (*)(void *, int, int))0x40047ec6u)        /* (app + 48, traccia, canale): il setter di TRACK n CHANNEL */
#define MIDI_MASK (*(volatile short *)0x8000537cu)               /* le tracce MIDI (copia di kit + 0x5cda) */
#define MFLAGS(t) (*(volatile u32 *)(0x446173c4u + 4u * (908u + (u32)(t))))
#define MCHAN(t)  (*(volatile signed char *)(0x446173f4u + 202u * (u32)(t) + 134u))

/* 0 = OFF, 1 = TRIG, 2 + n = la nota n. In .data perche' parta da OFF qualunque cosa ci sia nella memoria */
unsigned char mout_tab[16] __attribute__((section(".data"))) = {0};
/* per la prova nell'emulatore: 'MOUT', disegni, richieste di canale per una traccia audio con MIDI OUT, ultimo canale dato */
volatile u32 mout_dbg[4] __attribute__((section(".data"))) = {0x4D4F5554, 0, 0, 0};
/* la vista del menu SETUP (il costruttore la riceve come lista): dopo un cambio va ridisegnata, come fanno i setter di serie */
static void *mout_view __attribute__((section(".data"))) = 0;

typedef struct { void *p; u32 z; u32 mgr; void *inv; } fn16;      /* std::function di GCC 4: dati (8), gestore, invocatore */

extern void mout_label(void);                                      /* in midiout.s: torna lo std::string "MIDI OUT" (risultato in a0) */

static int traccia(int *midi)
{
    char *app = APP();
    int t = ACTIVE(app + 48);
    *midi = t > 15 || IS_MIDI(app + 244, t) != 0;
    return t;
}

static const char NOTE[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

void mout_draw(void *f, int u, void *canvas, int x, int y)
{
    (void)f; (void)u;
    mout_dbg[1]++;
    int midi, t = traccia(&midi);
    char s[8]; const char *txt = s;
    if (midi) txt = "N/A";
    else if (mout_tab[t] == 0) txt = "OFF";
    else if (mout_tab[t] == 1) txt = "TRIG";
    else {
        int n = mout_tab[t] - 2, o = n / 12 - 1, k = 0;          /* C4 = nota 60 */
        const char *nm = NOTE[n % 12];
        while (*nm) s[k++] = *nm++;
        if (o < 0) { s[k++] = '-'; o = -o; }
        s[k++] = (char)('0' + o); s[k] = 0;
    }
    TEXT(canvas, FONT, x + 19, y + 2, 2, 0, 0, txt);
}

void mout_change(void *f, int u, int delta)
{
    (void)f; (void)u;                                             /* u = la MenuItem (0x401171be) */
    int midi, t = traccia(&midi);
    if (midi) return;
    int v = mout_tab[t] + delta;
    mout_tab[t] = (unsigned char)(v < 0 ? 0 : v > 129 ? 129 : v);
    if (mout_view) INVALIDA(mout_view);
    /* la vista si ridisegna quando il modello avvisa (i setter di serie chiamano obj->vtable[4]): si riscrive lo stesso canale
     * della traccia con il setter di TRACK n CHANNEL (0x40047ec6), che avvisa sempre, senza cambiare niente */
    char *app = APP();
    TRK_SET(app + 48, t, TRK_CH(app + 48, t));
}

/* la riga, alla fine del menu (chiamata dal gancio alla fine del costruttore, a4 = la lista) */
void mout_menu(void *list)
{
    mout_view = list;
    fn16 fl, fd, fc, fe;
    fl.p = OP_NEW(1); fl.z = 0; fl.mgr = MGR1; fl.inv = (void *)mout_label;
    fd.p = OP_NEW(1); fd.z = 0; fd.mgr = MGR1; fd.inv = (void *)mout_draw;
    fc.p = OP_NEW(1); fc.z = 0; fc.mgr = MGR1; fc.inv = (void *)mout_change;
    fe.p = 0; fe.z = 0; fe.mgr = 0; fe.inv = 0;
    void *item = OP_NEW(0x54);
    ITEM_NEW(item, &fl, &fe, &fd, &fc, 2, 8);                     /* etichetta, nessuna azione, disegno, cambio */
    ((void (*)(void *, void *))(*(void ***)list)[2])(list, item);
    FN_FREE(&fl); FN_FREE(&fd); FN_FREE(&fc); FN_FREE(&fe);
}

/* al posto di 0x4012a3b2 (gancio "jump" all'ingresso: stessa pila, stesso ritorno) */
int mout_chan(int t)
{
    if (MFLAGS(t) & 1) return MCHAN(t);                            /* come di serie: traccia MIDI con il canale acceso */
    if ((u32)t > 15 || !mout_tab[t] || (MIDI_MASK >> t & 1)) return -1;
    char *app = APP();
    int ch = TRK_CH(app + 48, t);
    mout_dbg[2]++; mout_dbg[3] = (u32)ch;
    return ch;
}
