| em/pagine: il gancio della pagina SYN (ColdFire, sintassi gas, mcpu=54418).
| Entrato con jsr dal sito 0x40018214 (le tre istruzioni move.l %d2,-(%sp); move.l %a2,-(%sp); jsr griglia sono sostituite dalla
| chiamata). Sulla pila c'e' solo il ritorno nel sito; dopo il ritorno il sito esegue addq.l #8,%sp, quindi qui si rimettono
| canvas (d2) e view (a2) sotto l'indirizzo di ritorno e si salta a em_syn(view, canvas), che torna direttamente nel sito.
    .text
    .globl em_syn_hook
em_syn_hook:
    move.l  (%sp)+,%a1
    move.l  %d2,-(%sp)
    move.l  %a2,-(%sp)
    move.l  %a1,-(%sp)
    jmp     em_syn
