| em/midiout 0.2: le note delle tracce audio escono in MIDI (ColdFire, sintassi gas, mcpu=54418).
|
| Due strade:
|  1. TRACK WILL TRIGGER da una traccia audio verso una traccia MIDI (0.1, porting di `layermidi` di dn2_firmware_explore,
|     scripts/build_layer_midi.py, AGPL-3.0, provato sullo strumento il 2-3/10/2026): le copie diventano note MIDI sul canale
|     della traccia MIDI;
|  2. (0.2) MIDI OUT nel menu SETUP della traccia (mout.c): una traccia audio con MIDI OUT acceso manda le sue note (TRIG) o una
|     nota fissa sul canale di TRACK n CHANNEL.
| Ganci nell'ISR dei fotogrammi (come la 0.1):
|   0x40026980  testa del corpo per nota (mvz.w 0x8000537e,%d1, 6 byte): jsr note_hook, che la riesegue alla fine;
|   0x40026D32  testa del rilascio per nota (mvs.b 2(%a2),%d4 ; move.l %d4,-(%sp), 6 byte): jmp release_hook, che le
|               riesegue e torna a 0x40026D38.
| Il record e' -36(%fp), la voce %a2. Tutti i registri salvati, tranne %a5 (coda del lotto MIDI, voluto) e %d1 (lo
| imposta l'istruzione spostata). Il pool dei record MIDI (0x4012a6e8) non controlla se e' vuoto: si prende un record solo
| se ne restano almeno due liberi.
| E (0.2) la fine del costruttore del menu SETUP (0x400b97a2, movem.l 8(%sp),%d2-%d7/%a2-%a6): jsr gancio_menu.
    .text
    .globl note_hook
    .globl release_hook
    .globl gancio_menu
    .globl mout_label
note_hook:
    lea     -32(%sp),%sp
    movem.l %d0-%d3/%a0-%a1/%a3-%a4,(%sp)
    bsr.s   gate
    bne.s   1f
    moveq   #1,%d3                      | note on
    bsr.w   emit
1:  movem.l (%sp),%d0-%d3/%a0-%a1/%a3-%a4
    lea     32(%sp),%sp
    mvz.w   0x8000537e,%d1              | l'istruzione spostata, per ultima
    rts
release_hook:
    lea     -32(%sp),%sp
    movem.l %d0-%d3/%a0-%a1/%a3-%a4,(%sp)
    bsr.s   gate
    bne.s   1f
    moveq   #0,%d3                      | note off
    bsr.w   emit
1:  movem.l (%sp),%d0-%d3/%a0-%a1/%a3-%a4
    lea     32(%sp),%sp
    mvs.b   2(%a2),%d4                  | le due istruzioni spostate
    move.l  %d4,-(%sp)
    jmp     0x40026d38
| -> Z quando il record -36(%fp) va mandato in MIDI e il pool MIDI ha un record di avanzo; allora %a4 il record, %d2 la
| sua traccia. Traccia MIDI: solo le copie di TRACK WILL TRIGGER (0.1). Traccia audio: se ha MIDI OUT acceso (0.2).
gate:
    move.l  -36(%fp),%a4
    move.l  16(%a4),%d2
    moveq   #15,%d1
    cmp.l   %d2,%d1
    bcs.s   8f
    mvs.w   0x8000537c,%d1
    btst    %d2,%d1
    bne.s   5f                          | traccia MIDI
    lea     mout_tab,%a1                | traccia audio: MIDI OUT?
    tst.b   0(%a1,%d2.l)
    beq.s   8f                          | spento: come di serie
    bra.s   6f
5:  move.l  56(%a4),%d1
    btst    #17,%d1
    beq.s   8f                          | non e' una copia: come di serie
    move.l  #1163,%d0                   | (0.2.1) arp acceso su questa traccia MIDI: la copia la suona
    muls.l  %d2,%d0                     | em/midiarp come nota dell'arp (in dnfw layermidi e midiarp
    move.l  0x800052a0,%a1              | mandavano due volte la stessa nota)
    add.l   %d0,%a1
    tst.b   403(%a1)                    | kit + 52 + 1163*traccia + 351: MODE dell'arp
    bne.s   8f
