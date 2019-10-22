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

#ifndef FX2_REGS_H
#define FX2_REGS_H

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// XDATA registers.
//

XDATA_REG(GPIF_WAVE_DATA, 0xE400) // GPIF waveform descriptor.
XDATA_REG(RES_WAVEDATA_END, 0xE480) // Reserved.

// General configuration.

XDATA_REG(CPUCS, 0xE600) // Control & status.
XDATA_REG(IFCONFIG, 0xE601) // Interface configuration.
XDATA_REG(PINFLAGSAB, 0xE602) // FIFO FLAGA and FLAGB assignments.
XDATA_REG(PINFLAGSCD, 0xE603) // FIFO FLAGC and FLAGD assignments.
XDATA_REG(FIFORESET, 0xE604) // Restore FIFOS to default state.
XDATA_REG(BREAKPT, 0xE605) // Breakpoint.
XDATA_REG(BPADDRH, 0xE606) // Breakpoint address H.
XDATA_REG(BPADDRL, 0xE607) // Breakpoint address L.
XDATA_REG(UART230, 0xE608) // 230 Kbaud clock for T0,T1,T2.
XDATA_REG(FIFOPINPOLAR, 0xE609) // FIFO polarities.
XDATA_REG(REVID, 0xE60A) // Chip revision.
XDATA_REG(REVCTL, 0xE60B) // Chip revision control.

// Endpoint configuration.

XDATA_REG(EP1OUTCFG, 0xE610) // Endpoint 1-out configuration.
XDATA_REG(EP1INCFG, 0xE611) // Endpoint 1-in configuration.
XDATA_REG(EP2CFG, 0xE612) // Endpoint 2 configuration.
XDATA_REG(EP4CFG, 0xE613) // Endpoint 4 configuration.
XDATA_REG(EP6CFG, 0xE614) // Endpoint 6 configuration.
XDATA_REG(EP8CFG, 0xE615) // Endpoint 8 configuration.
XDATA_REG(EP2FIFOCFG, 0xE618) // Endpoint 2 FIFO configuration.
XDATA_REG(EP4FIFOCFG, 0xE619) // Endpoint 4 FIFO configuration.
XDATA_REG(EP6FIFOCFG, 0xE61A) // Endpoint 6 FIFO configuration.
XDATA_REG(EP8FIFOCFG, 0xE61B) // Endpoint 8 FIFO configuration.
XDATA_REG(EP2AUTOINLENH, 0xE620) // Endpoint 2 packet length H (in only).
XDATA_REG(EP2AUTOINLENL, 0xE621) // Endpoint 2 packet length L (in only).
XDATA_REG(EP4AUTOINLENH, 0xE622) // Endpoint 4 packet length H (in only).
XDATA_REG(EP4AUTOINLENL, 0xE623) // Endpoint 4 packet length L (in only).
XDATA_REG(EP6AUTOINLENH, 0xE624) // Endpoint 6 packet length H (in only).
XDATA_REG(EP6AUTOINLENL, 0xE625) // Endpoint 6 packet length L (in only).
XDATA_REG(EP8AUTOINLENH, 0xE626) // Endpoint 8 packet length H (in only).
XDATA_REG(EP8AUTOINLENL, 0xE627) // Endpoint 8 packet length L (in only).
XDATA_REG(EP2FIFOPFH, 0xE630) // EP2 programmable flag trigger H.
XDATA_REG(EP2FIFOPFL, 0xE631) // EP2 programmable flag trigger L.
XDATA_REG(EP4FIFOPFH, 0xE632) // EP4 programmable flag trigger H.
XDATA_REG(EP4FIFOPFL, 0xE633) // EP4 programmable flag trigger L.
XDATA_REG(EP6FIFOPFH, 0xE634) // EP6 programmable flag trigger H.
XDATA_REG(EP6FIFOPFL, 0xE635) // EP6 programmable flag trigger L.
XDATA_REG(EP8FIFOPFH, 0xE636) // EP8 programmable flag trigger H.
XDATA_REG(EP8FIFOPFL, 0xE637) // EP8 programmable flag trigger L.
XDATA_REG(EP2ISOINPKTS, 0xE640) // EP2 (if ISO) in packets per frame (1-3).
XDATA_REG(EP4ISOINPKTS, 0xE641) // EP4 (if ISO) in packets per frame (1-3).
XDATA_REG(EP6ISOINPKTS, 0xE642) // EP6 (if ISO) in packets per frame (1-3).
XDATA_REG(EP8ISOINPKTS, 0xE643) // EP8 (if ISO) in packets per frame (1-3).
XDATA_REG(INPKTEND, 0xE648) // Force in packet end.
XDATA_REG(OUTPKTEND, 0xE649) // Force out packet end.

// Interrupts.

