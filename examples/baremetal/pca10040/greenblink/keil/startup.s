;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Copyright (C) 2020 Denis Shienkov <denis.shienkov@gmail.com>
;; Contact: https://www.qt.io/licensing/
;;
;; This file is part of the examples of Qbs.
;;
;; $QT_BEGIN_LICENSE:BSD$
;; Commercial License Usage
;; Licensees holding valid commercial Qt licenses may use this file in
;; accordance with the commercial license agreement provided with the
;; Software or, alternatively, in accordance with the terms contained in
;; a written agreement between you and The Qt Company. For licensing terms
;; and conditions see https://www.qt.io/terms-conditions. For further
;; information use the contact form at https://www.qt.io/contact-us.
;;
;; BSD License Usage
;; Alternatively, you may use this file under the terms of the BSD license
;; as follows:
;;
;; "Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are
;; met:
;;   * Redistributions of source code must retain the above copyright
;;     notice, this list of conditions and the following disclaimer.
;;   * Redistributions in binary form must reproduce the above copyright
;;     notice, this list of conditions and the following disclaimer in
;;     the documentation and/or other materials provided with the
;;     distribution.
;;   * Neither the name of The Qt Company Ltd nor the names of its
;;     contributors may be used to endorse or promote products derived
;;     from this software without specific prior written permission.
;;
;;
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;; "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;; LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;; A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;; OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;; SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
;; LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;; DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;; THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
;;
;; $QT_END_LICENSE$
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_size_of_stack EQU 0x2000
_size_of_heap EQU 0x2000

;; Stack configuration.
    AREA STACK, NOINIT, READWRITE, ALIGN=3
_start_of_stack SPACE _size_of_stack
_end_of_stack

;; Heap configuration.
    AREA HEAP, NOINIT, READWRITE, ALIGN=3
_start_of_heap SPACE _size_of_heap
_end_of_heap

    PRESERVE8
    THUMB

;; Define the empty vectors table.
    AREA RESET, DATA, READONLY
_vectors_table
    ;; Generic interrupts offset.
    DCD    _end_of_stack    ; Initial stack pointer value.
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
    DCD    0                ; Power clock.
    DCD    0                ; Radio.
    DCD    0                ; UARTE0/UART0.
    DCD    0                ; SPIM0/SPIS0/TWIM0/TWIS0/SPI0/TWI0.
    DCD    0                ; SPIM1/SPIS1/TWIM1/TWIS1/SPI1/TWI1.
    DCD    0                ; NFCT.
    DCD    0                ; GPIOTE.
    DCD    0                ; SAADC.
    DCD    0                ; TIMER0.
    DCD    0                ; TIMER1.
    DCD    0                ; TIMER2.
    DCD    0                ; RTC0.
    DCD    0                ; TEMP.
    DCD    0                ; RNG.
    DCD    0                ; ECB.
    DCD    0                ; CCM/AAR.
    DCD    0                ; WDT.
    DCD    0                ; RTC1.
    DCD    0                ; QDEC.
    DCD    0                ; COMP/LPCOMP.
    DCD    0                ; SWI0/EGU0.
    DCD    0                ; SWI1/EGU1.
    DCD    0                ; SWI2/EGU2.
    DCD    0                ; SWI3/EGU3.
    DCD    0                ; SWI4/EGU4.
    DCD    0                ; SWI5/EGU5.
    DCD    0                ; TIMER3.
    DCD    0                ; TIMER4.
    DCD    0                ; PWM0.
    DCD    0                ; PDM.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; MWU.
    DCD    0                ; PWM1.
    DCD    0                ; PWM2.
    DCD    0                ; SPIM2/SPIS2/SPI2.
    DCD    0                ; RTC2.
    DCD    0                ; I2S.
    DCD    0                ; FPU.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.
    DCD    0                ; Reserved.

_end_of_vectors_table

_size_of_vectors_table EQU _end_of_vectors_table - _vectors_table

    AREA |.text|, CODE, READONLY
;; Reset handler.
reset_handler PROC
    EXPORT reset_handler [WEAK]
    IMPORT main
    LDR    R0, =main
    BX     R0
    ENDP
    ALIGN

    END
