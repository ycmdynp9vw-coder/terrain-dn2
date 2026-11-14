| em/arpmodes 0.1: arpeggiatore SHUF (5) e RAND (6), i nomi che il firmware ha gia'.
| Generato da tmp_claude/v59/gen_arpmodes.py dal sorgente di dnfw scripts/build_arpmodes.py (AGPL-3.0): stesso codice,
| le due cave in un file e la RAM 0x467a0000 come .bss (arp_ram, 2320 B; dnfw non si fida che sia a zero, qui lo e').
| Gancio: 0x4002a13c (12 B, moveq #3,%d0; cmp.l %d4,%d0; beq.w DOWN; bra.w CYCL) -> jmp arpmodes.
    .text
    .globl arpmodes

| Entered from the step's dispatch (0x4002a13c) for MODE > 1, not 2. Live:
| a0 the arp state, a1 the sound, a2 the held-note bitmaps, d1 the track,
| d2 4 x track, d3 the step counter, d4 MODE. The stock tail wants d0 the note
| index (0..127) and +28 the octave, with a0, a1 and d3 as they came.
arpmodes:
    moveq   #3,%d0
    cmp.l   %d4,%d0
    bne.s   1f
    jmp     0x4002a208
1:  subq.l  #5,%d4
    moveq   #1,%d0
    cmp.l   %d4,%d0
    bcc.s   2f                  | 5 SHUF (d4 0), 6 RAND (d4 1)
    jmp     0x4002a28a        | 4, 7, anything else: CYCL, as stock
2:  lea     %a2@(0,%d2:l:4),%a2 | the track's four bitmap words
    moveq   #0,%d5              | H, the notes held
    moveq   #12,%d6
3:  move.l  %a2@(0,%d6:l),%d0
    bra.s   5f
4:  addq.l  #1,%d5
    move.l  %d0,%d7
    subq.l  #1,%d7
    and.l   %d7,%d0             | the lowest bit cleared
5:  bne.s   4b
    subq.l  #4,%d6
    bpl.s   3b
    tst.l   %d5
    beq.s   rest
    mvs.b   %a1@(353),%d6       | RNG
    addq.l  #1,%d6
    bgt.s   6f
    moveq   #1,%d6
6:  move.l  %d5,%d7
    muls.w  %d6,%d7             | N, the range: H x (RNG + 1)
    lea     arp_ram,%a3     | +0 the generator
    move.l  %d1,%d0
    muls.w  #144,%d0
    lea     %a3@(16,%d0:l),%a4  | the track's record
    move.l  %a0@(24),%d0
    cmp.l   %a4@,%d0
    beq.s   7f
    move.l  %d0,%a4@            | a new arp: a new record
    moveq   #-1,%d0
    move.l  %d0,%a4@(8)         | nothing played yet
    clr.l   %a4@(4)             | N 0: SHUF starts a cycle
    move.l  0x466768b0,%d0
    eor.l   %d0,%a3@            | the millisecond it started stirs the generator
7:  move.l  %d7,%d6
    tst.l   %d4
    bne.s   9f
    jmp     shuf
9:  bsr.s   draw                | RAND: any entry, repeats allowed
    bra.s   play

rest:
    jmp     0x4002a39a

| d6 = r (1..65535) -> d0 uniform in 0..r-1. The generator at a3 steps once.
| Clobbers d1, d2.
draw:
    move.l  %a3@,%d0
    bne.s   1f
    move.l  #0x2545f491,%d0     | xorshift's fixed point is zero: never step it
1:  move.l  %d0,%d1
    moveq   #13,%d2
    lsl.l   %d2,%d1
    eor.l   %d1,%d0
    move.l  %d0,%d1
    moveq   #17,%d2
    lsr.l   %d2,%d1
    eor.l   %d1,%d0
    move.l  %d0,%d1
    lsl.l   #5,%d1
    eor.l   %d1,%d0
    move.l  %d0,%a3@
    clr.w   %d0
    swap    %d0                 | the top 16 bits
    mulu.w  %d6,%d0
    clr.w   %d0
    swap    %d0                 | x r / 65536
    rts

| d0 = j, an entry of the range; d5 = H. -> the stock tail with the note and
| its octave, as UP leaves them.
play:
    divu.w  %d5,%d0             | low: octave, high: rank
    move.l  %d0,%d6
    swap    %d6
    mvz.w   %d6,%d6
    mvz.w   %d0,%d0
    move.l  %d0,%a0@(28)
    moveq   #-1,%d0
8:  addq.l  #1,%d0
    moveq   #127,%d1
    cmp.l   %d0,%d1
    bcs.s   rest                | past note 127: the bitmap moved under us
    move.l  %d0,%d1
    lsr.l   #5,%d1
    move.l  %a2@(0,%d1:l:4),%d1
    btst    %d0,%d1
    beq.s   8b
    subq.l  #1,%d6
    bpl.s   8b
    jmp     0x4002a376

| SHUF, from the first cave: a4 the record, d7 = d6 = N, d5 = H, a3 the
| generator. Record: +4 N of this cycle, +8 the last entry, +12 how many this
| cycle has played, +16 one bit per entry played.
shuf:
    moveq   #-1,%d4             | the entry held out of this draw: none
    cmp.l   %a4@(4),%d7
    bne.s   1f
    sub.l   %a4@(12),%d6        | r, what this cycle has left
    bgt.s   3f
1:  move.l  %d7,%a4@(4)         | a new cycle: nothing played
    clr.l   %a4@(12)
    moveq   #31,%d1
2:  clr.l   %a4@(16,%d1:l:4)
    subq.l  #1,%d1
    bpl.s   2b
    move.l  %d7,%d6
    move.l  %a4@(8),%d0         | the entry the last cycle ended on,
    cmp.l   %d7,%d0
    bcc.s   3f                  | (none, or outside this range)
    moveq   #1,%d1
    cmp.l   %d7,%d1
    beq.s   3f                  | (a range of one plays it anyway)
    move.l  %d0,%d4             | ... is held out of the first draw
    bsr.s   mark
    subq.l  #1,%d6
3:  jsr     draw
    move.l  %d0,%d6             | k: the k-th entry not yet played
    moveq   #-1,%d0
4:  addq.l  #1,%d0
    cmp.l   %d7,%d0
    bcc.s   1b                  | past N: the record disagrees with itself, start over
    move.l  %d0,%d1
    lsr.l   #3,%d1
    btst    %d0,%a4@(16,%d1:l)
    bne.s   4b
    subq.l  #1,%d6
    bpl.s   4b
    move.l  %d4,%d1
    bmi.s   5f
    move.l  %d1,%d2
    lsr.l   #3,%d2
    bclr    %d1,%a4@(16,%d2:l)  | the held-out entry is this cycle's again
5:  bsr.s   mark
    addq.l  #1,%a4@(12)
    move.l  %d0,%a4@(8)
    jmp     play

| Entry d0 played this cycle. Clobbers d1.
mark:
    move.l  %d0,%d1
    lsr.l   #3,%d1
    bset    %d0,%a4@(16,%d1:l)
    rts

    .bss
    .align  2
arp_ram:
    .space  2320