XDATA_REG(EP2FIFOIE, 0xE650) // Endpoint 2 flag interrupt enable.
XDATA_REG(EP2FIFOIRQ, 0xE651) // Endpoint 2 flag interrupt request.
XDATA_REG(EP4FIFOIE, 0xE652) // Endpoint 4 flag interrupt enable.
XDATA_REG(EP4FIFOIRQ, 0xE653) // Endpoint 4 flag interrupt request.
XDATA_REG(EP6FIFOIE, 0xE654) // Endpoint 6 flag interrupt enable.
XDATA_REG(EP6FIFOIRQ, 0xE655) // Endpoint 6 flag interrupt request.
XDATA_REG(EP8FIFOIE, 0xE656) // Endpoint 8 flag interrupt enable.
XDATA_REG(EP8FIFOIRQ, 0xE657) // Endpoint 8 flag interrupt request.
XDATA_REG(IBNIE, 0xE658) // IN-BULK-NAK interrupt enable.
XDATA_REG(IBNIRQ, 0xE659) // IN-BULK-NAK interrupt request.
XDATA_REG(NAKIE, 0xE65A) // Endpoint ping NAK interrupt enable.
XDATA_REG(NAKIRQ, 0xE65B) // Endpoint ping NAK interrupt request.
XDATA_REG(USBIE, 0xE65C) // USB Int enables.
XDATA_REG(USBIRQ, 0xE65D) // USB interrupt requests.
XDATA_REG(EPIE, 0xE65E) // Endpoint interrupt enables.
XDATA_REG(EPIRQ, 0xE65F) // Endpoint interrupt requests.
XDATA_REG(GPIFIE, 0xE660) // GPIF interrupt enable.
XDATA_REG(GPIFIRQ, 0xE661) // GPIF interrupt request.
XDATA_REG(USBERRIE, 0xE662) // USB error interrupt enables.
XDATA_REG(USBERRIRQ, 0xE663) // USB error interrupt requests.
XDATA_REG(ERRCNTLIM, 0xE664) // USB error counter and limit.
XDATA_REG(CLRERRCNT, 0xE665) // Clear error counter EC[3..0].
XDATA_REG(INT2IVEC, 0xE666) // Interrupt 2 (USB) autovector.
XDATA_REG(INT4IVEC, 0xE667) // Interrupt 4 (FIFOS & GPIF) autovector.
XDATA_REG(INTSETUP, 0xE668) // interrupt 2 & 4 setup.

// Input/Output.

XDATA_REG(PORTACFG, 0xE670) // I/O PORTA alternate configuration.
XDATA_REG(PORTCCFG, 0xE671) // I/O PORTC alternate configuration.
XDATA_REG(PORTECFG, 0xE672) // I/O PORTE alternate configuration.
XDATA_REG(I2CS, 0xE678) // Bus control & status.
XDATA_REG(I2DAT, 0xE679) // Bus data.
XDATA_REG(I2CTL, 0xE67A) // Bus control.
XDATA_REG(XAUTODAT1, 0xE67B) // Autoptr1 movx access.
XDATA_REG(XAUTODAT2, 0xE67C) // Autoptr2 movx access.

// USB Control.

XDATA_REG(USBCS, 0xE680) // USB control & status.
XDATA_REG(SUSPEND, 0xE681) // Put chip into suspend.
XDATA_REG(WAKEUPCS, 0xE682) // Wakeup source and polarity.
XDATA_REG(TOGCTL, 0xE683) // Toggle control.
XDATA_REG(USBFRAMEH, 0xE684) // USB frame count H.
XDATA_REG(USBFRAMEL, 0xE685) // USB frame count L.
XDATA_REG(MICROFRAME, 0xE686) // Microframe count, 0-7.
XDATA_REG(FNADDR, 0xE687) // USB function address.

// Endpoints.

XDATA_REG(EP0BCH, 0xE68A) // Endpoint 0 byte count H.
XDATA_REG(EP0BCL, 0xE68B) // Endpoint 0 byte count L.
XDATA_REG(EP1OUTBC, 0xE68D) // Endpoint 1 out byte count.
XDATA_REG(EP1INBC, 0xE68F) // Endpoint 1 in byte count.
XDATA_REG(EP2BCH, 0xE690) // Endpoint 2 byte count H.
XDATA_REG(EP2BCL, 0xE691) // Endpoint 2 byte count L.
XDATA_REG(EP4BCH, 0xE694) // Endpoint 4 byte count H.
XDATA_REG(EP4BCL, 0xE695) // Endpoint 4 byte count L.
XDATA_REG(EP6BCH, 0xE698) // Endpoint 6 byte count H.
XDATA_REG(EP6BCL, 0xE699) // Endpoint 6 byte count L.
XDATA_REG(EP8BCH, 0xE69C) // Endpoint 8 byte count H.
XDATA_REG(EP8BCL, 0xE69D) // Endpoint 8 byte count L.
XDATA_REG(EP0CS, 0xE6A0) // Endpoint control and status.
XDATA_REG(EP1OUTCS, 0xE6A1) // Endpoint 1 out control and status.
XDATA_REG(EP1INCS, 0xE6A2) // Endpoint 1 in control and status.
XDATA_REG(EP2CS, 0xE6A3) // Endpoint 2 control and status.
XDATA_REG(EP4CS, 0xE6A4) // Endpoint 4 control and status.
XDATA_REG(EP6CS, 0xE6A5) // Endpoint 6 control and status.
XDATA_REG(EP8CS, 0xE6A6) // Endpoint 8 control and status.
XDATA_REG(EP2FIFOFLGS, 0xE6A7) // Endpoint 2 flags.
XDATA_REG(EP4FIFOFLGS, 0xE6A8) // Endpoint 4 flags.
XDATA_REG(EP6FIFOFLGS, 0xE6A9) // Endpoint 6 flags.
XDATA_REG(EP8FIFOFLGS, 0xE6AA) // Endpoint 8 flags.
XDATA_REG(EP2FIFOBCH, 0xE6AB) // EP2 FIFO total byte count H.
XDATA_REG(EP2FIFOBCL, 0xE6AC) // EP2 FIFO total byte count L.
XDATA_REG(EP4FIFOBCH, 0xE6AD) // EP4 FIFO total byte count H.
XDATA_REG(EP4FIFOBCL, 0xE6AE) // EP4 FIFO total byte count L.
XDATA_REG(EP6FIFOBCH, 0xE6AF) // EP6 FIFO total byte count H.
XDATA_REG(EP6FIFOBCL, 0xE6B0) // EP6 FIFO total byte count L.
XDATA_REG(EP8FIFOBCH, 0xE6B1) // EP8 FIFO total byte count H.
XDATA_REG(EP8FIFOBCL, 0xE6B2) // EP8 FIFO total byte count L.
XDATA_REG(SUDPTRH, 0xE6B3) // Setup data pointer high address byte.
XDATA_REG(SUDPTRL, 0xE6B4) // Setup data pointer low address byte.
XDATA_REG(SUDPTRCTL, 0xE6B5) // Setup data pointer auto mode.
XDATA_REG(SETUPDAT[8], 0xE6B8) // 8 bytes of setup data.

