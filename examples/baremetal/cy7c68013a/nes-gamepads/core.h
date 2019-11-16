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

#ifndef FX2_HW_H
#define FX2_HW_H

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Allowed range: 5000 - 48000.
#ifndef CORE_IFREQ
#  define CORE_IFREQ 48000 // IFCLK frequency in kHz.
#endif

// Allowed values: 48000, 24000, or 12000.
#ifndef CORE_CFREQ
#  define CORE_CFREQ 48000 // CLKOUT frequency in kHz.
#endif

#if (CORE_IFREQ < 5000)
#  error "CORE_IFREQ too small! Valid range: 5000 - 48000."
#endif

#if (CORE_IFREQ > 48000)
#  error "CORE_IFREQ too large! Valid range: 5000 - 48000."
#endif

#if (CORE_CFREQ != 48000)
#  if (CORE_CFREQ != 24000)
#    if (CORE_CFREQ != 12000)
#      error "CORE_CFREQ invalid! Valid values: 48000, 24000, 12000."
#    endif
#  endif
#endif

// Calculate synchronization delay.
#define CORE_SCYCL (3 * (CORE_CFREQ) + 5 * (CORE_IFREQ) - 1) / (2 * (CORE_IFREQ))

#if (CORE_SCYCL == 1)
#  define sync_delay() {\
    NOP(); }
#endif

#if (CORE_SCYCL == 2)
#  define sync_delay() {\
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 3)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 4)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 5)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 6)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 7)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 8)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 9)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 10)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 11)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 12)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 13)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 14)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 15)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#if (CORE_SCYCL == 16)
#  define sync_delay() {\
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); \
    NOP(); }
#endif

#define code_all_irq_disable() (IE = 0)
#define code_all_irq_enable() (IE = 1)

void core_init(void);

#ifdef __cplusplus
}
#endif

#endif // FX2_HW_H
