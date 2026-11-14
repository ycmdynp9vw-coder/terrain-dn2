| em/pagine 3.10.2: SETTINGS > PERSONALIZE > MOD GRAPHICS (spento di base). Acceso, le grafiche delle pagine dei
| motori seguono la modulazione (sentito - impostato); spento, mostrano il valore impostato e non si ridisegnano di piu'.
| Generato da tmp_claude/v60/gen_menu_pagine.py da dnfw scripts/build_personalize_toggle.py (AGPL-3.0).
| Interruttore: byte 0x405ce8b0 (1.12) (letto anche da pagine6.c). Gancio 0x400987a8 sulla 1.12 (0x40098644 sulla 1.11, 6 B): jmp pg_menu_hook.
    .text
    .globl pg_menu_hook
| -> %d0 1 when the toggle is on, else 0.
pg_get:
    mvz.b   0x405ce8b0,%d0
    subq.l  #1,%d0
    seq     %d0
    andi.l  #1,%d0
    rts

| (on): the toggle, through the stock setting writer.
pg_set:
    link    %a6,#-4
    moveq   #0,%d0
    tst.b   11(%a6)
    sne     %d0
    andi.l  #1,%d0
    move.b  %d0,-4(%a6)
    pea     1
    pea     -4(%a6)
    pea     0x405ce8b0
    jsr     0x400bb65c
    lea     12(%sp),%sp
    unlk    %a6
    rts

| pg_name(&string, functor): the item's pg_name (0x40096464).
pg_name:
    link    %a6,#-4
    move.l  %d2,-(%sp)
    move.l  %a0,%d2
    pea     -1(%a6)
    pea     pg_item_name
    move.l  %a0,-(%sp)
    jsr     0x401ceab8
    lea     12(%sp),%sp
    move.l  %d2,%d0
    move.l  -8(%a6),%d2
    unlk    %a6
    rts

| pg_press(functor): flip, redraw the view (0x400966d8).
pg_press:
    move.l  %a2,-(%sp)
    move.l  8(%sp),%a0
    move.l  (%a0),%a2
    bsr.w   pg_get
    eori.l  #1,%d0
    move.l  %d0,-(%sp)
    bsr.w   pg_set
    addq.l  #4,%sp
    move.l  (%a2),%a2
    lea     56(%a2),%a2
    move.l  %a2,8(%sp)
    move.l  (%sp)+,%a2
    jmp     0x4011ca9a

| pg_value(functor, out, x, y): ON or OFF (0x40097802).
pg_value:
    bsr.w   pg_get
    move.l  12(%sp),4(%sp)
    move.l  %d0,%d1
    lsl.l   #5,%d0
    lsl.l   #2,%d1
    sub.l   %d1,%d0
    add.l   0x44645d84,%d0
    move.l  %d0,8(%sp)
    move.l  16(%sp),%d1
    addi.l  #11,%d1
    move.l  %d1,12(%sp)
    move.l  20(%sp),16(%sp)
    clr.l   20(%sp)
    jmp     0x40115adc

| pg_adjust(functor, -, delta): on for a positive turn, off for a negative one
| (0x4009670a), and redraw.
pg_adjust:
    move.l  %a2,-(%sp)
    move.l  %d2,-(%sp)
    move.l  12(%sp),%a0
    move.l  20(%sp),%d2
    move.l  (%a0),%a2
    bsr.w   pg_get
    add.l   %d2,%d0
    bpl.s   1f
    moveq   #0,%d0
1:  moveq   #1,%d1
    cmp.l   %d1,%d0
    ble.s   2f
    move.l  %d1,%d0
2:  move.l  %d0,-(%sp)
    bsr.w   pg_set
    addq.l  #4,%sp
    move.l  (%a2),%a2
    lea     56(%a2),%a2
    move.l  %a2,12(%sp)
    move.l  (%sp)+,%d2
    move.l  (%sp)+,%a2
    jmp     0x4011ca9a


| v62d: SETTINGS > PERSONALIZE > NEGATIVE SCREEN (byte 0x405ce8b1, spento di base): le stesse routine con ng_
| -> %d0 1 when the toggle is on, else 0.
ng_get:
    mvz.b   0x405ce8b1,%d0
    subq.l  #1,%d0
    seq     %d0
    andi.l  #1,%d0
    rts

| (on): the toggle, through the stock setting writer.
ng_set:
    link    %a6,#-4
    moveq   #0,%d0
    tst.b   11(%a6)
    sne     %d0
    andi.l  #1,%d0
    move.b  %d0,-4(%a6)
    pea     1
    pea     -4(%a6)
    pea     0x405ce8b1
    jsr     0x400bb65c
    lea     12(%sp),%sp
    unlk    %a6
    rts

| ng_name(&string, functor): the item's ng_name (0x40096464).
ng_name:
    link    %a6,#-4
    move.l  %d2,-(%sp)
    move.l  %a0,%d2
    pea     -1(%a6)
    pea     ng_item_name
    move.l  %a0,-(%sp)
    jsr     0x401ceab8
    lea     12(%sp),%sp
    move.l  %d2,%d0
    move.l  -8(%a6),%d2
    unlk    %a6
    rts

