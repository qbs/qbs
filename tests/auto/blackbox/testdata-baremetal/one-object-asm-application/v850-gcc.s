    .global _main
    .type   _main, @function
_main:
    add -4, sp
    st.w r29, 0[sp]
    mov sp, r29
    mov 0, r10
    mov r29, sp
    ld.w 0[sp], r29
    add 4, sp
    jmp [r31]
