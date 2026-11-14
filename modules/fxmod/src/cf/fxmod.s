| em/fxmod 0.1: un LFO puo' modulare 24 parametri di Chorus, Delay e Reverb (codici DEST 101..124).
| Porting di `fxmod` di dn2_firmware_explore (scripts/build_fxbrowser.py, fxmod_code.json, AGPL-3.0; provato sullo
| strumento il 2026-09-23 come fxbrowser2). Stesse tre routine della cave 0x4028ef42, con due differenze:
|  - fx_code chiama le due funzioni di serie (0x400dbe28 voce -> +4, 0x400dbe4c voce -> +0) invece di rifarle con la
|    tabella di serie e il limite 321: cosi' leggono la copia della tabella di core/params e il suo limite (righe di
|    TERRAIN comprese);
|  - fx_slot e' il gancio diretto all'ingresso di 0x400dc18e (com'era in fxbrowser2), non il bus di dnfw.
    .text
    .globl fx_code
    .globl fx_eval
    .globl fx_slot

| voce della tabella -> codice DEST: quello di serie, +76 per un record dei gruppi 16 Chorus, 17 Reverb, 18 Delay.
| Sostituisce `jsr 0x400dbe28` in sette punti. C: argomento sulla pila, d0-d1/a0-a1 liberi, d2 salvato.
fx_code:
    movel   %d2,%sp@-
    movel   %sp@(8),%sp@-           | la voce
    jsr     0x400dbe4c              | -> il gruppo (+0 del record)
    movel   %d0,%d2
    jsr     0x400dbe28              | -> lo slot (+4), stessa voce sulla pila
    addql   #4,%sp
    subil   #16,%d2
    cmpil   #2,%d2
    bhis    1f
    addil   #76,%d0                 | Chorus / Reverb / Delay -> 101..124
1:  movel   %sp@+,%d2
    rts

| Valutatore A, 0x40137d7e (jmp, 8 B spostati: mvs.w 82(%a4),%d1 ; lea 0(%a0,%d7.l*2),%fp). %d7 = DEST: oltre 100 il
| blocco dello specchio e' quello degli FX (blocco 16: 0x800068e4 + 34 + 202*16 - 2*76 = 0x8000750e).
fx_eval:
    moveq   #100,%d1
    cmpl    %d7,%d1
    bccs    2f
    lea     0x8000750e,%a0
2:  mvsw    %a4@(82),%d1            | le due istruzioni spostate
    lea     %a0@(0,%d7:l:2),%fp
    jmp     0x40137d86

| 0x400dc18e, param_set_slot_to_id(slot, type, filter), all'ingresso (jmp, 8 B spostati: move.l %d2,-(%sp) ;
| moveq #100,%d1 ; movea.l 8(%sp),%a0). Slot 101..124: la voce dalla tabella degli slot del set FX
| (0x42c659a8, costruita all'avvio); il resto come di serie (riprende a 0x400dc196, dove c'e' il gancio di core/machines).
fx_slot:
    movel   %sp@(4),%d0
    moveq   #100,%d1
    cmpl    %d0,%d1
    bccs    1f
    moveq   #124,%d1
    cmpl    %d0,%d1
    bcss    1f
    lea     0x42c65878,%a0          | 0x42c659a8 - 4*76 (1.12, come dnfw fxmod_code_1.12)
    movel   %a0@(0,%d0:l:4),%d0
    rts
1:  movel   %d2,%sp@-               | le istruzioni spostate
    moveq   #100,%d1
    moveal  %sp@(8),%a0
    jmp     0x400dc196