| ng_press(functor): flip, redraw the view (0x400966d8).
ng_press:
    move.l  %a2,-(%sp)
    move.l  8(%sp),%a0
    move.l  (%a0),%a2
    bsr.w   ng_get
    eori.l  #1,%d0
    move.l  %d0,-(%sp)
    bsr.w   ng_set
    addq.l  #4,%sp
    move.l  (%a2),%a2
    lea     56(%a2),%a2
    move.l  %a2,8(%sp)
    move.l  (%sp)+,%a2
    jmp     0x4011ca9a

| ng_value(functor, out, x, y): ON or OFF (0x40097802).
ng_value:
    bsr.w   ng_get
    move.l  12(%sp),4(%sp)
    move.l  %d0,%d1
    lsl.l   #5,%d0
    lsl.l   #2,%d1
    sub.l   %d1,%d0
    add.l   0x44645d84,%d0
    move.l  %d0,8(%sp)
    move.l  16(%sp),%d1
    addi.l  #11,%d1
    move.l  %d1,12(%sp)
    move.l  20(%sp),16(%sp)
    clr.l   20(%sp)
    jmp     0x40115adc

| ng_adjust(functor, -, delta): on for a positive turn, off for a negative one
| (0x4009670a), and redraw.
ng_adjust:
    move.l  %a2,-(%sp)
    move.l  %d2,-(%sp)
    move.l  12(%sp),%a0
    move.l  20(%sp),%d2
    move.l  (%a0),%a2
    bsr.w   ng_get
    add.l   %d2,%d0
    bpl.s   1f
    moveq   #0,%d0
1:  moveq   #1,%d1
    cmp.l   %d1,%d0
    ble.s   2f
    move.l  %d1,%d0
2:  move.l  %d0,-(%sp)
    bsr.w   ng_set
    addq.l  #4,%sp
    move.l  (%a2),%a2
    lea     56(%a2),%a2
    move.l  %a2,12(%sp)
    move.l  (%sp)+,%d2
    move.l  (%sp)+,%a2
    jmp     0x4011ca9a


| la fine del costruttore di PersonalizeMenuView, raggiunta con jmp: aggiunge la voce, poi lea 128(%sp),%sp ; rts
pg_menu_hook:
    lea     -16(%sp),%sp
    movem.l %d0/%d6/%a3-%a4,(%sp)
    move.l  148(%sp),%a4                | il view: 132 + 16
    lea     -64(%sp),%sp
    move.l  %sp,%a3
    pea     1
    jsr     0x40120544
    move.l  %d0,(%a3)
    clr.l   4(%a3)
    lea     0x40097046,%a0
    move.l  %a0,8(%a3)
    lea     pg_name,%a0
    move.l  %a0,12(%a3)
    pea     4
    jsr     0x40120544
    move.l  %d0,%a0
    move.l  %a4,(%a0)
    move.l  %d0,16(%a3)
    clr.l   20(%a3)
    lea     0x40097098,%a0
    move.l  %a0,24(%a3)
    lea     pg_press,%a0
    move.l  %a0,28(%a3)
    pea     1
    jsr     0x40120544
    move.l  %d0,32(%a3)
    clr.l   36(%a3)
    lea     0x400970f4,%a0
    move.l  %a0,40(%a3)
    lea     pg_value,%a0
    move.l  %a0,44(%a3)
    pea     4
    jsr     0x40120544
    move.l  %d0,%a0
    move.l  %a4,(%a0)
    move.l  %d0,48(%a3)
    clr.l   52(%a3)
    lea     0x40097146,%a0
    move.l  %a0,56(%a3)
    lea     pg_adjust,%a0
    move.l  %a0,60(%a3)
    pea     0x54
    jsr     0x40120544
    lea     20(%sp),%sp
    move.l  %d0,%d6
    pea     8
    pea     -1
    pea     48(%a3)
    pea     32(%a3)
    pea     16(%a3)
    move.l  %a3,-(%sp)
    move.l  %d6,-(%sp)
    jsr     0x401173b8
    move.l  %d6,-(%sp)
    move.l  %a4,-(%sp)
    jsr     0x40116bee
    pea     48(%a3)
    jsr     0x40188366
    pea     32(%a3)
    jsr     0x40188366
    pea     16(%a3)
    jsr     0x40188366
    move.l  %a3,-(%sp)
    jsr     0x40188366
    lea     52(%sp),%sp
    lea     64(%sp),%sp
    lea     -64(%sp),%sp
    move.l  %sp,%a3
    pea     1
    jsr     0x40120544
    move.l  %d0,(%a3)
    clr.l   4(%a3)
    lea     0x40097046,%a0
    move.l  %a0,8(%a3)
    lea     ng_name,%a0
    move.l  %a0,12(%a3)
    pea     4
    jsr     0x40120544
    move.l  %d0,%a0
    move.l  %a4,(%a0)
    move.l  %d0,16(%a3)
    clr.l   20(%a3)
    lea     0x40097098,%a0
    move.l  %a0,24(%a3)
    lea     ng_press,%a0
    move.l  %a0,28(%a3)
    pea     1
    jsr     0x40120544
    move.l  %d0,32(%a3)
    clr.l   36(%a3)
    lea     0x400970f4,%a0
    move.l  %a0,40(%a3)
    lea     ng_value,%a0
    move.l  %a0,44(%a3)
    pea     4
    jsr     0x40120544
    move.l  %d0,%a0
    move.l  %a4,(%a0)
    move.l  %d0,48(%a3)
    clr.l   52(%a3)
    lea     0x40097146,%a0
    move.l  %a0,56(%a3)
    lea     ng_adjust,%a0
    move.l  %a0,60(%a3)
    pea     0x54
    jsr     0x40120544
    lea     20(%sp),%sp
    move.l  %d0,%d6
    pea     8
    pea     -1
    pea     48(%a3)
    pea     32(%a3)
    pea     16(%a3)
    move.l  %a3,-(%sp)
    move.l  %d6,-(%sp)
    jsr     0x401173b8
    move.l  %d6,-(%sp)
    move.l  %a4,-(%sp)
    jsr     0x40116bee
    pea     48(%a3)
    jsr     0x40188366
    pea     32(%a3)
    jsr     0x40188366
    pea     16(%a3)
    jsr     0x40188366
    move.l  %a3,-(%sp)
    jsr     0x40188366
    lea     52(%sp),%sp
    lea     64(%sp),%sp
    movem.l (%sp),%d0/%d6/%a3-%a4
    lea     16(%sp),%sp
    lea     128(%sp),%sp                | l'istruzione spostata
    rts

    .section .rodata