// GPIF.

XDATA_REG(GPIFWFSELECT, 0xE6C0) // Waveform selector.
XDATA_REG(GPIFIDLECS, 0xE6C1) // GPIF done, GPIF idle drive mode.
XDATA_REG(GPIFIDLECTL, 0xE6C2) // Inactive bus, ctl states.
XDATA_REG(GPIFCTLCFG, 0xE6C3) // CTL out pin drive.
XDATA_REG(GPIFADRH, 0xE6C4) // GPIF address H.
XDATA_REG(GPIFADRL, 0xE6C5) // GPIF address L.

XDATA_REG(GPIFTCB3, 0xE6CE) // GPIF transaction count byte 3.
XDATA_REG(GPIFTCB2, 0xE6CF) // GPIF transaction count byte 2.
XDATA_REG(GPIFTCB1, 0xE6D0) // GPIF transaction count byte 1.
XDATA_REG(GPIFTCB0, 0xE6D1) // GPIF transaction count byte 0.

XDATA_REG(EP2GPIFFLGSEL, 0xE6D2) // EP2 GPIF flag select.
XDATA_REG(EP2GPIFPFSTOP, 0xE6D3) // Stop GPIF EP2 transaction on prog flag.
XDATA_REG(EP2GPIFTRIG, 0xE6D4) // EP2 FIFO trigger.
XDATA_REG(EP4GPIFFLGSEL, 0xE6DA) // EP4 GPIF flag select.
XDATA_REG(EP4GPIFPFSTOP, 0xE6DB) // Stop GPIF EP4 transaction on prog flag.
XDATA_REG(EP4GPIFTRIG, 0xE6DC) // EP4 FIFO trigger.
XDATA_REG(EP6GPIFFLGSEL, 0xE6E2) // EP6 GPIF Flag select.
XDATA_REG(EP6GPIFPFSTOP, 0xE6E3) // Stop GPIF EP6 transaction on prog flag.
XDATA_REG(EP6GPIFTRIG, 0xE6E4) // EP6 FIFO trigger.
XDATA_REG(EP8GPIFFLGSEL, 0xE6EA) // EP8 GPIF flag select.
XDATA_REG(EP8GPIFPFSTOP, 0xE6EB) // Stop GPIF EP8 transaction on prog flag.
XDATA_REG(EP8GPIFTRIG, 0xE6EC) // EP8 FIFO trigger.
XDATA_REG(XGPIFSGLDATH, 0xE6F0) // GPIF data H (16-bit mode only).
XDATA_REG(XGPIFSGLDATLX, 0xE6F1) // Read/write GPIF data L & trigger transac.
XDATA_REG(XGPIFSGLDATLNOX, 0xE6F2) // Read GPIF data L, no transac trigger.
XDATA_REG(GPIFREADYCFG, 0xE6F3) // Internal RDY,Sync/Async, RDY5CFG.
XDATA_REG(GPIFREADYSTAT, 0xE6F4) // RDY pin states.
XDATA_REG(GPIFABORT, 0xE6F5) // Abort GPIF cycles.

// UDMA.

XDATA_REG(FLOWSTATE, 0xE6C6) // Defines GPIF flow state.
XDATA_REG(FLOWLOGIC, 0xE6C7) // Defines flow/hold decision criteria.
XDATA_REG(FLOWEQ0CTL, 0xE6C8) // CTL states during active flow state.
XDATA_REG(FLOWEQ1CTL, 0xE6C9) // CTL states during hold flow state.
XDATA_REG(FLOWHOLDOFF, 0xE6CA) // Holdoff configuration.
XDATA_REG(FLOWSTB, 0xE6CB) // CTL/RDY signal to use as master data strobe.
XDATA_REG(FLOWSTBEDGE, 0xE6CC) // Defines active master strobe edge.
XDATA_REG(FLOWSTBHPERIOD, 0xE6CD) // Half period of output master strobe.
XDATA_REG(GPIFHOLDAMOUNT, 0xE60C) // Data delay shift.
XDATA_REG(UDMACRCH, 0xE67D) // CRC upper byte.
XDATA_REG(UDMACRCL, 0xE67E) // CRC lower byte.
XDATA_REG(UDMACRCQUAL, 0xE67F) // UDMA in only, host terminated use only.

