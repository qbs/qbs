    .global main
    .type   main, @function
main:
    push fp
    mv fp, sp
    ldi r4, #0
    mv r0, r4
    pop fp
    jmp lr