pg_item_name:
    .asciz  "MOD GRAPHICS"
ng_item_name:
    .asciz  "NEGATIVE SCREEN"

| v62d: lo schermo in negativo. panel_diff (0x40131fd2 sulla 1.12) confronta il buffer davanti (puntatore 0x402a10c8) con quello
| di dietro e manda al pannello le righe cambiate. Con NEGATIVE SCREEN acceso il buffer davanti viene invertito solo per la
| durata di panel_diff: al pannello (e nel buffer di dietro) va il negativo, il firmware continua a disegnare in positivo.
| Chiamato al posto di panel_diff dal giro dell'UI (lea 0x40131fd2,%a4 a 0x4002e470) e dai due jsr a 0x40131d2c e 0x40131ec2.
    .text
    .globl neg_panel
neg_panel:
    tst.b   ic_fatto                | 14/11: la prima volta dopo l'avvio, il turno della prossima intro (em/intro legge 0x405ce8b2)
    bne.s   0f
    moveq   #1,%d0
    move.b  %d0,ic_fatto
    bsr.w   ic_avanza
0:  tst.b   0x405ce8b1
    bne.s   1f
    jmp     0x40131fd2
1:  move.l  (%sp)+,neg_ret
    bsr.s   neg_inv
    jsr     0x40131fd2
    move.l  %d0,neg_d0
    bsr.s   neg_inv
    move.l  neg_d0,%d0
    move.l  neg_ret,-(%sp)
    rts
neg_inv:
    lea     -12(%sp),%sp
    movem.l %d1-%d2/%a0,(%sp)
    movea.l 0x402a10c8,%a0
    move.l  #256,%d1
2:  move.l  (%a0),%d2
    not.l   %d2
    move.l  %d2,(%a0)+
    subq.l  #1,%d1
    bne.s   2b
    movem.l (%sp),%d1-%d2/%a0
    lea     12(%sp),%sp
    rts
    .bss
    .align  4
neg_ret:
    .skip   4
neg_d0:
    .skip   4

| 14/11: il ciclo delle intro (GLITCH, RITO, FOURIER). Il byte 0x405ce8b2 (coda del blocco impostazioni, dopo NEGATIVE SCREEN)
| dice quale intro mostrare alla prossima accensione; qui, una volta per avvio, diventa (valore % 3 + 1) % 3, scritto con il
| writer delle impostazioni di serie come le voci di PERSONALIZE. Conserva d0-d1/a0-a1 (siamo dentro il giro dell'UI).
    .text
ic_avanza:
    link    %a6,#-4
    lea     -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    moveq   #0,%d0
    move.b  0x405ce8b2,%d0
1:  cmpi.l  #3,%d0
    bcs.s   2f
    subq.l  #3,%d0
    bra.s   1b
2:  addq.l  #1,%d0
    cmpi.l  #3,%d0
    bne.s   3f
    moveq   #0,%d0
3:  move.b  %d0,-4(%a6)
    pea     1
    pea     -4(%a6)
    pea     0x405ce8b2
    jsr     0x400bb65c
    lea     12(%sp),%sp
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea     16(%sp),%sp
    unlk    %a6
    rts

    .bss
ic_fatto:
    .skip 2