// Endpoint buffers.
#define MAX_EP0_SIZE 64
#define MAX_EP1_SIZE 64
#define MAX_EP2468_SIZE 1024

XDATA_REG(EP0BUF[MAX_EP0_SIZE], 0xE740) // EP0 buffer (in or out).
XDATA_REG(EP1OUTBUF[MAX_EP1_SIZE], 0xE780) // EP1 buffer (out only).
XDATA_REG(EP1INBUF[MAX_EP1_SIZE], 0xE7C0) // EP1 buffer (in only).
XDATA_REG(EP2FIFOBUF[MAX_EP2468_SIZE], 0xF000) // 512/1024-byte EP2 buffer (in or out).
XDATA_REG(EP4FIFOBUF[MAX_EP2468_SIZE], 0xF400) // 512 byte EP4 buffer (in or out).
XDATA_REG(EP6FIFOBUF[MAX_EP2468_SIZE], 0xF800) // 512/1024-byte EP6 buffer (in or out).
XDATA_REG(EP8FIFOBUF[MAX_EP2468_SIZE], 0xFC00) // 512 byte EP8 buffer (in or out).

// Error correction code (ECC) registers (FX2LP/FX1 only).

XDATA_REG(ECCCFG, 0xE628) // ECC configuration.
XDATA_REG(ECCRESET, 0xE629) // ECC reset.
XDATA_REG(ECC1B0, 0xE62A) // ECC1 byte 0.
XDATA_REG(ECC1B1, 0xE62B) // ECC1 byte 1.
XDATA_REG(ECC1B2, 0xE62C) // ECC1 byte 2.
XDATA_REG(ECC2B0, 0xE62D) // ECC2 byte 0.
XDATA_REG(ECC2B1, 0xE62E) // ECC2 byte 1.
XDATA_REG(ECC2B2, 0xE62F) // ECC2 byte 2.

// Feature registers(FX2LP/FX1 only).

XDATA_REG(GPCR2, 0xE50D) // Chip features.

//
// SFR registers.
//

SPEC_FUN_REG(IOA, 0x80) // Port A.
SPEC_FUN_REG_BIT(PA0, 0x80, 0)
SPEC_FUN_REG_BIT(PA1, 0x80, 1)
SPEC_FUN_REG_BIT(PA2, 0x80, 2)
SPEC_FUN_REG_BIT(PA3, 0x80, 3)
SPEC_FUN_REG_BIT(PA4, 0x80, 4)
SPEC_FUN_REG_BIT(PA5, 0x80, 5)
SPEC_FUN_REG_BIT(PA6, 0x80, 6)
SPEC_FUN_REG_BIT(PA7, 0x80, 7)

SPEC_FUN_REG(SP, 0x81) // Stack pointer.
SPEC_FUN_REG(DPL, 0x82) // Data pointer 0 L.
SPEC_FUN_REG(DPH, 0x83) // Data pointer 0 H.
SPEC_FUN_REG(DPL1, 0x84) // Data pointer 1 L.
SPEC_FUN_REG(DPH1, 0x85) // Data pointer 1 L.
SPEC_FUN_REG(DPS, 0x86) // Data pointer 0/1 select.
SPEC_FUN_REG(PCON, 0x87) // Power control.

SPEC_FUN_REG(TCON, 0x88) // Timer/counter control.
SPEC_FUN_REG_BIT(IT0, 0x88, 0)
SPEC_FUN_REG_BIT(IE0, 0x88, 1)
SPEC_FUN_REG_BIT(IT1, 0x88, 2)
SPEC_FUN_REG_BIT(IE1, 0x88, 3)
SPEC_FUN_REG_BIT(TR0, 0x88, 4)
SPEC_FUN_REG_BIT(TF0, 0x88, 5)
SPEC_FUN_REG_BIT(TR1, 0x88, 6)
SPEC_FUN_REG_BIT(TF1, 0x88, 7)

SPEC_FUN_REG(TMOD, 0x89) // Timer/counter mode control.
SPEC_FUN_REG(TL0, 0x8A) // Timer 0 reload L.
SPEC_FUN_REG(TL1, 0x8B) // Timer 1 reload L.
SPEC_FUN_REG(TH0, 0x8C) // Timer 0 reload H.
SPEC_FUN_REG(TH1, 0x8D) // Timer 1 reload H.
SPEC_FUN_REG(CKCON, 0x8E) // Clock control.

