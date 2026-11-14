| em/lfolength 0.1: LFO TRIG MODE ONE e HALF fanno un ciclo / mezzo ciclo dal punto di partenza (SPH) e tengono il
| valore raggiunto, su tracce audio (valutatore A, 0x40137a06) e MIDI (valutatore B, 0x401376bc).
| Porting di `lfolength` di dn2_firmware_explore (scripts/build_lfo_length.py, AGPL-3.0), stesso codice: solo la sede
| cambia (cf.expansion invece del chunk 0x467b0000). Il byte libero +119 di ogni record LFO tiene il punto agganciato.
| Ganci (jmp, istruzioni spostate rieseguite qui):
|   0x40137c46 (10 B) A trig      movel %sp@(68),%d2 ; movel #1382400000,%d0
|   0x40137ccc (6 B)  A forma     movel #1667999861,%d0
|   0x4013783c (10 B) B trig      mvsw %a4@(44),%d0 ; movel #21600000,%d2
|   0x401378c4 (6 B)  B forma     moveq #6,%d0 ; cmpl %d3,%d0 ; beqs (RND)
| Registri: d4 = TRIG MODE, d3 = forma, a2 = stato LFO; d0/d1 liberi (A), d1 salvato (B forma).
    .text
    .globl lfa_trig
    .globl lfa_wave
    .globl lfb_trig
    .globl lfb_wave

lfa_trig:
    movel   %sp@(68),%d2            | rieseguita
    movel   #1382400000,%d0         | rieseguita
    moveq   #3,%d1
    cmpl    %d1,%d4
    blts    1f
    moveq   #4,%d1
    cmpl    %d1,%d4
    bgts    1f
    moveq   #5,%d1
    cmpl    %d1,%d3
    bgts    1f
    movel   %d2,%d1                 | ONE / HALF: aggancia il byte alto della cella ...
    asrl    #8,%d1
    moveb   %d1,%a2@(119)
    clrl    %d2                     | ... e parte da fase 0
    jmp     0x40137c5e
1:  clrb    %a2@(119)
    jmp     0x40137c50

lfa_wave:
    moveb   %a2@(119),%d0
    extbl   %d0
    beqs    9f
    swap    %d0
    clrw    %d0
    lsll    #8,%d0
    movel   #1382400000,%d1
    macl    %d1,%d0,%acc0
    movclrl %acc0,%d0
    tstl    %d0
    bmis    1f
    subl    %d1,%d0
1:  addl    %d0,%d2
    bpls    2f
    addl    %d1,%d2
2:  moveq   #3,%d0
    cmpl    %d0,%d4
    blts    3f
    moveq   #4,%d0
    cmpl    %d0,%d4
    bles    9f
3:  movel   %d2,%a2@(80)
    clrb    %a2@(119)
9:  movel   #1667999861,%d0         | rieseguita
    jmp     0x40137cd2

lfb_trig:
    mvsw    %a4@(44),%d0            | rieseguita
    movel   #21600000,%d2           | rieseguita
    moveq   #3,%d1
    cmpl    %d1,%d4
    blts    1f
    moveq   #4,%d1
    cmpl    %d1,%d4
    bgts    1f
    moveq   #5,%d1
    cmpl    %d1,%d3
    bgts    1f
    movel   %d0,%d1
    asrl    #8,%d1
    moveb   %d1,%a2@(119)
    clrl    %d2
    jmp     0x40137854
1:  clrb    %a2@(119)
    jmp     0x40137846

lfb_wave:
    moveq   #6,%d0                  | rieseguita: RND va per la sua strada
    cmpl    %d3,%d0
    beqs    8f
    movel   %d1,%sp@-
    moveb   %a2@(119),%d0
    extbl   %d0
    beqs    9f
    swap    %d0
    clrw    %d0
    lsll    #8,%d0
    movel   #21600000,%d1
    macl    %d1,%d0,%acc0
    movclrl %acc0,%d0
    tstl    %d0
    bmis    1f
    subl    %d1,%d0
1:  addl    %d0,%d2
    bpls    2f
    addl    %d1,%d2
2:  moveq   #3,%d0
    cmpl    %d0,%d4
    blts    3f
    moveq   #4,%d0
    cmpl    %d0,%d4
    bles    9f
3:  movel   %d2,%a2@(80)
    clrb    %a2@(119)
9:  movel   %sp@+,%d1
    moveq   #6,%d0
    jmp     0x401378ca
8:  jmp     0x401378fa
