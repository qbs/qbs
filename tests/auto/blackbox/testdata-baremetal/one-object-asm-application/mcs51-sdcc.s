    .globl  main
    .area PSEG  (PAG,XDATA)
    .area XSEG  (XDATA)
    .area HOME  (CODE)
main:
    mov dptr,   #0x0000
    ret