SPEC_FUN_REG(IOB, 0x90) // Port B.
SPEC_FUN_REG_BIT(PB0, 0x90, 0)
SPEC_FUN_REG_BIT(PB1, 0x90, 1)
SPEC_FUN_REG_BIT(PB2, 0x90, 2)
SPEC_FUN_REG_BIT(PB3, 0x90, 3)
SPEC_FUN_REG_BIT(PB4, 0x90, 4)
SPEC_FUN_REG_BIT(PB5, 0x90, 5)
SPEC_FUN_REG_BIT(PB6, 0x90, 6)
SPEC_FUN_REG_BIT(PB7, 0x90, 7)

SPEC_FUN_REG(EXIF, 0x91) // External interrupt flag.
SPEC_FUN_REG(MPAGE, 0x92) // Upper address page of movx.

SPEC_FUN_REG(SCON0, 0x98) // Serial port 0 control.
SPEC_FUN_REG_BIT(RI, 0x98, 0)
SPEC_FUN_REG_BIT(TI, 0x98, 1)
SPEC_FUN_REG_BIT(RB8, 0x98, 2)
SPEC_FUN_REG_BIT(TB8, 0x98, 3)
SPEC_FUN_REG_BIT(REN, 0x98, 4)
SPEC_FUN_REG_BIT(SM2, 0x98, 5)
SPEC_FUN_REG_BIT(SM1, 0x98, 6)
SPEC_FUN_REG_BIT(SM0, 0x98, 7)

SPEC_FUN_REG(SBUF0, 0x99) // Serial port 0 data buffer.

SPEC_FUN_REG(AUTOPTRH1, 0x9A) // Auto-pointer 1 address H.
SPEC_FUN_REG(AUTOPTRL1, 0x9B) // Auto-pointer 1 address L.
SPEC_FUN_REG(AUTOPTRH2, 0x9D) // Auto-pointer 2 address H.
SPEC_FUN_REG(AUTOPTRL2, 0x9E) // Auto-pointer 2 address L.

SPEC_FUN_REG(IOC, 0xA0) // Port C.
SPEC_FUN_REG_BIT(PC0, 0xA0, 0)
SPEC_FUN_REG_BIT(PC1, 0xA0, 1)
SPEC_FUN_REG_BIT(PC2, 0xA0, 2)
SPEC_FUN_REG_BIT(PC3, 0xA0, 3)
SPEC_FUN_REG_BIT(PC4, 0xA0, 4)
SPEC_FUN_REG_BIT(PC5, 0xA0, 5)
SPEC_FUN_REG_BIT(PC6, 0xA0, 6)
SPEC_FUN_REG_BIT(PC7, 0xA0, 7)

SPEC_FUN_REG(INT2CLR, 0xA1) // Interrupt 2 clear.
SPEC_FUN_REG(INT4CLR, 0xA2) // Interrupt 4 clear.

SPEC_FUN_REG(IE, 0xA8) // Interrupt enable.
SPEC_FUN_REG_BIT(EX0, 0xA8, 0)
SPEC_FUN_REG_BIT(ET0, 0xA8, 1)
SPEC_FUN_REG_BIT(EX1, 0xA8, 2)
SPEC_FUN_REG_BIT(ET1, 0xA8, 3)
SPEC_FUN_REG_BIT(ES0, 0xA8, 4)
SPEC_FUN_REG_BIT(ET2, 0xA8, 5)
SPEC_FUN_REG_BIT(ES1, 0xA8, 6)
SPEC_FUN_REG_BIT(EA, 0xA8, 7)

SPEC_FUN_REG(EP2468STAT, 0xAA) // Endpoint 2,4,6,8 status flags.
SPEC_FUN_REG(EP24FIFOFLGS, 0xAB) // Endpoint 2,4 slave FIFO status flags.
SPEC_FUN_REG(EP68FIFOFLGS, 0xAC) // Endpoint 6,8 slave FIFO status flags.
SPEC_FUN_REG(AUTOPTRSETUP, 0xAF) // Auto-pointer 1,2 setup.

SPEC_FUN_REG(IOD, 0xB0) // Port D.
SPEC_FUN_REG_BIT(PD0, 0xB0, 0)
SPEC_FUN_REG_BIT(PD1, 0xB0, 1)
SPEC_FUN_REG_BIT(PD2, 0xB0, 2)
SPEC_FUN_REG_BIT(PD3, 0xB0, 3)
SPEC_FUN_REG_BIT(PD4, 0xB0, 4)
SPEC_FUN_REG_BIT(PD5, 0xB0, 5)
SPEC_FUN_REG_BIT(PD6, 0xB0, 6)
SPEC_FUN_REG_BIT(PD7, 0xB0, 7)

SPEC_FUN_REG(IOE, 0xB1) // Port E.

SPEC_FUN_REG(OEA, 0xB2) // Port A out enable.
SPEC_FUN_REG(OEB, 0xB3) // Port B out enable.
SPEC_FUN_REG(OEC, 0xB4) // Port C out enable.
SPEC_FUN_REG(OED, 0xB5) // Port D out enable.
SPEC_FUN_REG(OEE, 0xB6) // Port E out enable.

SPEC_FUN_REG(IP, 0xB8) // Interrupt prority.
SPEC_FUN_REG_BIT(PX0, 0xB8, 0)
SPEC_FUN_REG_BIT(PT0, 0xB8, 1)
SPEC_FUN_REG_BIT(PX1, 0xB8, 2)
SPEC_FUN_REG_BIT(PT1, 0xB8, 3)
SPEC_FUN_REG_BIT(PS0, 0xB8, 4)
SPEC_FUN_REG_BIT(PT2, 0xB8, 5)
SPEC_FUN_REG_BIT(PS1, 0xB8, 6)

