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

#include "gpio.h"
#include "hid.h"
#include "hwdefs.h"
#include "ucs.h"
#include "usb.h"

static uint16_t usb_khz_freq_get(void)
{
    uint16_t freq = 0;
    const uint8_t selm_curr = (UCSCTL4_L & SELM_7);
    if (selm_curr <= 4) {
        uint16_t fll_ref_freq = 33; // It is 32.768 kHz.
        if ((UCSCTL3_L & SELREF_7) >= 0x50)
            fll_ref_freq = (4.0) * 1000;

        uint16_t flln_curr = (UCSCTL2 & 0x03FF) + 1;
        if (selm_curr == SELM_3) {
            uint16_t flld_curr = (UCSCTL2 & FLLD_7);
            flld_curr >>= 12;
            flln_curr <<= flld_curr;
        }

        const uint8_t fll_ref_div = (UCSCTL3_L & FLLREFDIV_7);
        if (fll_ref_div == 0)
            freq = flln_curr * (fll_ref_freq / 1);
        else if (fll_ref_div == 1)
            freq = flln_curr * (fll_ref_freq / 2);
        else if (fll_ref_div == 2)
            freq = flln_curr * (fll_ref_freq / 4);
        else if (fll_ref_div == 3)
            freq = flln_curr * (fll_ref_freq / 8);
        else if (fll_ref_div == 4)
            freq = flln_curr * (fll_ref_freq / 12);
        else if (fll_ref_div == 5)
            freq = flln_curr * (fll_ref_freq / 16);
    } else {
        freq = (4.0) * 1000;
    }
    return freq >> (UCSCTL5_L & DIVM_7);
}

static uint16_t usb_delay_250us_get(void)
{
    const uint16_t mclk_freq = usb_khz_freq_get();
    const uint16_t delay_250us = ((mclk_freq >> 6) | (mclk_freq >> 7)
                                  | (mclk_freq >> 9));
    return delay_250us;
}

static void usb_cfg_access_allow(bool allow)
{
    enum { ALLOW_KEY = 0x9628, DENIED_KEY = 0x9600 };
    USBKEYPID = allow ? ALLOW_KEY : DENIED_KEY;
}

static bool usb_pll_enable(void)
{
    if (!(USBPWRCTL & USBBGVBV))
        return false;

    if ((USBCNF & USB_EN) && (USBPLLCTL & UPLLEN))
        return true;

    gpio_pins_set_as_pf_out(GPIO_PORT_P5, GPIO_PIN2);
    gpio_pins_set_as_pf_out(GPIO_PORT_P5, GPIO_PIN3);

    usb_cfg_access_allow(true);

    if (!ucs_xt2_blocking_turn_on(XT2DRIVE_0, 50000))
        return false;

    USBPLLDIVB = USBPLL_SETCLK_4_0;
    USBPLLCTL = UPFDEN | UPLLEN;

    const uint16_t delay_250us = usb_delay_250us_get();
    uint8_t j = 0;
    do {
        USBPLLIR = 0;
        for (uint8_t k = 0; k < 2; ++k) {
            for (uint16_t i = 0; i < delay_250us; ++i) {
                __no_operation();
            }
        }

        if (j++ > 10) {
            usb_cfg_access_allow(false);
            return false;
        }
    } while (USBPLLIR != 0);

    USBCNF |= USB_EN;
    usb_cfg_access_allow(false);
    return true;
}

static void usb_reset(void)
{
    usb_cfg_access_allow(true);

    hid_ep0_enumerated_set(false);

    USBCTL = 0;
    USBFUNADR = 0;
    USBOEPIE = 0;
    USBIEPIE = 0;

    hid_ep0_init();
    hid_ep1_init();

    USBCTL = FEN;
    USBIFG = 0;
    USBIE = SETUPIE | RSTRIE | SUSRIE;
    usb_cfg_access_allow(false);
}

static void usb_connect(void)
{
    usb_cfg_access_allow(true);
    USBCNF |= PUR_EN;
    USBPWRCTL |= VBOFFIE;
    usb_cfg_access_allow(false);
}

