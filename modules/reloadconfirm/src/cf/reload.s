| em/reloadconfirm 0.1: SETTINGS > PERSONALIZE > RELOAD CONFIRM (spento di base). Acceso, FUNC + NO chiede
| "ARE YOU SURE YOU WANT TO / RELOAD THE PATTERN? Y/N" prima di ricaricare il pattern.
| Generato da tmp_claude/v59/gen_reload.py dal sorgente di dnfw scripts/build_reload_confirm.py (AGPL-3.0, provato
| sullo strumento il 2026-10-04): stesso codice, solo swallow/vtable in .bss e le scritte in .rodata. Ganci: 0x4005f1a2 (FUNC + NO, 10 B)
| jmp key_hook; 0x4009863e (fine del costruttore di PERSONALIZE, 6 B) jmp menu_hook.
| L'interruttore e' il byte 0x405cd8ae (coda del blocco impostazioni, salvato col blocco).
    .text
    .globl key_hook
    .globl menu_hook

| FUNC + NO, pressed, reached by jmp. Off: the stock reload. On: the prompt.
key_hook:
    mvz.b   0x405ce8ae,%d0
    moveq   #1,%d1
    cmp.l   %d1,%d0
    beq.s   1f
    move.l  160(%a2),-(%sp)             | the displaced instructions
    jsr     0x400f6fc4
    jmp     0x4005f1ac
1:  bsr.w   confirm
    jmp     0x4005ef52

| The prompt's key handler, for our prompt only (`confirm` points its vtable
| here). The prompt opens on NO's press, and a prompt answers on a release
| (0x40112bee): left alone, the release of the NO that opened it would answer
| NO. So that one release is dropped; a fresh NO press first clears the drop.
prompt_keys:
    tst.b   swallow
    beq.s   9f
    move.l  8(%sp),-(%sp)
    jsr     0x401162f8
    addq.l  #4,%sp
    moveq   #12,%d1                     | NO
    cmp.l   %d1,%d0
    bne.s   9f
    move.l  8(%sp),-(%sp)
    jsr     0x40116320
    addq.l  #4,%sp
    tst.l   %d0
    beq.s   1f
    clr.b   swallow                     | the opening NO's release: dropped
    moveq   #1,%d0                      | handled
    rts
1:  move.l  8(%sp),-(%sp)
    jsr     0x4011639c
    addq.l  #4,%sp
    tst.b   %d0
    beq.s   9f
    clr.b   swallow                     | a new NO press: the answer to come
9:  jmp     0x40112e64

