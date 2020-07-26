    .global main
    .type   main, @function
main:
    link.w %fp, #0
    clr.l %d0
    unlk %fp
    rts
