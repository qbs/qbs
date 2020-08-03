    .global _main
    .type   _main, @function
_main:
    push.l r10
    mov.L r0, r10
    mov.L #0, r5
    mov.L r5, r1
    rtsd #4, r10-r10
