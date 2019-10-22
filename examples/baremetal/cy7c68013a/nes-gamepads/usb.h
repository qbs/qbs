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

#ifndef FX2_USB_H
#define FX2_USB_H

#include "regs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum usb_setup_bmreq_bits {
    bmSETUP_DIR = bmBIT7,
    bmSETUP_TYPE = bmBIT5 | bmBIT6,
    bmSETUP_RECIPIENT = bmBIT0 | bmBIT1 | bmBIT2 | bmBIT3 | bmBIT4
};

// Setup request direction.
enum usb_setup_req_direction_bits {
    bmSETUP_TO_DEVICE = 0, // From host to sevice direction.
    bmSETUP_TO_HOST = bmBIT7 // From device to host direction.
};

// Setup request type.
enum usb_setup_req_type_bits {
    bmSETUP_STANDARD = 0, // Standard request.
    bmSETUP_CLASS = bmBIT5, // Class request.
    bmSETUP_VENDOR = bmBIT6, // Vendor request.
};

// Setup request recipient.
enum usb_setup_req_recipient_bits {
    bmSETUP_DEVICE = 0, // Device recipient.
    bmSETUP_IFACE = bmBIT0, // Interface recipient.
    bmSETUP_EP = bmBIT1, // End point recipient.
    bmSETUP_OTHER = bmBIT0 | bmBIT1 // Other recipient.
};

// Setup request code.
enum usb_setup_req_code {
    USB_SETUP_GET_STATUS = 0x00, // Get status code.
    USB_SETUP_CLEAR_FEATURE = 0x01, // Clear feature code.
    USB_SETUP_RESERVED1 = 0x02, // Reserved code.
    USB_SETUP_SET_FEATURE = 0x03, // Set feature code.
    USB_SETUP_RESERVED2 = 0x04, // Reserved code.
    USB_SETUP_SET_ADDRESS = 0x05, // Set address code.
    USB_SETUP_GET_DESCRIPTOR = 0x06, // Get descriptor code.
    USB_SETUP_SET_DESCRIPTOR = 0x07, // Set descriptor code.
    USB_SETUP_GET_CONFIGURATION = 0x08, // Get configuration code.
    USB_SETUP_SET_CONFIGURATION = 0x09, // Set configuration code.
    USB_SETUP_GET_INTERFACE = 0x0A, // Get interface code.
    USB_SETUP_SET_INTERFACE = 0x0B, // Set interface code.
    USB_SETUP_SYNC_FRAME = 0x0C, // Sync frame code.
    USB_SETUP_ANCHOR_LOAD = 0xA0 // Anchor load code.
};

// Standard status responses.
enum usb_setup_status_code {
    USB_STATUS_SELF_POWERED = 0x01,
    USB_STATUS_REMOTE_WAKEUP = 0x02
};

// Standard feature selectors.
enum usb_setup_feature_selector {
    USB_FEATURE_STALL = 0x00,
    USB_FEATURE_REMOTE_WAKEUP = 0x01,
    USB_FEATRUE_TEST_MODE = 0x02
};

// Get descriptor codes.
enum usb_setup_get_descriptor_code {
    USB_DESC_DEVICE = 0x01, // Device descriptor.
    USB_DESC_CONF = 0x02, // Configuration descriptor.
    USB_DESC_STRING = 0x03, // String descriptor.
    USB_DESC_INTERFACE = 0x04, // Interface descriptor.
    USB_DESC_ENDPOINT = 0x05, // End point descriptor.
    USB_DESC_DEVICE_QUAL = 0x06, // Device qualifier descriptor.
    USB_DESC_OTHER_SPEED_CONF = 0x07, // Other configuration descriptor.
    USB_DESC_INTERFACE_POWER = 0x08, // Interface power descriptor.
    USB_DESC_OTG = 0x09, // OTG descriptor.
    USB_DESC_DEBUG = 0x0A, // Debug descriptor.
    USB_DESC_INTERFACE_ASSOC = 0x0B, // Interface association descriptor.
    USB_DESC_HID = 0x21, // Get HID descriptor.
    USB_DESC_REPORT = 0x22 // Get report descriptor.
};

// End point configuration (EP1INCFG/EP1OUTCFG/EP2/EP4/EP6/EP8).
enum epcfg_bits {
    bmEP_VALID = bmBIT7,
    bmEP_DIR = bmBIT6, // Only for EP2-EP8!
    bmEP_TYPE = bmBIT5 | bmBIT4,
    bmEP_SIZE = bmBIT3, // Only for EP2-EP8!
    bmEP_BUF = bmBIT1 | bmBIT0 // Only for EP2-EP8!
};

enum ep_valid_bits {
    bmEP_DISABLE = 0,
    bmEP_ENABLE = bmBIT7
};

// Only for EP2-EP8!
enum ep_direction {
    bmEP_OUT = 0,
    bmEP_IN = bmBIT6
};

enum ep_type {
    bmEP_ISO = bmBIT4, // Only for EP2-EP8!
    bmEP_BULK = bmBIT5, // Default value.
    bmEP_INT = bmBIT4 | bmBIT5
};

// Only for EP2-EP8!
enum ep_size {
    EP_512 = 0,
    EP_1024 = bmBIT3 // Except EP4/EP8.
};

// Only for EP2-EP8!
enum ep_buf {
    EP_QUAD = 0,
    EP_DOUBLE = bmBIT1, // Default value.
    EP_TRIPLE = bmBIT0 | bmBIT1
};

struct ep0_buf {
    BYTE *dat;
    WORD len;
};

#define usb_disconnect() (USBCS |= bmDISCON)
#define usb_connect() (USBCS &= ~bmDISCON)

#define usb_is_high_speed() \
    (USBCS & bmHSM)

#define usp_ep_reset_toggle(ep) \
    TOGCTL = (((ep & 0x80) >> 3) + (ep & 0x0F)); \
    TOGCTL |= bmRESETTOGGLE

#define usb_ep0_stall() \
    EP0CS |= bmEPSTALL

#define usb_ep0_hsnack() \
    EP0CS |= bmHSNAK

#define usb_word_msb_get(word) \
    (BYTE)(((WORD)(word) >> 8) & 0xFF)
#define usb_word_lsb_get(word) \
    (BYTE)((WORD)(word) & 0xFF)

#define usb_word_swap(x) \
    ((((WORD)((x) & 0x00FF)) << 8) | \
    (((WORD)((x) & 0xFF00)) >> 8))

#define usb_dword_swap(x) \
    ((((DWORD)((x) & 0x000000FFul)) << 24) | \
    (((DWORD)((x) & 0x0000FF00ul)) << 8) | \
    (((DWORD)((x) & 0x00FF0000ul)) >> 8) | \
    (((DWORD)((x) & 0xFF000000ul)) >> 24))

void usb_init(void);
void usb_task(void);

#ifdef __cplusplus
}
#endif

#endif // FX2_USB_H