static void usb_pwr_vbus_wait(void)
{
    const uint16_t delay_250us = usb_delay_250us_get();
    for (uint8_t j = 0; j < 4; ++j) {
        for (uint16_t i = 0; i < delay_250us; ++i) {
            __no_operation();
        }
    }
}

static void usb_pwr_vbus_on_handler(void)
{
    usb_pwr_vbus_wait();

    if (USBPWRCTL & USBBGVBV) {
        usb_cfg_access_allow(true);
        USBPWRCTL |= VBOFFIE;
        USBPWRCTL &= ~ (VBONIFG + VBOFFIFG);
        usb_cfg_access_allow(false);
    }
}

static void usb_pwr_vbus_off_handler(void)
{
    usb_pwr_vbus_wait();

    if (!(USBPWRCTL & USBBGVBV)) {
        usb_cfg_access_allow(true);
        USBCNF = 0;
        USBPLLCTL &= ~UPLLEN;
        USBPWRCTL &= ~(VBOFFIE + VBOFFIFG + SLDOEN);
        usb_cfg_access_allow(false);
    }
}

void usb_suspend(void)
{
    usb_cfg_access_allow(true);
    USBCTL |= FRSTE;
    USBIFG &= ~SUSRIFG;
    USBPLLCTL &= ~UPLLEN;
    ucs_xt2_turn_off();
    USBIE = RESRIE;
    usb_cfg_access_allow(false);
}

void usb_resume(void)
{
    usb_pll_enable();
    USBIFG &= ~(RESRIFG | SUSRIFG);
    USBIE = SETUPIE | RSTRIE | SUSRIE;
}

void usb_init(void)
{
    const uint16_t gie_backup  = (__get_SR_register() & GIE);

    usb_cfg_access_allow(true);
    USBPHYCTL = PUSEL;
    USBPWRCTL = VUSBEN | SLDOAON;

    const uint16_t delay_250us = usb_delay_250us_get();
    for (uint8_t j = 0; j < 20; ++j) {
        for (uint16_t i = 0; i < delay_250us; ++i) {
            __no_operation();
        }
    }

    USBPWRCTL |= VBONIE;
    usb_cfg_access_allow(false);

    __bis_SR_register(gie_backup);

    if (USBPWRCTL & USBBGVBV) {
        if (usb_pll_enable()) {
            usb_reset();
            usb_connect();
        }
    }
}

void usb_task(void)
{
    hid_ep1_task();
}

INTERRUPT(usb_ubm_isr, USB_UBM_VECTOR)
{
    bool wake_up = false;

    if (USBIFG & SETUPIFG) {
        hid_ep0_setup_handler();
        USBIFG &= ~SETUPIFG;
    }

    switch (__even_in_range(USBVECINT & 0x3F, USBVECINT_OUTPUT_ENDPOINT7)) {
    case USBVECINT_PWR_DROP:
        __no_operation();
        break;
    case USBVECINT_PLL_RANGE:
        wake_up = true;
        break;
    case USBVECINT_PWR_VBUSOn:
        usb_pwr_vbus_on_handler();
        if (usb_pll_enable()) {
            usb_reset();
            usb_connect();
        }
        wake_up = true;
        break;
    case USBVECINT_PWR_VBUSOff:
        usb_pwr_vbus_off_handler();
        ucs_xt2_turn_off();
        wake_up = true;
        break;
    case USBVECINT_INPUT_ENDPOINT0:
        hid_ep0_in_handler();
        break;
    case USBVECINT_RSTR:
        usb_reset();
        wake_up = true;
        break;
    case USBVECINT_SUSR:
        usb_suspend();
        wake_up = true;
        break;
    case USBVECINT_RESR:
        usb_resume();
        wake_up = true;
        break;
    case USBVECINT_SETUP_PACKET_RECEIVED:
        hid_ep0_in_nak();
        hid_ep0_out_nak();
        hid_ep0_setup_handler();
        break;
    default:
        break;
    }

    if (wake_up) {
        __bic_SR_register_on_exit(LPM3_bits);
        __no_operation();
    }
}
