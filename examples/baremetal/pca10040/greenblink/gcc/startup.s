/****************************************************************************
**
** Copyright (C) 2020 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of Qbs.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

.syntax unified
.arch armv7e-m
.thumb

// Define the stack.
.section .stack
.align 3
.equ    _size_of_stack, 8192
.globl _end_of_stack
.globl _stack_limit
_stack_limit:
.space _size_of_stack
.size _stack_limit, . - _stack_limit
_end_of_stack:
.size _end_of_stack, . - _end_of_stack

// Define the heap.
.section .heap
.align 3
.equ _size_of_heap, 8192
.globl _end_of_heap
.globl _heap_limit
_end_of_heap:
.space _size_of_heap
.size _end_of_heap, . - _end_of_heap
_heap_limit:
.size _heap_limit, . - _heap_limit

// Define the empty vectors table.
.section .isr_vector, "ax"
.align 2
.globl _vectors_table
_vectors_table:
    // Generic interrupts offset.
    .word    _end_of_stack  // Initial stack pointer value.
    .word    reset_handler  // Reset.
    .word    0              // NMI.
    .word    0              // Hard fault.
    .word    0              // Memory management fault.
    .word    0              // Bus fault.
    .word    0              // Usage fault.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // SVC.
    .word    0              // Debug monitor.
    .word    0              // Reserved.
    .word    0              // PendSV.
    .word    0              // SysTick.

    // External interrupts offset.
    .word    0              // POWER CLOCK.
    .word    0              // RADIO.
    .word    0              // UARTE0/UART0
    .word    0              // SPIM0/SPIS0/TWIM0/TWIS0/SPI0/TWI0.
    .word    0              // SPIM1/SPIS1/TWIM1/TWIS1/SPI1/TWI1.
    .word    0              // NFCT.
    .word    0              // GPIOTE.
    .word    0              // SAADC.
    .word    0              // TIMER0.
    .word    0              // TIMER1.
    .word    0              // TIMER2.
    .word    0              // RTC0.
    .word    0              // TEMP.
    .word    0              // RNG.
    .word    0              // ECB.
    .word    0              // CCM_AAR.
    .word    0              // WDT.
    .word    0              // RTC1.
    .word    0              // QDEC.
    .word    0              // COMP_LPCOMP.
    .word    0              // SWI0_EGU0.
    .word    0              // SWI1_EGU1.
    .word    0              // SWI2_EGU2.
    .word    0              // SWI3_EGU3.
    .word    0              // SWI4_EGU4.
    .word    0              // SWI5_EGU5.
    .word    0              // TIMER3.
    .word    0              // TIMER4.
    .word    0              // PWM0.
    .word    0              // PDM.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // MWU.
    .word    0              // PWM1.
    .word    0              // PWM2.
    .word    0              // SPIM2/SPIS2/SPI2.
    .word    0              // RTC2.
    .word    0              // I2S.
    .word    0              // FPU.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
    .word    0              // Reserved.
.size _vectors_table, . - _vectors_table

// Define the 'reset_handler' function, which is an entry point.
.text
.thumb
.thumb_func
.align 1
.globl reset_handler
.type reset_handler, %function
reset_handler:
    // Loop to copy data from read only memory to RAM.
    ldr     r1, =_end_of_code_section
    ldr     r2, =_start_of_data_section
    ldr     r3, =__bss_start__
    subs    r3, r3, r2
    ble    _copy_data_init_done
_copy_data_init:
    subs    r3, r3, #4
    ldr     r0, [r1,r3]
    str     r0, [r2,r3]
    bgt     _copy_data_init
_copy_data_init_done:
    // Zero fill the bss segment.
    ldr     r1, =__bss_start__
    ldr     r2, =__bss_end__
    movs    r0, 0
    subs    r2, r2, r1
    ble     _loop_fill_zero_bss_done

_loop_fill_zero_bss:
    subs    r2, r2, #4
    str     r0, [r1, r2]
    bgt     _loop_fill_zero_bss

_loop_fill_zero_bss_done:
    // Call the application's entry point.
    bl      main
    bx      lr
.size reset_handler, . - reset_handler