SPEC_FUN_REG(EP01STAT, 0xBA) // Endpoint 0,1 status.
SPEC_FUN_REG(GPIFTRIG, 0xBB) // Endpoint 2,4,6,8 GPIF slave FIFO trigger.

SPEC_FUN_REG(GPIFSGLDATH, 0xBD) // GPIF data H.
SPEC_FUN_REG(GPIFSGLDATLX, 0xBE) // GPIF data L with trigger.
SPEC_FUN_REG(GPIFSGLDATLNOX, 0xBF) // GPIF data L without trigger.

SPEC_FUN_REG(SCON1, 0xC0) // Serial port 1 control.
SPEC_FUN_REG_BIT(RI1, 0xC0, 0)
SPEC_FUN_REG_BIT(TI1, 0xC0, 1)
SPEC_FUN_REG_BIT(RB81, 0xC0, 2)
SPEC_FUN_REG_BIT(TB81, 0xC0, 3)
SPEC_FUN_REG_BIT(REN1, 0xC0, 4)
SPEC_FUN_REG_BIT(SM21, 0xC0, 5)
SPEC_FUN_REG_BIT(SM11, 0xC0, 6)
SPEC_FUN_REG_BIT(SM01, 0xC0, 7)

SPEC_FUN_REG(SBUF1, 0xC1) // Serial port 1 data buffer.

SPEC_FUN_REG(T2CON, 0xC8) // Timer/counter 2 control.
SPEC_FUN_REG_BIT(CP_RL2, 0xC8, 0)
SPEC_FUN_REG_BIT(C_T2, 0xC8, 1)
SPEC_FUN_REG_BIT(TR2, 0xC8, 2)
SPEC_FUN_REG_BIT(EXEN2, 0xC8, 3)
SPEC_FUN_REG_BIT(TCLK, 0xC8, 4)
SPEC_FUN_REG_BIT(RCLK, 0xC8, 5)
SPEC_FUN_REG_BIT(EXF2, 0xC8, 6)
SPEC_FUN_REG_BIT(TF2, 0xC8, 7)

SPEC_FUN_REG(RCAP2L, 0xCA) // Capture for timer 2 auto-reload up-counter L.
SPEC_FUN_REG(RCAP2H, 0xCB) // Capture for timer 2 auto-reload up-counter H.
SPEC_FUN_REG(TL2, 0xCC) // Timer 2 reload L.
SPEC_FUN_REG(TH2, 0xCD) // Timer 2 reload H.

SPEC_FUN_REG(PSW, 0xD0) // Program status word.
SPEC_FUN_REG_BIT(P, 0xD0, 0)
SPEC_FUN_REG_BIT(FL, 0xD0, 1)
SPEC_FUN_REG_BIT(OV, 0xD0, 2)
SPEC_FUN_REG_BIT(RS0, 0xD0, 3)
SPEC_FUN_REG_BIT(RS1, 0xD0, 4)
SPEC_FUN_REG_BIT(F0, 0xD0, 5)
SPEC_FUN_REG_BIT(AC, 0xD0, 6)
SPEC_FUN_REG_BIT(CY, 0xD0, 7)

SPEC_FUN_REG(EICON, 0xD8) // External interrupt control.
SPEC_FUN_REG_BIT(INT6, 0xD8, 3)
SPEC_FUN_REG_BIT(RESI, 0xD8, 4)
SPEC_FUN_REG_BIT(ERESI, 0xD8, 5)
SPEC_FUN_REG_BIT(SMOD1, 0xD8, 7)

SPEC_FUN_REG(ACC, 0xE0) // Accumulator.

SPEC_FUN_REG(EIE, 0xE8) // External interrupt enable.
SPEC_FUN_REG_BIT(EUSB, 0xE8, 0)
SPEC_FUN_REG_BIT(EI2C, 0xE8, 1)
SPEC_FUN_REG_BIT(EIEX4, 0xE8, 2)
SPEC_FUN_REG_BIT(EIEX5, 0xE8, 3)
SPEC_FUN_REG_BIT(EIEX6, 0xE8, 4)

SPEC_FUN_REG(B, 0xF0) // B

SPEC_FUN_REG(EIP, 0xF8) // External interrupt priority control.
SPEC_FUN_REG_BIT(PUSB, 0xF8, 0)
SPEC_FUN_REG_BIT(PI2C, 0xF8, 1)
SPEC_FUN_REG_BIT(EIPX4, 0xF8, 2)
SPEC_FUN_REG_BIT(EIPX5, 0xF8, 3)
SPEC_FUN_REG_BIT(EIPX6, 0xF8, 4)

//
// Register bit masks.
//

// CPU control & status register (CPUCS).
enum cpucs_bits {
    bmPRTCSTB = bmBIT5,
    bmCLKSPD = bmBIT4 | bmBIT3,
    bmCLKSPD1 = bmBIT4,
    bmCLKSPD0 = bmBIT3,
    bmCLKINV = bmBIT2,
    bmCLKOE = bmBIT1,
    bm8051RES = bmBIT0
};

