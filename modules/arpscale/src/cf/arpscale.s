| em/arpscale 0.1: SETTINGS > PERSONALIZE > ARP OFS IN SCALE (spento di base). Acceso, gli OFFSET dei passi
| dell'arpeggio si muovono per gradi della scala della traccia; [UP] su un passo lo segna per muoversi in semitoni.
| Generato da tmp_claude/v59/gen_arpscale.py dal sorgente di dnfw scripts/build_arpscale.py (AGPL-3.0): stesso codice,
| la scritta in .rodata e clip/undone in .bss. Interruttore: byte 0x405cd8af. Segni: valore 100 del suono (+0xdc),
| byte 242..243 del suono salvato.
    .text
    .globl as_arp
    .globl as_menu_hook
    .globl as_load_marks
    .globl as_save_marks
    .globl as_step_key
    .globl as_draw_step
    .globl as_rotate_marks
    .globl as_copy_marks
    .globl as_clear_marks
    .globl as_paste_marks
    .globl as_undo_marks

| The as_arp step's tail (0x4002a390), reached by jmp. %d0 the held note, %d1 12 x
| the as_arp's octave, %d2 the step's offset, %d3 the step, %a1 the sound + the
| step; out: %d0 the note, at 0x4002a39c, which restores %d2-%d7/%a2-%a4. %d0
| and %d1 are kept until stock is ruled out.
as_arp:
    tst.l   %d2
    beq.w   arp_stock                   | offset 0: the held note
    movea.l %a1,%a0
    suba.l  %d3,%a0                     | the sound
    move.w  220(%a0),%d4
    btst    %d3,%d4
    bne.w   arp_stock                   | a marked step: semitones
    mvz.b   0x405ce8af,%d3
    moveq   #1,%d4
    cmp.l   %d4,%d3
    bne.w   arp_stock                   | the toggle is off
    move.l  36(%sp),%d3       | the step's caller: only the frame ISR's
    cmpi.l  #0x400266e6,%d3 | two calls have the record at -36(%fp)
    beq.s   1f
    cmpi.l  #0x40026874,%d3
    bne.w   arp_stock
1:
    move.l  -36(%a6),%a2           | the voice record
    move.l  44(%a2),%d3         | the playback copy of the track that made it
    bne.s   1f
    move.l  0x42c5b9a0,%d3           | a live note's record has none: the
    beq.w   arp_stock                   | current copy's start
    bra.s   3f
1:  move.l  16(%a2),%d4        | that track: the record's own, or a
    btst    #1,57(%a2)   | layered copy's source
    beq.s   2f
    move.l  92(%a2),%d4
2:  move.l  #1701,%d5
    muls.l  %d5,%d4
    sub.l   %d4,%d3                     | the copy's start
3:  move.l  16(%a2),%d4        | the record's track's part, and the header
    move.l  #1701,%d5
    muls.l  %d5,%d4
    move.l  %d3,%a4
    adda.l  #0x1dd40,%a4
    add.l   %d3,%d4
    move.l  %d4,%a3
    mvz.b   33(%a4),%d5         | which values are the track's
    btst    #0,%d5
    beq.s   2f
    mvs.b   1171(%a3),%d3
    bra.s   3f
2:  mvs.b   30(%a4),%d3
3:  moveq   #5,%d4
    cmp.l   %d4,%d3
    beq.w   arp_stock                   | CHORD MEMORY: no scale
    btst    #2,%d5
    beq.s   2f
    mvs.b   1170(%a3),%d6
    bra.s   3f
2:  mvs.b   29(%a4),%d6
3:  tst.l   %d6
    bmi.w   arp_stock                   | CHROMATIC
    moveq   #34,%d4
    cmp.l   %d4,%d6
    bgt.w   arp_stock
    btst    #1,%d5
    beq.s   2f
    mvz.b   1172(%a3),%d7
    bra.s   3f
2:  mvz.b   31(%a4),%d7
3:  moveq   #11,%d4
    cmp.l   %d4,%d7
    bhi.w   arp_stock

| The held note's place: %d3 its semitone above the root (0..11), %a1 its
| octave above the root. %d0 is free from here on.
    move.l  %d0,%d3
    sub.l   %d7,%d3
    addi.l  #12,%d3                     | held - root + 12 > 0
    moveq   #12,%d4
    move.l  %d3,%d5
    divu.l  %d4,%d5
    move.l  %d5,%a1
    subq.l  #1,%a1
    muls.l  %d4,%d5
    sub.l   %d5,%d3
    move.l  %d6,%d4                     | the scale's degree row
    moveq   #13,%d5
    muls.l  %d5,%d4
    lea     0x4020f0d8,%a0
    adda.l  %d4,%a0
    mvz.b   (0,%a0,%d3.l),%d4
    tst.l   %d4
    bne.s   arp_found