6:  move.l  0x4460f4b8,%d0              | il pool non controlla il vuoto: tenerne uno per il compito MIDI
    beq.s   8f
    move.l  %d0,%a1
    tst.l   92(%a1)
    beq.s   8f
    moveq   #0,%d0
    rts
8:  moveq   #1,%d0
    rts
| un record MIDI per la voce %a2 del record %a4 sulla traccia %d2, tipo %d3 (1 on, 0 off), in coda al lotto dell'ISR
emit:
    jsr     0x4012a6e8
    move.l  %d0,%a3
    move.l  %d2,8(%a3)                  | traccia
    move.l  %d3,12(%a3)                 | tipo
    moveq   #0,%d0
    tst.l   %d3
    beq.s   1f
    move.l  #0x80,%d0
1:  move.l  %d0,16(%a3)
    clr.l   20(%a3)
    moveq   #-1,%d0
    move.l  %d0,24(%a3)                 | la nota e' quella in linea a +36
    clr.l   32(%a3)
    move.l  0x80005398,%d0
    tst.l   %d3
    beq.s   2f
    move.l  -44(%fp),%a0                | note-on: il tempo che l'ISR da' al trigger della voce
    tst.l   (%a0)
    bne.s   2f
    move.l  4(%a0),%d0
2:  move.l  %d0,48(%a3)
    moveq   #2,%d0
    move.l  %d0,52(%a3)
    clr.b   36(%a3)
    clr.b   37(%a3)
    move.b  2(%a2),%d0                  | nota del trig
    mvs.w   0x8000537c,%d1
    btst    %d2,%d1
    bne.s   9f                          | traccia MIDI (0.1): la nota del trig
    lea     mout_tab,%a0
    moveq   #0,%d1
    move.b  0(%a0,%d2.l),%d1
    subq.l  #2,%d1
    bmi.s   9f                          | TRIG: la nota del trig
    move.b  %d1,%d0                     | nota fissa
9:  move.b  %d0,38(%a3)                 | nota
    clr.b   41(%a3)
    move.l  44(%a4),%d0                 | il suono da cui vengono i valori di partenza
    bne.s   2f
    move.l  #1163,%d0
    muls.l  %d2,%d0
    add.l   0x800052a0,%d0
    add.l   #52,%d0
2:  move.l  %d0,44(%a3)
    move.l  %d0,%a0
    move.b  3(%a2),%d1                  | velocity; negativa = quella del suono
    bpl.s   3f
    move.b  0x480(%a0),%d1
    bpl.s   3f
    moveq   #100,%d1
3:  move.b  %d1,39(%a3)
    moveq   #0,%d3
    move.b  4(%a2),%d3                  | lunghezza; negativa = quella del suono
    tst.b   %d3
    bpl.s   4f
    move.b  0x481(%a0),%d3
4:  moveq   #126,%d1
    cmp.l   %d3,%d1
    bcc.s   5f
    move.l  %d1,%d3                     | INF non si spegnerebbe mai
5:  move.b  %d3,40(%a3)
    tst.l   -180(%fp)
    bne.s   6f
    move.l  %a3,-180(%fp)
    bra.s   7f
6:  move.l  %a3,92(%a5)
7:  move.l  %a3,%a5
    rts
| (0.2) fine del costruttore del menu SETUP: %a4 = la lista. Si aggiunge la riga MIDI OUT, poi l'istruzione spostata con la pila
| spostata di 4 (il nostro ritorno): i registri si ripristinano subito dopo, quindi qui si possono usare tutti.
gancio_menu:
    move.l  %a4,-(%sp)
    jsr     mout_menu
    addq.l  #4,%sp
    movem.l 12(%sp),%d2-%d7/%a2-%a6
    rts
| (0.2) l'etichetta della riga: std::string "MIDI OUT" nel risultato in %a0, come le etichette di serie (0x400b596c)
mout_label:
    link.w  %fp,#-4
    move.l  %d2,-(%sp)
    pea     -1(%fp)
    pea     mout_nome
    move.l  %a0,%d2
    move.l  %a0,-(%sp)
    jsr     0x401ceab8
    lea     12(%sp),%sp
    move.l  %d2,%d0
    move.l  -8(%fp),%d2
    unlk    %fp
    rts
    .section .rodata
mout_nome:
    .asciz  "MIDI OUT"