// Port A (PORTACFG).
enum portacfg_bits {
    bmFLAGD = bmBIT7,
    bmINT1 = bmBIT1,
    bmINT0 = bmBIT0
};

// Port C (PORTCCFG).
enum portccfg_bits {
    bmGPIFA7 = bmBIT7,
    bmGPIFA6 = bmBIT6,
    bmGPIFA5 = bmBIT5,
    bmGPIFA4 = bmBIT4,
    bmGPIFA3 = bmBIT3,
    bmGPIFA2 = bmBIT2,
    bmGPIFA1 = bmBIT1,
    bmGPIFA0 = bmBIT0
};

// Port E (PORTECFG).
enum portecfg_bits {
    bmGPIFA8 = bmBIT7,
    bmT2EX = bmBIT6,
    bmINT6 = bmBIT5,
    bmRXD1OUT = bmBIT4,
    bmRXD0OUT = bmBIT3,
    bmT2OUT = bmBIT2,
    bmT1OUT = bmBIT1,
    bmT0OUT = bmBIT0
};

// I2C control & status register (I2CS).
enum i2cs_bits {
    bmSTART = bmBIT7,
    bmSTOP = bmBIT6,
    bmLASTRD = bmBIT5,
    bmID = bmBIT4 | bmBIT3,
    bmBERR = bmBIT2,
    bmACK = bmBIT1,
    bmDONE = bmBIT0
};

// I2C control register (I2CTL).
enum i2ctl_bits {
    bmSTOPIE = bmBIT1,
    bm400KHZ = bmBIT0
};

// Interrupt 2 (USB) autovector register (INT2IVEC).
enum int2ivec_bits {
    bmIV4 = bmBIT6,
    bmIV3 = bmBIT5,
    bmIV2 = bmBIT4,
    bmIV1 = bmBIT3,
    bmIV0 = bmBIT2
};

// USB interrupt request & enable registers (USBIE/USBIRQ).
enum usbieirq_bits {
    bmEP0ACK = bmBIT6,
    bmHSGRANT = bmBIT5,
    bmURES = bmBIT4,
    bmSUSP = bmBIT3,
    bmSUTOK = bmBIT2,
    bmSOF = bmBIT1,
    bmSUDAV = bmBIT0
};

// End point interrupt request & enable registers (EPIE/EPIRQ).
enum epieirq_bits {
    bmEP8 = bmBIT7,
    bmEP6 = bmBIT6,
    bmEP4 = bmBIT5,
    bmEP2 = bmBIT4,
    bmEP1OUT = bmBIT3,
    bmEP1IN  = bmBIT2,
    bmEP0OUT = bmBIT1,
    bmEP0IN  = bmBIT0
};

// Breakpoint register (BREAKPT).
enum breakpt_bits {
    bmBREAK = bmBIT3,
    bmBPPULSE = bmBIT2,
    bmBPEN = bmBIT1
};

// Interrupt 2 & 4 setup (INTSETUP).
enum intsetup_bits {
    bmAV2EN = bmBIT3,
    bmINT4SRC = bmBIT1,
    bmAV4EN = bmBIT0
};

// USB control & status register (USBCS).
enum usbcs_bits {
    bmHSM = bmBIT7,
    bmDISCON = bmBIT3,
    bmNOSYNSOF = bmBIT2,
    bmRENUM = bmBIT1,
    bmSIGRESUME = bmBIT0
};

// Wakeup control and status register (WAKEUPCS).
enum wakeupcs_bits {
    bmWU2 = bmBIT7,
    bmWU = bmBIT6,
    bmWU2POL = bmBIT5,
    bmWUPOL = bmBIT4,
    bmDPEN = bmBIT2,
    bmWU2EN = bmBIT1,
    bmWUEN = bmBIT0
};

// End point 0 control & status register (EP0CS).
enum ep0cs_bits {
    bmHSNAK = bmBIT7
};

// End point 0-1 control & status registers
// (EP0CS/EP1OUTCS/EP1INCS).
enum ep01cs_bits {
    bmEPBUSY = bmBIT1,
    bmEPSTALL = bmBIT0
};

// End point 2-8 Control & status registers
// (EP2CS/EP4CS/EP6CS/EP8CS).
enum ep2468cs_bits {
    MSK_EP2468CS_NPAK = bmBIT6 | bmBIT5 | bmBIT4,
    MSK_EP2468CS_EPFULL = bmBIT3,
    MSK_EP2468CS_EPEMPTY = bmBIT2
};

// End point status SFR bits (EP2468STAT).
enum ep2468stat_bits {
    bmEP8FULL = bmBIT7,
    bmEP8EMPTY = bmBIT6,
    bmEP6FULL = bmBIT5,
    bmEP6EMPTY = bmBIT4,
    bmEP4FULL = bmBIT3,
    bmEP4EMPTY = bmBIT2,
    bmEP2FULL = bmBIT1,
    bmEP2EMPTY = bmBIT0
};

// End point 0,1 status bits (EP01STAT).
enum ep01stat_bits {
    bmEP1INBSY = bmBIT2,
    bmEP1OUTBSY = bmBIT1,
    bmEP0BSY = bmBIT0
};

