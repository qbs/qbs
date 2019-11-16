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

#ifndef FX2_DEFS_H
#define FX2_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

enum bit_mask {
    bmBIT0 = 0x01,
    bmBIT1 = 0x02,
    bmBIT2 = 0x04,
    bmBIT3 = 0x08,
    bmBIT4 = 0x10,
    bmBIT5 = 0x20,
    bmBIT6 = 0x40,
    bmBIT7 = 0x80
};

enum boolean {
    FALSE = 0,
    TRUE
};

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned char BOOL;

#ifndef NULL
#define NULL (void *)0
#endif

#if defined(__ICC8051__)

#include <intrinsics.h>

# define NOP() __no_operation()

# define XDATA __xdata
# define CODE __code
# define AT __at

# define SFR __sfr
# define SBIT __bit

# define XDATA_REG(reg_name, reg_addr) \
    XDATA __no_init volatile BYTE reg_name @ reg_addr;

# define SPEC_FUN_REG(reg_name, reg_addr) \
    SFR __no_init volatile BYTE reg_name @ reg_addr;

# define SPEC_FUN_REG_BIT(bit_name, reg_addr, bit_num) \
    SBIT __no_init volatile _Bool bit_name @ (reg_addr+bit_num);

# define _PPTOSTR_(x) #x
# define _PPARAM_(address) _PPTOSTR_(vector=address * 8 + 3)
# define INTERRUPT(isr_name, vector) \
    _Pragma(_PPARAM_(vector)) __interrupt void isr_name(void)

#elif defined (__C51__)

# include <intrins.h>

# define NOP() _nop_()

# define XDATA xdata
# define CODE code
# define AT _at_

# define SFR sfr
# define SBIT sbit

# if defined(DEFINE_REGS)
#  define XDATA_REG(reg_name, reg_addr) \
    XDATA volatile BYTE reg_name AT reg_addr;
# else
#  define XDATA_REG(reg_name, reg_addr) \
    extern XDATA volatile BYTE reg_name;
# endif

# define SPEC_FUN_REG(reg_name, reg_addr) \
    SFR reg_name = reg_addr;

# define SPEC_FUN_REG_BIT(bit_name, reg_addr, bit_num) \
    sbit bit_name = reg_addr + bit_num;

# define INTERRUPT(isr_name, num) \
    void isr_name (void) interrupt num

#elif defined (__SDCC_mcs51)

# define NOP() __asm nop __endasm

# define XDATA __xdata
# define CODE __code
# define AT __at

# define SFR __sfr
# define SBIT __sbit

# define XDATA_REG(reg_name, reg_addr) \
    XDATA AT reg_addr volatile BYTE reg_name;

# define SPEC_FUN_REG(reg_name, reg_addr) \
    SFR AT reg_addr reg_name;

# define SPEC_FUN_REG_BIT(bit_name, reg_addr, bit_num) \
    SBIT AT reg_addr + bit_num bit_name;

# define INTERRUPT(isr_name, num) \
    void isr_name (void) __interrupt num

#else

#error "Unsupported toolchain"

#endif

#ifdef __cplusplus
}
#endif

#endif // FX2_DEFS_H
