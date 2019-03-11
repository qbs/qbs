/****************************************************************************
**
** Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
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
.cpu cortex-m4
.fpu softvfp
.thumb

// These symbols are exported from the linker script.
.word _start_of_init_data_section
.word _start_of_data_section
.word _end_of_data_section
.word _start_of_bss_section
.word _end_of_bss_section
.word _end_of_stack

// Define the 'reset_handler' function, which is an entry point.
.section .text.reset_handler
.weak reset_handler
.type reset_handler, %function
reset_handler:
    // Set the stack pointer.
    ldr     sp, =_end_of_stack

    // Copy the data segment initializers from flash to SRAM.
    movs    r1, #0
    b       _loop_copy_data_init
_copy_data_init:
    ldr     r3, =_start_of_init_data_section
    ldr     r3, [r3, r1]
    str     r3, [r0, r1]
    adds    r1, r1, #4
_loop_copy_data_init:
    ldr     r0, =_start_of_data_section
    ldr     r3, =_end_of_data_section
    adds    r2, r0, r1
    cmp     r2, r3
    bcc     _copy_data_init
    ldr     r2, =_start_of_bss_section
    b       _loop_fill_zero_bss

    // Zero fill the bss segment.
_fill_zero_bss:
    movs    r3, #0
    str     r3, [r2], #4
_loop_fill_zero_bss:
    ldr     r3, = _end_of_bss_section
    cmp     r2, r3
    bcc     _fill_zero_bss

    // Call the static constructors.
    bl      __libc_init_array

    // Call the application's entry point.
    bl      main
    bx      lr
.size reset_handler, .-reset_handler

// Define the empty vectors table.
.section .isr_vector,"a",%progbits
.type _vectors_table, %object
.size _vectors_table, .-_vectors_table
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
    .word    0              // Window WatchDog.
    .word    0              // PVD through EXTI Line detection.
    .word    0              // Tamper and TimeStamps through the EXTI line.
    .word    0              // RTC Wakeup through the EXTI line.
    .word    0              // FLASH.
    .word    0              // RCC.
    .word    0              // EXTI Line0.
    .word    0              // EXTI Line1.
    .word    0              // EXTI Line2.
    .word    0              // EXTI Line3.
    .word    0              // EXTI Line4.
    .word    0              // DMA1 Stream 0.
    .word    0              // DMA1 Stream 1.
    .word    0              // DMA1 Stream 2.
    .word    0              // DMA1 Stream 3.
    .word    0              // DMA1 Stream 4.
    .word    0              // DMA1 Stream 5.
    .word    0              // DMA1 Stream 6.
    .word    0              // ADC1, ADC2 and ADC3s.
    .word    0              // CAN1 TX.
    .word    0              // CAN1 RX0.
    .word    0              // CAN1 RX1.
    .word    0              // CAN1 SCE.
    .word    0              // External Line[9:5]s.
    .word    0              // TIM1 Break and TIM9.
    .word    0              // TIM1 Update and TIM10.
    .word    0              // TIM1 Trigger and Commutation and TIM11.
    .word    0              // TIM1 Capture Compare.
    .word    0              // TIM2.
    .word    0              // TIM3.
    .word    0              // TIM4.
    .word    0              // I2C1 Event.
    .word    0              // I2C1 Error.
    .word    0              // I2C2 Event.
    .word    0              // I2C2 Error.
    .word    0              // SPI1.
    .word    0              // SPI2.
    .word    0              // USART1.
    .word    0              // USART2.
    .word    0              // USART3.
    .word    0              // External Line[15:10]s.
    .word    0              // RTC Alarm (A and B) through EXTI Line.
    .word    0              // USB OTG FS Wakeup through EXTI line.
    .word    0              // TIM8 Break and TIM12.
    .word    0              // TIM8 Update and TIM13.
    .word    0              // TIM8 Trigger and Commutation and TIM14.
    .word    0              // TIM8 Capture Compare.
    .word    0              // DMA1 Stream7.
    .word    0              // FSMC.
    .word    0              // SDIO.
    .word    0              // TIM5.
    .word    0              // SPI3.
    .word    0              // UART4.
    .word    0              // UART5.
    .word    0              // TIM6 and DAC1&2 underrun errors.
    .word    0              // TIM7.
    .word    0              // DMA2 Stream 0.
    .word    0              // DMA2 Stream 1.
    .word    0              // DMA2 Stream 2.
    .word    0              // DMA2 Stream 3.
    .word    0              // DMA2 Stream 4.
    .word    0              // Ethernet.
    .word    0              // Ethernet Wakeup through EXTI line.
    .word    0              // CAN2 TX.
    .word    0              // CAN2 RX0.
    .word    0              // CAN2 RX1.
    .word    0              // CAN2 SCE.
    .word    0              // USB OTG FS.
    .word    0              // DMA2 Stream 5.
    .word    0              // DMA2 Stream 6.
    .word    0              // DMA2 Stream 7.
    .word    0              // USART6.
    .word    0              // I2C3 event.
    .word    0              // I2C3 error.
    .word    0              // USB OTG HS End Point 1 Out.
    .word    0              // USB OTG HS End Point 1 In.
    .word    0              // USB OTG HS Wakeup through EXTI.
    .word    0              // USB OTG HS.
    .word    0              // DCMI.
    .word    0              // CRYP crypto.
    .word    0              // Hash and Rng.
    .word    0              // FPU.