// Setup data pointer auto mode (SUDPTRCTL).
enum sudptrctl_bits {
    bmSDPAUTO = bmBIT0
};

// End point data toggle control (TOGCTL).
enum togctl_bits {
    bmQUERYTOGGLE = bmBIT7,
    bmSETTOGGLE = bmBIT6,
    bmRESETTOGGLE = bmBIT5,
    bmTOGCTLEPMASK = bmBIT3 | bmBIT2 | bmBIT1 | bmBIT0
};

// In bulk NAK enable and request bits (IBNIE/IBNIRQ).
enum ibnieirq_bits {
    bmEP8IBN = bmBIT5,
    bmEP6IBN = bmBIT4,
    bmEP4IBN = bmBIT3,
    bmEP2IBN = bmBIT2,
    bmEP1IBN = bmBIT1,
    bmEP0IBN = bmBIT0
};

// Ping NAK enable and request bits (NAKIE/NAKIRQ).
enum nakieirq_bits {
    bmEP8PING = bmBIT7,
    bmEP6PING = bmBIT6,
    bmEP4PING = bmBIT5,
    bmEP2PING = bmBIT4,
    bmEP1PING = bmBIT3,
    bmEP0PING = bmBIT2,
    bmIBN = bmBIT0
};

// Interface configuration bits (IFCONFIG).
enum ifconfig_bits {
    bmIFCLKSRC = bmBIT7,
    bm3048MHZ = bmBIT6,
    bmIFCLKOE = bmBIT5,
    bmIFCLKPOL = bmBIT4,
    bmASYNC = bmBIT3,
    bmGSTATE = bmBIT2,
    bmIFCFG = bmBIT0 | bmBIT1
};

// IFCFG field of IFCONFIG register.
enum ifcfg_bits {
    bmIFPORTS = 0,
    bmIFRESERVED = bmBIT0,
    bmIFGPIF = bmBIT1,
    bmIFSLAVEFIFO = bmBIT0 | bmBIT1
};

// End point 2,4,6,8 FIFO configuration bits
// (EP2FIFOCFG,EP4FIFOCFG,EP6FIFOCFG,EP8FIFOCFG).
enum ep2468fifocfg_bits {
    bmINFM = bmBIT6,
    bmOEP = bmBIT5,
    bmAUTOOUT = bmBIT4,
    bmAUTOIN = bmBIT3,
    bmZEROLENIN = bmBIT2,
    bmWORDWIDE = bmBIT0
};

// Chip revision control bits (REVCTL).
enum revctl_bits {
    bmNOAUTOARM = bmBIT1,
    bmSKIPCOMMIT = bmBIT0
};

// Fifo reset bits (FIFORESET).
enum fiforeset_bits {
    bmNAKALL = bmBIT7,
    bmFREP3 = bmBIT3,
    bmFREP2 = bmBIT2,
    bmFREP1 = bmBIT1,
    bmFREP0 = bmBIT0
};

// Chip feature register (GPCR2).
enum gpcr2_bits {
    bmFULLSPEEDONLY = bmBIT4
};

// Clock control register (CKCON).
// Note: a RevE errata states that stretch must=0
// to set OUTxBC.
enum ckcon_bits {
    bmT2M = bmBIT5,
    bmT1M = bmBIT4,
    bmT0M = bmBIT3,
    bmSTRETCH = bmBIT0 | bmBIT1 | bmBIT2
};

enum data_mem_stretch {
    bmFW_STRETCH1 = 0, // Movx: 2 cycles, R/W strobe: 2 cycles...
    bmFW_STRETCH2 = 1, // Movx: 3 cycles, R/W strobe: 4 cycles...
    bmFW_STRETCH3 = 2, // Movx: 4 cycles, R/W strobe: 8 cycles...
    bmFW_STRETCH4 = 3, // Movx: 5 cycles, R/W strobe: 12 cycles...
    bmFW_STRETCH5 = 4, // Movx: 6 cycles, R/W strobe: 16 cycles...
    bmFW_STRETCH6 = 5, // Movx: 7 cycles, R/W strobe: 20 cycles...
    bmFW_STRETCH7 = 6, // Movx: 8 cycles, R/W strobe: 24 cycles...
    bmFW_STRETCH8 = 7 // Movx: 9 cycles, R/W strobe: 28 cycles...
};

// External interrupt flags (EXIF).
enum exif_bits {
    bmIE5 = bmBIT7,
    bmIE4 = bmBIT6,
    bmI2CINT = bmBIT5,
    bmUSBNT = bmBIT4
};

// External interrupt control (EICON).
enum eicon_bits {
    bmSMOD1 = bmBIT7,
    bmERESI = bmBIT5,
    bmRESI = bmBIT4,
    //bmINT6 = bmBIT3
};

// Power control (PCON).
enum pcon_bits {
    bmSMOD0 = bmBIT7,
    bmIDLE = bmBIT0
};

// Autopointer 1 & 2 setup (AUTOPTRSETUP).
enum autoptrsetup_bits {
    bmAPTR2INC = bmBIT2,
    bmAPTR1INC = bmBIT1,
    bmAPTREN = bmBIT0
};

#ifdef __cplusplus
}
#endif

#endif // FX2_REGS_H
