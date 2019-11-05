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

#ifndef MSP430_UCS_H
#define MSP430_UCS_H

#ifdef __cplusplus
extern "C" {
#endif

enum ucs_signal {
    UCS_ACLK = 0x01,
    UCS_MCLK = 0x02,
    UCS_SMCLK = 0x04,
    UCS_FLLREF = 0x08
};

enum ucs_source {
    UCS_XT1CLK_SELECT,
    UCS_VLOCLK_SELECT,
    UCS_REFOCLK_SELECT,
    UCS_DCOCLK_SELECT,
    UCS_DCOCLKDIV_SELECT,
    UCS_XT2CLK_SELECT
};

enum ucs_divider {
    UCS_CLOCK_DIVIDER_1,
    UCS_CLOCK_DIVIDER_2,
    UCS_CLOCK_DIVIDER_4,
    UCS_CLOCK_DIVIDER_8,
    UCS_CLOCK_DIVIDER_12,
    UCS_CLOCK_DIVIDER_16,
    UCS_CLOCK_DIVIDER_32
};

enum ucs_fault_flag {
    UCS_XT2OFFG,
    UCS_XT1HFOFFG,
    UCS_XT1LFOFFG,
    UCS_DCOFFG
};

void ucs_clocks_init(void);

void ucs_init(enum ucs_signal signal,
              enum ucs_source source,
              enum ucs_divider divider);

bool ucs_xt2_blocking_turn_on(uint16_t drive, uint16_t timeout);

void ucs_xt2_turn_off(void);
void ucs_fll_settle_init(uint16_t fsystem, uint16_t ratio);

#ifdef __cplusplus
}
#endif

#endif // MSP430_UCS_H