| Not in the scale: the nearest note that is, the lower one first. The root is
| in every scale (asserted by the build), so this ends within six semitones.
    moveq   #1,%d5
arp_near:
    move.l  %d3,%d0
    sub.l   %d5,%d0
    bpl.s   4f
    addi.l  #12,%d0
    mvz.b   (0,%a0,%d0.l),%d4
    tst.l   %d4
    beq.s   5f
    move.l  %d0,%d3
    subq.l  #1,%a1
    bra.s   arp_found
4:  mvz.b   (0,%a0,%d0.l),%d4
    tst.l   %d4
    beq.s   5f
    move.l  %d0,%d3
    bra.s   arp_found
5:  move.l  %d3,%d0
    add.l   %d5,%d0
    moveq   #12,%d4
    cmp.l   %d4,%d0
    blt.s   6f
    subi.l  #12,%d0
    mvz.b   (0,%a0,%d0.l),%d4
    tst.l   %d4
    beq.s   7f
    move.l  %d0,%d3
    addq.l  #1,%a1
    bra.s   arp_found
6:  mvz.b   (0,%a0,%d0.l),%d4
    tst.l   %d4
    beq.s   7f
    move.l  %d0,%d3
    bra.s   arp_found
7:  addq.l  #1,%d5
    bra.s   arp_near

| %d4 the degree + 1. Add the offset in degrees and carry octaves by the
| scale's length (%d5), counted from its semitone row (%a2).
arp_found:
    subq.l  #1,%d4
    add.l   %d2,%d4
    lea     0x4020e358,%a2
    move.l  %d6,%d5
    lsl.l   #5,%d5
    adda.l  %d5,%a2
    moveq   #0,%d5
8:  moveq   #8,%d0
    cmp.l   %d0,%d5
    beq.s   9f
    move.l  (0,%a2,%d5.l*4),%d0
    bmi.s   9f
    addq.l  #1,%d5
    bra.s   8b
9:  move.l  %d4,%d0                     | floor(%d4 / length), and the degree left
    divs.l  %d5,%d0
    move.l  %d0,%d3
    muls.l  %d5,%d3
    sub.l   %d3,%d4
    bpl.s   1f
    add.l   %d5,%d4
    subq.l  #1,%d0
1:  add.l   %a1,%d0                     | octaves above the root
    moveq   #12,%d3
    muls.l  %d3,%d0
    add.l   (0,%a2,%d4.l*4),%d0
    add.l   %d7,%d0

    add.l   %d1,%d0                     | the as_arp's octave
    jmp     0x4002a39c

arp_stock:
    add.l   %d1,%d0                     | the displaced instructions
    add.l   %d2,%d0
    jmp     0x4002a39c

| The stored sound's load (0x400dd1ea) at its step mask (0x400dd5a0) and the
| save (0x400dd6a6) at the same field (0x400dd8c8), reached by jmp. In the load
| %a3 is the stored sound and %a2 the one in RAM, in the save the other way.
as_load_marks:
    move.w  336(%a3),356(%a2)     | the displaced instruction
    move.w  242(%a3),220(%a2)
    jmp     0x400dd70a

as_save_marks:
    move.w  356(%a3),336(%a2)     | the displaced instruction
    move.w  220(%a3),242(%a2)
    jmp     0x400dda32

| (model) -> %d0 its sound, 0 if none: the model's vtable +40. C convention.
sound_of:
    movea.l 4(%sp),%a0
    movea.l (%a0),%a1
    move.l  %a0,-(%sp)
    movea.l 40(%a1),%a1
    jsr     (%a1)
    addq.l  #4,%sp
    rts

| (model): tell the model's listeners the as_arp changed, with the event stock's
| step setters send (0x4004bdba). C convention.
notify:
    lea     -8(%sp),%sp
    move.l  #0x401de1fc,(%sp)
    move.b  #1,4(%sp)
    move.l  %sp,-(%sp)                  | &event
    move.l  16(%sp),-(%sp)              | the model
    movea.l (%sp),%a0
    movea.l (%a0),%a0
    movea.l 16(%a0),%a0
    jsr     (%a0)
    lea     16(%sp),%sp
    rts

