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

#ifndef MSP430_GPIO_H
#define MSP430_GPIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum gpio_port {
    GPIO_PORT_P1 = 1,
    GPIO_PORT_P2 = 2,
    GPIO_PORT_P3 = 3,
    GPIO_PORT_P4 = 4,
    GPIO_PORT_P5 = 5,
    GPIO_PORT_P6 = 6,
    GPIO_PORT_P7 = 7,
    GPIO_PORT_P8 = 8,
    GPIO_PORT_PJ = 13,
};

enum gpio_pin {
    GPIO_PIN0 = 0x0001,
    GPIO_PIN1 = 0x0002,
    GPIO_PIN2 = 0x0004,
    GPIO_PIN3 = 0x0008,
    GPIO_PIN4 = 0x0010,
    GPIO_PIN5 = 0x0020,
    GPIO_PIN6 = 0x0040,
    GPIO_PIN7 = 0x0080,
    GPIO_PIN8 = 0x0100,
    GPIO_PIN9 = 0x0200,
    GPIO_PIN10 = 0x0400,
    GPIO_PIN11 = 0x0800,
    GPIO_PIN12 = 0x1000,
    GPIO_PIN13 = 0x2000,
    GPIO_PIN14 = 0x4000,
    GPIO_PIN15 = 0x8000,
};

enum gpio_pin_status {
    GPIO_INPUT_PIN_HIGH = 0x01,
    GPIO_INPUT_PIN_LOW = 0x00
};

void gpio_pins_set_as_out(enum gpio_port port, uint16_t pins);
void gpio_pins_set_as_in(enum gpio_port port, uint16_t pins);
void gpio_pins_set_as_pf_out(enum gpio_port port, uint16_t pins);
void gpio_pins_set_as_pf_in(enum gpio_port port, uint16_t pins);
void gpio_pins_set_high(enum gpio_port port, uint16_t pins);
void gpio_pins_set_low(enum gpio_port port, uint16_t pins);
void gpio_pins_toggle(enum gpio_port port, uint16_t pins);

enum gpio_pin_status gpio_pin_get(enum gpio_port port, uint16_t pins);

#ifdef __cplusplus
}
#endif

#endif // MSP430_GPIO_H
