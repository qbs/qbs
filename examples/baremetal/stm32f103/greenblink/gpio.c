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

#include "gpio.h"
#include "system.h"

#define GPIO_GREEN_LED_PIN_POS      (13u)
#define GPIO_GREEN_LED_PIN          (1u << GPIO_GREEN_LED_PIN_POS)

// Bit definition for RCC_APB2ENR register.
#define RCC_APB2ENR_IOPCEN_POS      (4u)
#define RCC_APB2ENR_IOPCEN          (0x1u << RCC_APB2ENR_IOPCEN_POS)

// Bit definition for GPIO_CRL register.
#define GPIO_CRL_MODE_POS           (0u) // MODE field position.
#define GPIO_CRL_MODE_MSK           (0x3u << GPIO_CRL_MODE_POS) // MODE field mask.
#define GPIO_CRL_MODE_OUT_FREQ_LOW  (0x2u << GPIO_CRL_MODE_POS) // As output with low frequency.

#define GPIO_CRL_CNF_POS            (2u) // CNF field position.
#define GPIO_CRL_CNF_MSK            (0x3u << GPIO_CRL_CNF_POS) // CNF field mask.
#define GPIO_CRL_CNF_OUTPUT_PP      (0x00000000u) // General purpose output push-pull.

void gpio_init_green_led(void)
{
    // Enable RCC clock on GPIOC port.
    RCC_REGS_MAP->APB2ENR |= RCC_APB2ENR_IOPCEN;
    // Configure GPIOC pin #13.
    const uint32_t offset = ((GPIO_GREEN_LED_PIN_POS - 8u) << 2u);
    GPIOC_REGS_MAP->CRH &= ~((GPIO_CRL_MODE_MSK | GPIO_CRL_CNF_MSK) << offset);
    GPIOC_REGS_MAP->CRH |= ((GPIO_CRL_MODE_OUT_FREQ_LOW | GPIO_CRL_CNF_OUTPUT_PP) << offset);
}

void gpio_toggle_green_led(void)
{
    GPIOC_REGS_MAP->ODR ^= GPIO_GREEN_LED_PIN;
}