| [UP]/[DOWN] on a step, called in place of the menu's setStepEnabled(model,
| step, up) (0x400197e8). With the toggle on, [UP] on an active step marks it
| and [DOWN] on a marked step clears the mark. Anything else is stock's mute or
| unmute, and leaves the step unmarked. With the toggle off, it is stock's call
| alone, and the marks stay as they are.
as_step_key:
    lea     -8(%sp),%sp
    movem.l %d2/%a2,(%sp)
    move.l  12(%sp),-(%sp)              | the model
    bsr.w   sound_of
    addq.l  #4,%sp
    tst.l   %d0
    beq.s   8f
    movea.l %d0,%a2
    move.l  16(%sp),%d2                 | the step (0..15, the menu's)
    mvz.b   0x405ce8af,%d0
    moveq   #1,%d1
    cmp.l   %d1,%d0
    bne.s   8f                          | the toggle is off: stock, the marks as they are
    tst.b   23(%sp)                     | up?
    beq.s   5f
    move.w  356(%a2),%d0
    btst    %d2,%d0
    beq.s   7f                          | a muted step: stock unmutes it
    move.w  220(%a2),%d0
    bset    %d2,%d0
    bra.s   6f
5:  move.w  220(%a2),%d0
    btst    %d2,%d0
    beq.s   7f                          | an unmarked step: stock mutes it
    bclr    %d2,%d0
6:  move.w  %d0,220(%a2)
    move.l  12(%sp),-(%sp)
    bsr.w   notify
    addq.l  #4,%sp
    bra.s   9f
7:  move.w  220(%a2),%d0
    bclr    %d2,%d0
    move.w  %d0,220(%a2)
8:  move.l  20(%sp),-(%sp)              | stock: setStepEnabled(model, step, up)
    move.l  20(%sp),-(%sp)
    move.l  20(%sp),-(%sp)
    jsr     0x4004bd52
    lea     12(%sp),%sp
9:  movem.l (%sp),%d2/%a2
    lea     8(%sp),%sp
    rts

| The step graph, where stock chooses the filled square or the hollow one
| (0x40018b4a), reached by jmp. %d1, %a4, %a0, %a1 the step's box (x0, y0, x1,
| y1), %d2 the canvas, %d3 the model, %d4 the step, %a5 the box filler
| 0x40114842. With the toggle on, a marked step gets a plus in the box and the
| loop goes on at 0x40018b90; %d6, %d7, %a3 and %a4 are free there.
as_draw_step:
    lea     -16(%sp),%sp
    movem.l %d0-%d1/%a0-%a1,(%sp)
    mvz.b   0x405ce8af,%d0
    moveq   #1,%d1
    cmp.l   %d1,%d0
    bne.s   8f                          | the toggle is off
    move.l  %d3,-(%sp)
    bsr.w   sound_of
    addq.l  #4,%sp
    tst.l   %d0
    beq.s   8f
    movea.l %d0,%a0
    move.w  220(%a0),%d0
    btst    %d4,%d0
    beq.s   8f
    movem.l (%sp),%d0-%d1/%a0-%a1
    lea     16(%sp),%sp
    move.l  %d1,%d6                     | x0
    move.l  %a0,%d7                     | x1
    movea.l %a1,%a3                     | y1; %a4 is y0
    move.l  %a4,%d0
    add.l   %a3,%d0
    asr.l   #1,%d0                      | the middle row
    pea     1                           | the row: (canvas, x0, y, x1, y, 1)
    move.l  %d0,-(%sp)
    move.l  %d7,-(%sp)
    move.l  %d0,-(%sp)
    move.l  %d6,-(%sp)
    move.l  %d2,-(%sp)
    jsr     (%a5)
    lea     24(%sp),%sp
    move.l  %d6,%d0
    add.l   %d7,%d0
    asr.l   #1,%d0                      | the middle column
    pea     1                           | the column: (canvas, x, y0, x, y1, 1)
    move.l  %a3,-(%sp)
    move.l  %d0,-(%sp)
    move.l  %a4,-(%sp)
    move.l  %d0,-(%sp)
    move.l  %d2,-(%sp)
    jsr     (%a5)
    lea     24(%sp),%sp
    jmp     0x40018b90
8:  movem.l (%sp),%d0-%d1/%a0-%a1
    lea     16(%sp),%sp
    tst.l   %d7                         | the displaced instructions
    beq.s   1f
    jmp     0x40018b60
1:  pea     1
    jmp     0x40018b52

