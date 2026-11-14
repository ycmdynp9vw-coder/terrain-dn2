| em/midiarp 0.1: l'arpeggiatore sulle tracce MIDI (il menu si apre, l'arp suona in MIDI).
| Generato da tmp_claude/v59/gen_midiarp.py dal sorgente di dnfw scripts/build_arp_midi_play.py (AGPL-3.0; provato sullo
| strumento il 2026-09-17, con le correzioni del 2026-10-03 per le copie di TRACK WILL TRIGGER e il pool dei record MIDI):
| stesso codice, la tabella N.LEN (dall'immagine di serie) in .rodata.
    .text
    .globl ma_seq_gate
    .globl ma_live_send
    .globl ma_voice_hook

| Sequencer fork. In: %a0 kit, %d2 track. Out: Z set -> synth producer,
| Z clear -> MIDI producer. Replaces `mvs.w 23770(%a0),%dN; btst %d2,%dN`;
| the stock bne that follows is kept. Preserves every register.
ma_seq_gate:
    lea     -12(%sp),%sp
    movem.l %d0-%d1/%a1,(%sp)
    mvs.w   23770(%a0),%d1
    btst    %d2,%d1
    beq.s   1f                          | synth track: Z set, stock
    move.l  #1163,%d1
    muls.l  %d2,%d1
    lea     0(%a0,%d1.l),%a1
    tst.b   403(%a1)  | arp MODE
    seq     %d0                         | 0xff when the arp is off
    tst.b   %d0                         | off -> Z clear (MIDI); on -> Z set (synth)
1:  movem.l (%sp),%d0-%d1/%a1
    lea     12(%sp),%sp
    rts

| Live fork, MIDI branch. Same argument as both callees: one note event,
| whose first long is the track. Arp on -> the voice starter.
ma_live_send:
    move.l  4(%sp),%a0
    move.l  (%a0),%d0
    moveq   #15,%d1
    cmp.l   %d0,%d1
    bcs.s   2f
    move.l  0x800052a0,%d1
    beq.s   2f
    move.l  %d1,%a1
    move.l  #1163,%d1
    muls.l  %d0,%d1
    add.l   %d1,%a1
    tst.b   403(%a1)
    beq.s   2f
    move.l  52(%a0),%d0                 | a lock list from the MIDI pool (0x40121d06)
    beq.s   11f
    clr.l   52(%a0)
    move.l  %d0,-(%sp)
    jsr     0x4012a694        | back to its own pool, not the engine's
    addq.l  #4,%sp
11: jmp     0x4013801c
2:  jmp     0x4012bb90

| The ISR's voice trigger, (record, time). A MIDI track's note becomes a
| MIDI record on the ISR's outgoing batch; everything else is stock.
ma_voice_hook:
    move.l  4(%sp),%a0
    move.l  16(%a0),%d0
    moveq   #15,%d1
    cmp.l   %d0,%d1
    bcs.s   3f
    mvs.w   0x8000537c,%d1
    btst    %d0,%d1
    bne.s   4f
3:  jmp     0x400db688
4:  move.l  56(%a0),%d1
    btst    #20,%d1
    bne     9f
    btst    #17,%d1                     | a layered copy
    beq.s   16f
    btst    #19,%d1                     | ...is the arp's note or nothing
    beq     3b
    move.l  #1163,%d1
    muls.l  %d0,%d1
    move.l  0x800052a0,%a1
    add.l   %d1,%a1
    tst.b   403(%a1)  | and only while its track's arp is on
    beq     3b
16:
    move.l  0x4460f4b8,%a1          | the pool's free list: the allocator does not
    moveq   #3,%d1             | check it is empty and corrupts low memory,
17: move.l  %a1,%d0                     | so take a record only while 4 are free
    beq     9f                          | (the live MIDI sender allocates unguarded too)
    move.l  92(%a1),%a1
    subq.l  #1,%d1
    bpl.s   17b
    lea     -20(%sp),%sp
    movem.l %d2-%d3/%a2-%a4,(%sp)
    move.l  %a2,%a4                     | the ISR's entry (0x4002695a): the note being played
    move.l  %a0,%a2
    jsr     0x4012a6e8
    move.l  %d0,%a3
    move.l  16(%a2),%d2
    move.l  %d2,8(%a3)                  | track
    moveq   #1,%d0
    move.l  %d0,12(%a3)                 | note on
    move.l  #0x80,%d0
    move.l  %d0,16(%a3)                 | play it
    clr.l   20(%a3)
    moveq   #-1,%d0
    move.l  %d0,24(%a3)                 | the note is the inline entry at +36
    clr.l   32(%a3)
    move.l  28(%sp),48(%a3)             | time, the base of its note-off
    moveq   #2,%d0
    move.l  %d0,52(%a3)                 | live-played: the engine already gated mutes
    clr.b   36(%a3)
    clr.b   37(%a3)
    move.b  2(%a4),38(%a3)              | note: the entry's, which for an arp step is the inline one
    clr.b   41(%a3)
    move.l  64(%a2),%d0                 | the sound it plays with, as the ISR finds it (0x40026708)
    bne.s   5f
    move.l  60(%a2),%d0
    bne.s   5f
    move.l  #1163,%d0
    muls.l  %d2,%d0
    add.l   0x800052a0,%d0
    add.l   #52,%d0
5:  move.l  %d0,%a1
    move.l  44(%a2),%d0                 | the sound its defaults come from (0x4002672c)
    bne.s   12f
    move.l  %a1,%d0
12: move.l  %d0,44(%a3)                 | the MIDI task reads defaults through +44 too:
    move.l  %d0,%a0                     | never leave it null
    move.b  3(%a4),%d1                  | velocity; negative = the sound's
    bpl.s   13f
    move.b  1152(%a0),%d1
    bpl.s   13f
    moveq   #100,%d1
13: move.b  %d1,39(%a3)
    moveq   #0,%d3
    move.b  4(%a4),%d3                  | the note's own length
    move.l  56(%a2),%d1
    btst    #19,%d1                     | the arp is running this note
    beq.s   14f
    move.b  354(%a1),%d3         | N.LEN, an index into the arp's own table,
    moveq   #127,%d1                    | not the trig one: convert it
    and.l   %d1,%d3
    lea     nlen_lut,%a1
    move.b  0(%a1,%d3.l),%d3
14: tst.b   %d3
    bpl.s   6f
    move.b  1153(%a0),%d3     | negative = the sound's (0x40026680)
6:  moveq   #126,%d1
    cmp.l   %d3,%d1
    bcc.s   7f
    move.l  %d1,%d3                     | INF would never be switched off
7:  move.b  %d3,40(%a3)
    tst.l   -180(%fp)
    bne.s   8f
    move.l  %a3,-180(%fp)
    bra.s   10f
8:  move.l  %a3,92(%a5)
10: move.l  %a3,%a5
    movem.l (%sp),%d2-%d3/%a2-%a4
    lea     20(%sp),%sp
9:  moveq   #0,%d0
    rts

| N.LEN -> the trig length index nearest in duration (build-time, from the image).
    .section .rodata
nlen_lut:
    .byte   0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 82, 86, 90, 94, 96, 98, 100, 102, 104, 106, 108, 110, 111, 114, 118, 122, 126, 126
