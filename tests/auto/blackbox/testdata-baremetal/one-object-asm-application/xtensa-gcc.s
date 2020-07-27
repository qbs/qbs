    .global main
    .type   main, @function
main:
    addi   sp, sp, -16
    s32i.n a15, sp, 12
    mov.n  a15, sp
    movi.n a2, 0
    mov.n  sp, a15
    l32i.n a15, sp, 12
    addi   sp, sp, 16
    ret.n
