| em/intro: gancio all'ingresso della routine dell'intro 0x400d39ea(fotogramma, lunghezza, pannello) (jmp; spostate e lasciate le due
| istruzioni lea -60(%sp),%sp ; movem.l %d2-%d7/%a2-%a6,(%sp), rifatte in intro_stock prima del salto a 0x400d39f2).
| Fotogramma 0: la routine di serie (prepara la sorgente). Dal fotogramma 1: intro_frame(fotogramma, pannello) pulisce il pannello e
| scrive ogni punto del nostro fotogramma con setPixel (la strada che arriva allo schermo, come il tunnel), poi si torna subito al
| chiamante. Se intro_frame risponde 0: la routine di serie col fotogramma vero.
    .text
    .globl intro_hook
intro_hook:
    move.l  4(%sp),%d0
    beq.s   intro_stock
    move.l  12(%sp),-(%sp)          | pannello
    move.l  8(%sp),-(%sp)           | fotogramma
    jsr     intro_frame
    addq.l  #8,%sp
    tst.l   %d0
    beq.s   intro_stock
    rts
intro_stock:
    lea     -60(%sp),%sp
    movem.l %d2-%d7/%a2-%a6,(%sp)
    jmp     0x400d39f2
