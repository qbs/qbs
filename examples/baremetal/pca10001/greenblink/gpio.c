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

#define GPIO_GREEN_LED_PIN_POS      (18u)
#define GPIO_GREEN_LED_PIN          (1u << GPIO_GREEN_LED_PIN_POS)

// Pin direction attributes.
#define GPIO_PIN_CNF_DIR_POS        (0u) // Position of DIR field.
#define GPIO_PIN_CNF_DIR_OUT        (1u) // Configure pin as an output pin.
#define GPIO_PIN_CNF_DIR_MSK        (GPIO_PIN_CNF_DIR_OUT << GPIO_PIN_CNF_DIR_POS)

// Pin input buffer attributes.
#define GPIO_PIN_CNF_INPUT_POS      (1u) // Position of INPUT field.
#define GPIO_PIN_CNF_INPUT_OFF      (1u) // Disconnect input buffer.
#define GPIO_PIN_CNF_INPUT_MSK      (GPIO_PIN_CNF_INPUT_OFF << GPIO_PIN_CNF_INPUT_POS)

// Pin pull attributes.
#define GPIO_PIN_CNF_PULL_POS       (2u) // Position of PULL field.
#define GPIO_PIN_CNF_PULL_OFF       (0u) // No pull.
#define GPIO_PIN_CNF_PULL_MSK       (GPIO_PIN_CNF_PULL_OFF << GPIO_PIN_CNF_PULL_POS)

// Pin drive attributes.
#define GPIO_PIN_CNF_DRIVE_POS      (8u) // Position of DRIVE field.
#define GPIO_PIN_CNF_DRIVE_S0S1     (0u) // Standard '0', standard '1'.
#define GPIO_PIN_CNF_DRIVE_MSK      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_POS)

// Pin sense attributes.
#define GPIO_PIN_CNF_SENSE_POS      (16u) // Position of SENSE field.
#define GPIO_PIN_CNF_SENSE_OFF      (0u) // Disabled.
#define GPIO_PIN_CNF_SENSE_MSK      (GPIO_PIN_CNF_SENSE_OFF << GPIO_PIN_CNF_SENSE_POS)

void gpio_init_green_led(void)
{
    GPIOD_REGS_MAP->PIN_CNF[GPIO_GREEN_LED_PIN_POS] = (GPIO_PIN_CNF_DIR_MSK
                                                       | GPIO_PIN_CNF_INPUT_MSK
                                                       | GPIO_PIN_CNF_PULL_MSK
                                                       | GPIO_PIN_CNF_DRIVE_MSK
                                                       | GPIO_PIN_CNF_SENSE_MSK);
}

void gpio_toggle_green_led(void)
{
    const uint32_t gpio_state = GPIOD_REGS_MAP->OUT;
    GPIOD_REGS_MAP->OUTSET = (GPIO_GREEN_LED_PIN & ~gpio_state);
    GPIOD_REGS_MAP->OUTCLR = (GPIO_GREEN_LED_PIN & gpio_state);
}
