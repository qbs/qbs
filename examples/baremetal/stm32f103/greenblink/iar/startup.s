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

    MODULE  ?cstartup

    SECTION CSTACK:DATA:NOROOT(3)
    SECTION .intvec:CODE:NOROOT(2)

    EXTERN  __iar_program_start

    PUBLIC  _vectors_table
    DATA
_vectors_table
    ;; Generic interrupts offset.
    DCD     sfe(CSTACK)     ; Initial stack pointer value.
    DCD     reset_handler   ; Reset.
    DCD     0               ; NMI.
    DCD     0               ; Hard fault.
    DCD     0               ; MPU fault.
    DCD     0               ; Bus fault.
    DCD     0               ; Usage fault.
    DCD     0               ; Reserved.
    DCD     0               ; Reserved.
    DCD     0               ; Reserved.
    DCD     0               ; Reserved.
    DCD     0               ; SVC.
    DCD     0               ; Debug monitor.
    DCD     0               ; Reserved.
    DCD     0               ; PendSV.
    DCD     0               ; SysTick.
    ;; External interrupts offset.
    DCD     0               ; Window watchdog.
    DCD     0               ; PVD through EXTI line detection.
    DCD     0               ; Tamper.
    DCD     0               ; RTC.
    DCD     0               ; Flash.
    DCD     0               ; RCC.
    DCD     0               ; EXTI line 0.
    DCD     0               ; EXTI line 1.
    DCD     0               ; EXTI line 2.
    DCD     0               ; EXTI line 3.
    DCD     0               ; EXTI line 4.
    DCD     0               ; DMA1 channel 1.
    DCD     0               ; DMA1 channel 2.
    DCD     0               ; DMA1 channel 3.
    DCD     0               ; DMA1 channel 4.
    DCD     0               ; DMA1 channel 5.
    DCD     0               ; DMA1 channel 6.
    DCD     0               ; DMA1 channel 7.
    DCD     0               ; ADC1/2.
    DCD     0               ; USB high priority or CAN1 TX.
    DCD     0               ; USB low  priority or CAN1 RX0.
    DCD     0               ; CAN1 RX1.
    DCD     0               ; CAN1 SCE.
    DCD     0               ; EXTI line 9..5.
    DCD     0               ; TIM1 break.
    DCD     0               ; TIM1 update.
    DCD     0               ; TIM1 trigger and commutation.
    DCD     0               ; TIM1 capture compare.
    DCD     0               ; TIM2.
    DCD     0               ; TIM3.
    DCD     0               ; TIM4.
    DCD     0               ; I2C1 event.
    DCD     0               ; I2C1 error.
    DCD     0               ; I2C2 event.
    DCD     0               ; I2C2 error.
    DCD     0               ; SPI1.
    DCD     0               ; SPI2.
    DCD     0               ; USART1.
    DCD     0               ; USART2.
    DCD     0               ; USART3.
    DCD     0               ; EXTI line 15..10.
    DCD     0               ; RTC alarm through EXTI line.
    DCD     0               ; USB wakeup from suspend.

    ;; Reset handler.
    THUMB
    PUBWEAK reset_handler
    SECTION .text:CODE:REORDER:NOROOT(2)
reset_handler
    BLX    R0
    LDR    R0, =__iar_program_start
    BX     R0

    END
