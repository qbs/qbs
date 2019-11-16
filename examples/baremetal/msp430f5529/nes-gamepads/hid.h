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

#ifndef MSP430_HID_H
#define MSP430_HID_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hid_constants {
    HID_CONFIG_NUMBER = 1, // Number of valid configuration.
    HID_IFACE_NUMBER = 0, // Number of valid interface.
    HID_ALT_IFACE_NUMBER = 0, // Number of valid alternate interface.
    HID_EP_IN = 0x81 // Active end point address.
};

enum hid_gamepad_id {
    HID_REPORT_ID_GAMEPAD1 = 1,
    HID_REPORT_ID_GAMEPAD2 = 2
};

enum {
    HID_REPORTS_COUNT = 2,
    HID_REPORT_BITS_COUNT = 8
};

void hid_ep0_init(void);

// Called only in interrupt context.
void hid_ep0_setup_handler(void);
void hid_ep0_in_handler(void);

void hid_ep0_in_nak(void);
void hid_ep0_in_stall(void);
void hid_ep0_in_clear(void);
bool hid_ep0_in_is_stalled(void);

void hid_ep0_out_nak(void);
void hid_ep0_out_stall(void);
void hid_ep0_out_clear(void);

const uint8_t *hid_ep0_desc_get(uint8_t type, uint16_t *length);

uint8_t hid_ep0_setup_bm_request_type_get(void);
uint8_t hid_ep0_setup_request_get(void);
uint8_t hid_ep0_setup_lvalue_get(void);
uint8_t hid_ep0_setup_hvalue_get(void);
uint8_t hid_ep0_setup_lindex_get(void);
uint8_t hid_ep0_setup_hindex_get(void);

void hid_ep0_enumerated_set(bool enumerated);
bool hid_ep0_is_enumerated(void);

void hid_ep1_init(void);
void hid_ep1_task(void);

void hid_ep1_in_stall(void);
void hid_ep1_in_unstall(void);
bool hid_ep1_in_is_stalled(void);

#ifdef __cplusplus
}
#endif

#endif // MSP430_HID_H
