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

#include "core.h"
#include "gpio.h"
#include "hid.h"
#include "regs.h"

static WORD g_counter = 0;

// We use this empirical value to make a GPIO polling
// every ~10 msec (rough). It is a simplest way, because
// instead we need to use a timers with interrupts callbacks.
enum { POLLING_COUNTER = 580 };

// Pins on the port A.
enum gpio_pins {
    GPIO_CLK_PIN = bmBIT0,
    GPIO_DATA1_PIN = bmBIT2,
    GPIO_DATA2_PIN = bmBIT4,
    GPIO_LATCH_PIN = bmBIT6,
};

static struct {
    const BYTE pin;
    BYTE buttons;
} g_reports[HID_REPORTS_COUNT] = {
    {GPIO_DATA1_PIN, 0},
    {GPIO_DATA2_PIN, 0}
};

// Pulse width around ~1 usec.
static void gpio_latch_pulse(void)
{
    IOA |= GPIO_LATCH_PIN;
    sync_delay();
    sync_delay();
    sync_delay();
    IOA &= ~GPIO_LATCH_PIN;
    sync_delay();
}

// Pulse width around ~1 usec.
static void gpio_clk_pulse(void)
{
    IOA |= GPIO_CLK_PIN;
    sync_delay();
    sync_delay();
    sync_delay();
    IOA &= ~GPIO_CLK_PIN;
    sync_delay();
}

static void gpio_reports_clean(void)
{
    BYTE index = 0;
    for (index = 0; index < HID_REPORTS_COUNT; ++index)
        g_reports[index].buttons = 0;
}

static void gpio_reports_send(void)
{
    BYTE index = 0;
    for (index = 0; index < HID_REPORTS_COUNT; ++index)
        hid_ep1_report_update(index, g_reports[index].buttons);
}

static void gpio_poll(void)
{
    BYTE pos = 0;

    // Cleanup reports.
    gpio_reports_clean();

    // Send latch pulse.
    gpio_latch_pulse();

    for (pos = 0; pos < HID_REPORT_BITS_COUNT; ++pos) {
        // TODO: Add some nops here?

        BYTE index = 0;
        for (index = 0; index < HID_REPORTS_COUNT; ++index) {
            // Low state means that a button is pressed.
            const BOOL v = (IOA & g_reports[index].pin) ? 0 : 1;
            g_reports[index].buttons |= (v << pos);
        }

        // Send clock pulse.
        gpio_clk_pulse();
    }

    gpio_reports_send();
}

void gpio_init(void)
{
    // Set interface to ports.
    IFCONFIG = (IFCONFIG & (~bmIFCFG)) | bmIFPORTS;
    sync_delay();

    // Initialize the CLK and LATCH pins as output.
    OEA = GPIO_CLK_PIN | GPIO_LATCH_PIN;
    sync_delay();
}

void gpio_task(void)
{
    if (g_counter < POLLING_COUNTER) {
        ++g_counter;
    } else {
        g_counter = 0;
        gpio_poll();
    }
}