| The YES/NO prompt, as the pattern menu's RELOAD builds it (0x401a5e2c).
| Frame: -16 the callback (then the view's shared_ptr), -24 the made one,
| -28 and -32 the two lines, -33 and -34 their allocators.
confirm:
    link    %a6,#-64
    lea     -16(%sp),%sp
    movem.l %d2/%d5/%a2-%a3,(%sp)
    pea     0x8c
    jsr     0x40120544
    addq.l  #4,%sp
    move.l  %d0,%a2
    tst.l   %d0
    beq.w   9f
    moveq   #1,%d0
    move.l  %d0,4(%a2)
    move.l  %d0,8(%a2)
    move.l  #0x401dd7e4,(%a2)
    move.l  %a2,%d5
    addi.l  #16,%d5
    move.l  %d5,12(%a2)
    pea     -33(%a6)
    pea     line1
    pea     -28(%a6)
    jsr     0x401ceab8
    pea     -34(%a6)
    pea     line2
    pea     -32(%a6)
    jsr     0x401ceab8
    pea     1
    jsr     0x40120544
    lea     28(%sp),%sp
    move.l  %d0,-16(%a6)
    lea     0x401a4638,%a0
    move.l  %a0,-8(%a6)
    lea     on_answer,%a0
    move.l  %a0,-4(%a6)
    clr.l   -(%sp)
    pea     -16(%a6)
    pea     -32(%a6)
    pea     -28(%a6)
    move.l  %d5,-(%sp)
    jsr     0x40112d0e
    move.l  %d5,%a1                     | our instance's vtable: a copy, with
    move.l  (%a1),%d0                   | the key handler ours
    cmpi.l  #0x4020671c,%d0
    bne.s   2f                          | not the class read: leave it stock
    lea     vtable,%a0
    lea     0x40206714,%a1
    moveq   #19,%d0
1:  move.l  (%a1)+,(%a0)+
    subq.l  #1,%d0
    bne.s   1b
    lea     vtable+8,%a0
    lea     prompt_keys,%a1
    move.l  %a1,8(%a0)
    move.l  %d5,%a1
    move.l  %a0,(%a1)
    moveq   #1,%d0
    move.b  %d0,swallow
2:
    pea     -16(%a6)
    jsr     0x40188366
    pea     -32(%a6)
    jsr     0x401cd008
    pea     -28(%a6)
    jsr     0x401cd008
    lea     32(%sp),%sp
    move.l  %a2,-20(%a6)
    pea     0x401d3f70
    pea     -20(%a6)
    jsr     0x4018844a
    move.l  %d0,-24(%a6)
    pea     -24(%a6)
    pea     -16(%a6)
    jsr     0x401a578a
    lea     16(%sp),%sp
    jsr     0x4018cbbc
    move.l  %d0,-(%sp)
    jsr     0x4002c47a
    addq.l  #4,%sp
    clr.l   -(%sp)
    pea     -16(%a6)
    move.l  %d0,-(%sp)
    jsr     0x4011d9fe
    pea     -12(%a6)
    jsr     0x401883ac
    pea     -20(%a6)
    jsr     0x401883ac
    lea     20(%sp),%sp
9:  movem.l (%sp),%d2/%d5/%a2-%a3
    lea     16(%sp),%sp
    unlk    %a6
    rts

| The prompt's answer (functor, yes): YES reloads as FUNC + NO does.
on_answer:
    tst.b   11(%sp)
    beq.s   1f
    move.l  %d2,-(%sp)
    jsr     0x4018ad00
    move.l  %d0,-(%sp)
    jsr     0x400f6fc4
    addq.l  #4,%sp
    move.l  %d0,%d2
    jsr     0x4018ac5a
    pea     1
    pea     1
    move.l  %d2,-(%sp)
    pea     1                           | 1: from the temporary save
    move.l  %d0,-(%sp)
    jsr     0x400444da
    lea     20(%sp),%sp
    move.l  (%sp)+,%d2
1:  rts

| -> %d0 1 when the toggle is on, else 0.
get:
    mvz.b   0x405ce8ae,%d0
    subq.l  #1,%d0
    seq     %d0
    andi.l  #1,%d0
    rts

| (on): the toggle, through the stock setting writer.
set:
    link    %a6,#-4
    moveq   #0,%d0
    tst.b   11(%a6)
    sne     %d0
    andi.l  #1,%d0
    move.b  %d0,-4(%a6)
    pea     1
    pea     -4(%a6)
    pea     0x405ce8ae
    jsr     0x400bb65c
    lea     12(%sp),%sp
    unlk    %a6
    rts

| name(&string, functor): the item's name (0x40096464).
name:
    link    %a6,#-4
    move.l  %d2,-(%sp)
    move.l  %a0,%d2
    pea     -1(%a6)
    pea     item_name
    move.l  %a0,-(%sp)
    jsr     0x401ceab8
    lea     12(%sp),%sp
    move.l  %d2,%d0
    move.l  -8(%a6),%d2
    unlk    %a6
    rts

| press(functor): flip, redraw the view (0x400966d8).
press:
    move.l  %a2,-(%sp)
    move.l  8(%sp),%a0
    move.l  (%a0),%a2
    bsr.w   get
    eori.l  #1,%d0
    move.l  %d0,-(%sp)
    bsr.w   set
    addq.l  #4,%sp
    move.l  (%a2),%a2
    lea     56(%a2),%a2
    move.l  %a2,8(%sp)
    move.l  (%sp)+,%a2
    jmp     0x4011ca9a

| value(functor, out, x, y): ON or OFF (0x40097802).
value:
    bsr.w   get
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

| adjust(functor, -, delta): on for a positive turn, off for a negative one
| (0x4009670a), and redraw.
adjust:
    move.l  %a2,-(%sp)
    move.l  %d2,-(%sp)
    move.l  12(%sp),%a0
    move.l  20(%sp),%d2
    move.l  (%a0),%a2
    bsr.w   get
    add.l   %d2,%d0
    bpl.s   1f
    moveq   #0,%d0
1:  moveq   #1,%d1
    cmp.l   %d1,%d0
    ble.s   2f
    move.l  %d1,%d0
2:  move.l  %d0,-(%sp)
    bsr.w   set
    addq.l  #4,%sp
    move.l  (%a2),%a2
    lea     56(%a2),%a2
    move.l  %a2,12(%sp)
    move.l  (%sp)+,%d2
    move.l  (%sp)+,%a2
    jmp     0x4011ca9a


| PersonalizeMenuView's constructor, its epilogue, reached by jmp: append the
| twelfth item, then the displaced movem.l and back. %a4 is the view.
menu_hook:
    lea     -64(%sp),%sp
    move.l  %sp,%a3
    pea     1
    jsr     0x40120544
    move.l  %d0,(%a3)
    clr.l   4(%a3)
    lea     0x40097046,%a0
    move.l  %a0,8(%a3)
    lea     name,%a0
    move.l  %a0,12(%a3)
    pea     4
    jsr     0x40120544
    move.l  %d0,%a0
    move.l  %a4,(%a0)
    move.l  %d0,16(%a3)
    clr.l   20(%a3)
    lea     0x40097098,%a0
    move.l  %a0,24(%a3)
    lea     press,%a0
    move.l  %a0,28(%a3)
    pea     1
    jsr     0x40120544
    move.l  %d0,32(%a3)
    clr.l   36(%a3)
    lea     0x400970f4,%a0
    move.l  %a0,40(%a3)
    lea     value,%a0
    move.l  %a0,44(%a3)
    pea     4
    jsr     0x40120544
    move.l  %d0,%a0
    move.l  %a4,(%a0)
    move.l  %d0,48(%a3)
    clr.l   52(%a3)
    lea     0x40097146,%a0
    move.l  %a0,56(%a3)
    lea     adjust,%a0
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

    movem.l 24(%sp),%d2-%d6/%a2-%a6     | the displaced instruction
    jmp     0x400987a8

    .section .rodata
item_name:
    .asciz  "RELOAD CONFIRM"
line1:
    .asciz  "ARE YOU SURE YOU WANT TO"
line2:
    .asciz  "RELOAD THE PATTERN? Y/N"

    .bss
    .align  2
swallow:
    .space  4
vtable:
    .space  76
