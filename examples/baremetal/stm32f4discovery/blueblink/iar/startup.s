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

    MODULE  ?cstartup

    SECTION CSTACK:DATA:NOROOT(3)
    SECTION .intvec:CODE:NOROOT(2)

    EXTERN __iar_program_start

    PUBLIC _vectors_table
    DATA
_vectors_table
    ;; Generic interrupts offset.
    DCD    sfe(CSTACK)      ; Initial stack pointer value.
    DCD    reset_handler    ; Reset.
    DCD    0                ; NMI.
    DCD    0                ; Hard fault.
    DCD    0                ; Memory management fault.
    DCD    0                ; Bus fault.
    DCD    0                ; Usage fault.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; SVC.
    DCD    0                ; Debug monitor.
    DCD    0                ; Reserved.
    DCD    0                ; PendSV.
    DCD    0                ; SysTick.
    ;; External interrupts offset.
    DCD    0                ; Window WatchDog.
    DCD    0                ; PVD through EXTI Line detection.
    DCD    0                ; Tamper and TimeStamps through the EXTI line.
    DCD    0                ; RTC Wakeup through the EXTI line.
    DCD    0                ; FLASH.
    DCD    0                ; RCC.
    DCD    0                ; EXTI Line0.
    DCD    0                ; EXTI Line1.
    DCD    0                ; EXTI Line2.
    DCD    0                ; EXTI Line3.
    DCD    0                ; EXTI Line4.
    DCD    0                ; DMA1 Stream 0.
    DCD    0                ; DMA1 Stream 1.
    DCD    0                ; DMA1 Stream 2.
    DCD    0                ; DMA1 Stream 3.
    DCD    0                ; DMA1 Stream 4.
    DCD    0                ; DMA1 Stream 5.
    DCD    0                ; DMA1 Stream 6.
    DCD    0                ; ADC1, ADC2 and ADC3s.
    DCD    0                ; CAN1 TX.
    DCD    0                ; CAN1 RX0.
    DCD    0                ; CAN1 RX1.
    DCD    0                ; CAN1 SCE.
    DCD    0                ; External Line[9:5]s.
    DCD    0                ; TIM1 Break and TIM9.
    DCD    0                ; TIM1 Update and TIM10.
    DCD    0                ; TIM1 Trigger and Commutation and TIM11.
    DCD    0                ; TIM1 Capture Compare.
    DCD    0                ; TIM2.
    DCD    0                ; TIM3.
    DCD    0                ; TIM4.
    DCD    0                ; I2C1 Event.
    DCD    0                ; I2C1 Error.
    DCD    0                ; I2C2 Event.
    DCD    0                ; I2C2 Error.
    DCD    0                ; SPI1.
    DCD    0                ; SPI2.
    DCD    0                ; USART1.
    DCD    0                ; USART2.
    DCD    0                ; USART3.
    DCD    0                ; External Line[15:10]s.
    DCD    0                ; RTC Alarm (A and B) through EXTI Line.
    DCD    0                ; USB OTG FS Wakeup through EXTI line.
    DCD    0                ; TIM8 Break and TIM12.
    DCD    0                ; TIM8 Update and TIM13.
    DCD    0                ; TIM8 Trigger and Commutation and TIM14.
    DCD    0                ; TIM8 Capture Compare.
    DCD    0                ; DMA1 Stream7.
    DCD    0                ; FSMC.
    DCD    0                ; SDIO.
    DCD    0                ; TIM5.
    DCD    0                ; SPI3.
    DCD    0                ; UART4.
    DCD    0                ; UART5.
    DCD    0                ; TIM6 and DAC1&2 underrun errors.
    DCD    0                ; TIM7.
    DCD    0                ; DMA2 Stream 0.
    DCD    0                ; DMA2 Stream 1.
    DCD    0                ; DMA2 Stream 2.
    DCD    0                ; DMA2 Stream 3.
    DCD    0                ; DMA2 Stream 4.
    DCD    0                ; Ethernet.
    DCD    0                ; Ethernet Wakeup through EXTI line.
    DCD    0                ; CAN2 TX.
    DCD    0                ; CAN2 RX0.
    DCD    0                ; CAN2 RX1.
    DCD    0                ; CAN2 SCE.
    DCD    0                ; USB OTG FS.
    DCD    0                ; DMA2 Stream 5.
    DCD    0                ; DMA2 Stream 6.
    DCD    0                ; DMA2 Stream 7.
    DCD    0                ; USART6.
    DCD    0                ; I2C3 event.
    DCD    0                ; I2C3 error.
    DCD    0                ; USB OTG HS End Point 1 Out.
    DCD    0                ; USB OTG HS End Point 1 In.
    DCD    0                ; USB OTG HS Wakeup through EXTI.
    DCD    0                ; USB OTG HS.
    DCD    0                ; DCMI.
    DCD    0                ; CRYP crypto.
    DCD    0                ; Hash and RNG.
    DCD    0                ; FPU.

    ;; Reset handler.
    THUMB
    PUBWEAK reset_handler
    SECTION .text:CODE:REORDER:NOROOT(2)
reset_handler
    BLX    R0
    LDR    R0, =__iar_program_start
    BX     R0

    END
