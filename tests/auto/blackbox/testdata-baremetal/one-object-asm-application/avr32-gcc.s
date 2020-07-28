    .global main
    .type   main, @function
main:
    stm --sp, r7, lr
    mov r7, sp
    mov r8, 0
    mov r12, r8
    ldm sp++, r7, pc