| The rotate's getMask(model) (0x40019992), called after it moves the offsets
| and before it moves the mask: the marks move the same way, within LEN, then
| stock's getMask. The rotate's %d2 is the key, %d7 LEN - 1.
as_rotate_marks:
    lea     -16(%sp),%sp
    movem.l %d3-%d5/%a2,(%sp)
    move.l  20(%sp),-(%sp)              | the model
    bsr.w   sound_of
    addq.l  #4,%sp
    tst.l   %d0
    beq.s   9f
    movea.l %d0,%a2
    moveq   #0,%d3
    move.w  220(%a2),%d3
    moveq   #1,%d4
    lsl.l   %d7,%d4
    add.l   %d4,%d4
    subq.l  #1,%d4                      | the steps within LEN
    move.l  %d3,%d5
    and.l   %d4,%d5                     | their marks
    eor.l   %d5,%d3                     | the marks past LEN, kept
    moveq   #13,%d0
    cmp.l   %d2,%d0
    bne.s   2f
    moveq   #1,%d0                      | LEFT: step i takes step i + 1's mark
    and.l   %d5,%d0
    lsl.l   %d7,%d0                     | and the last takes the first's
    lsr.l   #1,%d5
    bra.s   3f
2:  move.l  %d5,%d0                     | RIGHT: step i takes step i - 1's mark
    lsr.l   %d7,%d0                     | and the first takes the last's
    add.l   %d5,%d5
    and.l   %d4,%d5
3:  or.l    %d0,%d5
    or.l    %d5,%d3
    move.w  %d3,220(%a2)
9:  movem.l (%sp),%d3-%d5/%a2
    lea     16(%sp),%sp
    jmp     0x4004bde6

| The menu's copy(model), clear(model, 1), paste(model) and undo(model, kind),
| each called in place of stock's and acting only when stock's returns nonzero.
| They keep the copy's and the undo record's marks (clip, undone) with stock's.
as_copy_marks:
    move.l  4(%sp),-(%sp)
    jsr     0x4004cec4
    addq.l  #4,%sp
    tst.b   %d0
    beq.s   9f
    move.l  %d0,-(%sp)
    move.l  8(%sp),-(%sp)
    bsr.w   sound_of
    addq.l  #4,%sp
    tst.l   %d0
    beq.s   8f
    movea.l %d0,%a0
    lea     clip,%a1
    move.w  220(%a0),(%a1)
8:  move.l  (%sp)+,%d0
9:  rts

as_clear_marks:
    move.l  8(%sp),-(%sp)
    move.l  8(%sp),-(%sp)
    jsr     0x4004d25e
    addq.l  #8,%sp
    tst.b   %d0
    beq.s   9f
    move.l  %d0,-(%sp)
    move.l  8(%sp),-(%sp)
    bsr.w   sound_of
    addq.l  #4,%sp
    tst.l   %d0
    beq.s   8f
    movea.l %d0,%a0
    lea     undone,%a1
    move.w  220(%a0),(%a1)          | stock's clear leaves them: the steps' before
    clr.w   220(%a0)
8:  move.l  (%sp)+,%d0
9:  rts

as_paste_marks:
    move.l  4(%sp),-(%sp)
    jsr     0x4004d3e0
    addq.l  #4,%sp
    tst.b   %d0
    beq.s   9f
    move.l  %d0,-(%sp)
    move.l  8(%sp),-(%sp)
    bsr.w   sound_of
    addq.l  #4,%sp
    tst.l   %d0
    beq.s   8f
    movea.l %d0,%a0
    lea     undone,%a1
    move.w  220(%a0),(%a1)          | stock's paste leaves them: the steps' before
    lea     clip,%a1
    move.w  (%a1),220(%a0)
8:  move.l  (%sp)+,%d0
9:  rts

as_undo_marks:
    move.l  8(%sp),-(%sp)
    move.l  8(%sp),-(%sp)
    jsr     0x4004d108
    addq.l  #8,%sp
    tst.b   %d0
    beq.s   9f
    move.l  %d0,-(%sp)
    move.l  8(%sp),-(%sp)
    bsr.w   sound_of
    addq.l  #4,%sp
    tst.l   %d0
    beq.s   8f
    movea.l %d0,%a0
    lea     undone,%a1
    move.w  (%a1),220(%a0)
8:  move.l  (%sp)+,%d0
9:  rts

| -> %d0 1 when the toggle is on, else 0.
get:
    mvz.b   0x405ce8af,%d0
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
    pea     0x405ce8af
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


| PersonalizeMenuView's constructor, after its last stock item is appended,
| reached by jmp: the displaced destructor calls, then append this item. %a4 is
| the view; %a3 (the destructor) and %d6 are the constructor's, so kept.
as_menu_hook:
    jsr     (%a3)                       | the displaced instructions
    move.l  %d3,-(%sp)
    jsr     (%a3)
    lea     -8(%sp),%sp
    movem.l %d6/%a3,(%sp)
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

    movem.l (%sp),%d6/%a3
    lea     8(%sp),%sp
    jmp     0x4009879a

    .section .rodata
item_name:
    .asciz  "ARP OFS IN SCALE"

| The copy's and the undo record's marks.
    .bss
    .align  2
clip:
    .space  2
undone:
    .space  2
