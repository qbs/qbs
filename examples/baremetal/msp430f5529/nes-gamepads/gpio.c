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

#include "hwdefs.h"
#include "gpio.h"

enum { INVALID_ADDRESS = 0xFFFF };

static uint16_t gpio_port_address_get(enum gpio_port port)
{
    switch (port) {
    case GPIO_PORT_P1:
        return P1_BASE;
    case GPIO_PORT_P2:
        return P2_BASE;
    case GPIO_PORT_P3:
        return P3_BASE;
    case GPIO_PORT_P4:
        return P4_BASE;
    case GPIO_PORT_P5:
        return P5_BASE;
    case GPIO_PORT_P6:
        return P6_BASE;
    case GPIO_PORT_P7:
        return P7_BASE;
    case GPIO_PORT_P8:
        return P8_BASE;
    case GPIO_PORT_PJ:
        return PJ_BASE;
    default:
        break;
    }
    return INVALID_ADDRESS;
}

static uint16_t gpio_pins_adjust(enum gpio_port port, uint16_t pins)
{
    if ((port & 1) ^ 1)
        pins <<= 8;
    return pins;
}

void gpio_pins_set_as_out(enum gpio_port port, uint16_t pins)
{
    const uint16_t base_address = gpio_port_address_get(port);
    if (base_address == INVALID_ADDRESS)
        return;
    pins = gpio_pins_adjust(port, pins);
    HWREG16(base_address + OFS_PASEL) &= ~pins;
    HWREG16(base_address + OFS_PADIR) |= pins;
}

void gpio_pins_set_as_in(enum gpio_port port, uint16_t pins)
{
    const uint16_t base_address = gpio_port_address_get(port);
    if (base_address == INVALID_ADDRESS)
        return;
    pins = gpio_pins_adjust(port, pins);
    HWREG16(base_address + OFS_PASEL) &= ~pins;
    HWREG16(base_address + OFS_PADIR) &= ~pins;
    HWREG16(base_address + OFS_PAREN) &= ~pins;
}

void gpio_pins_set_as_pf_out(enum gpio_port port, uint16_t pins)
{
    const uint16_t base_address = gpio_port_address_get(port);
    if (base_address == INVALID_ADDRESS)
        return;
    pins = gpio_pins_adjust(port, pins);
    HWREG16(base_address + OFS_PADIR) |= pins;
    HWREG16(base_address + OFS_PASEL) |= pins;
}

void gpio_pins_set_as_pf_in(enum gpio_port port, uint16_t pins)
{
    const uint16_t base_address = gpio_port_address_get(port);
    if (base_address == INVALID_ADDRESS)
        return;
    pins = gpio_pins_adjust(port, pins);
    HWREG16(base_address + OFS_PADIR) &= ~pins;
    HWREG16(base_address + OFS_PASEL) |= pins;
}

void gpio_pins_set_high(enum gpio_port port,
                        uint16_t pins)
{
    const uint16_t base_address = gpio_port_address_get(port);
    if (base_address == INVALID_ADDRESS)
        return;
    pins = gpio_pins_adjust(port, pins);
    HWREG16(base_address + OFS_PAOUT) |= pins;
}

void gpio_pins_set_low(enum gpio_port port, uint16_t pins)
{
    const uint16_t base_address = gpio_port_address_get(port);
    if (base_address == INVALID_ADDRESS)
        return;
    pins = gpio_pins_adjust(port, pins);
    HWREG16(base_address + OFS_PAOUT) &= ~pins;
}

void gpio_pins_toggle(enum gpio_port port, uint16_t pins)
{
    const uint16_t base_address = gpio_port_address_get(port);
    if (base_address == INVALID_ADDRESS)
        return;
    pins = gpio_pins_adjust(port, pins);
    HWREG16(base_address + OFS_PAOUT) ^= pins;
}

enum gpio_pin_status gpio_pin_get(enum gpio_port port, uint16_t pins)
{
    const uint16_t base_address = gpio_port_address_get(port);
    if (base_address == INVALID_ADDRESS)
        return GPIO_INPUT_PIN_LOW;
    pins = gpio_pins_adjust(port, pins);
    const uint16_t value = HWREG16(base_address + OFS_PAIN) & pins;
    return (value > 0) ? GPIO_INPUT_PIN_HIGH : GPIO_INPUT_PIN_LOW;
}
