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

#ifndef MSP430_USB_H
#define MSP430_USB_H

#ifdef __cplusplus
extern "C" {
#endif

enum usb_setup_bmreq_bits {
    SETUP_DIR = 0x80,
    SETUP_TYPE = 0x60,
    SETUP_RECIPIENT = 0x1F
};

// Setup request direction.
enum usb_setup_req_direction_bits {
    SETUP_OUTPUT = 0, // From host to device direction.
    SETUP_INPUT = 0x80 // From device to host direction.
};

// Setup request type.
enum usb_setup_req_type_bits {
    SETUP_STANDARD = 0, // Standard request.
    SETUP_CLASS = 0x20, // Class request.
    SETUP_VENDOR = 0x40 // Vendor request.
};

// Setup request recipient.
enum usb_setup_req_recipient_bits {
    SETUP_DEVICE = 0, // Device recipient.
    SETUP_IFACE = 0x01, // Interface recipient.
    SETUP_EP = 0x02, // End point recipient.
    SETUP_OTHER = 0x03 // Other recipient.
};

// Setup request code.
enum usb_setup_req_code {
    SETUP_GET_STATUS = 0x00, // Get status code.
    SETUP_CLEAR_FEATURE = 0x01, // Clear feature code.
    SETUP_RESERVED1 = 0x02, // Reserved code.
    SETUP_SET_FEATURE = 0x03, // Set feature code.
    SETUP_RESERVED2 = 0x04, // Reserved code.
    SETUP_SET_ADDRESS = 0x05, // Set address code.
    SETUP_GET_DESCRIPTOR = 0x06, // Get descriptor code.
    SETUP_SET_DESCRIPTOR = 0x07, // Set descriptor code.
    SETUP_GET_CONFIGURATION = 0x08, // Get configuration code.
    SETUP_SET_CONFIGURATION = 0x09, // Set configuration code.
    SETUP_GET_INTERFACE = 0x0A, // Get interface code.
    SETUP_SET_INTERFACE = 0x0B, // Set interface code.
    SETUP_SYNC_FRAME = 0x0C, // Sync frame code.
    SETUP_ANCHOR_LOAD = 0xA0 // Anchor load code.
};

// Standard status responses.
enum usb_setup_status_code {
    STATUS_SELF_POWERED = 0x01,
    STATUS_REMOTE_WAKEUP = 0x02
};

// Standard feature selectors.
enum usb_setup_feature_selector {
    FEATURE_STALL = 0x00,
    FEATURE_REMOTE_WAKEUP = 0x01,
    FEATURE_TEST_MODE = 0x02
};

// Get descriptor codes.
enum usb_setup_get_descriptor_code {
    DESC_DEVICE = 0x01, // Device descriptor.
    DESC_CONF = 0x02, // Configuration descriptor.
    DESC_STRING = 0x03, // String descriptor.
    DESC_INTERFACE = 0x04, // Interface descriptor.
    DESC_ENDPOINT = 0x05, // End point descriptor.
    DESC_DEVICE_QUAL = 0x06, // Device qualifier descriptor.
    DESC_OTHER_SPEED_CONF = 0x07, // Other configuration descriptor.
    DESC_INTERFACE_POWER = 0x08, // Interface power descriptor.
    DESC_OTG = 0x09, // OTG descriptor.
    DESC_DEBUG = 0x0A, // Debug descriptor.
    DESC_INTERFACE_ASSOC = 0x0B, // Interface association descriptor.
    DESC_HID = 0x21, // Get HID descriptor.
    DESC_REPORT = 0x22 // Get report descriptor.
};


enum usb_ep_size {
    EP0_MAX_PACKET_SIZE = 8,
    EP_MAX_PACKET_SIZE = 64,
    EP_MAX_FIFO_SIZE = 256,
    EP_NO_MORE_DATA = 0xFFFF
};

void usb_init(void);
void usb_task(void);

#ifdef __cplusplus
}
#endif

#endif // MSP430_USB_H
