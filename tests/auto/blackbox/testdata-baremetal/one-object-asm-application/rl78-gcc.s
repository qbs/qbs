r8 = 0xffef0
.text
    .global _main
    .type   _main, @function
_main:
    subw sp, #2
    clrw ax
    movw [sp], ax
    movw r8, ax
    addw sp, #2
    ret
