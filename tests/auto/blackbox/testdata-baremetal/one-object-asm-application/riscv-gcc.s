    .globl  main
    .type   main, @function
main:
    add sp, sp, -16
    sd s0, 8(sp)
    add s0, sp, 16
    li a5, 0
    mv a0, a5
    ld s0, 8(sp)
    add sp, sp, 16
    jr ra
