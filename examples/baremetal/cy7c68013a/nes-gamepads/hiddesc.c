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

#include "hid.h"

enum usb_bcd_version {
    USB_SPEC_BCD_VERSION = 0x0200,
    USB_DEVICE_BCD_VERSION = 0x1234,
    USB_HID_BCD_VERSION = 0x0111
};

enum usb_ids {
    USB_DEVICE_VID = 0xFFFF,
    USB_DEVICE_PID = 0xFFFF
};

enum usb_lang_id {
    USB_LANG_ID = 0x0409
};

enum usb_descriptor_string_index {
    USB_DESC_LANGID_STRING_INDEX = 0,
    USB_DESC_MFG_STRING_INDEX,
    USB_DESC_PRODUCT_STRING_INDEX,
    USB_DESC_SERIAL_STRING_INDEX
};

enum usb_descr_length {
    // Standard length.
    USB_DESC_DEVICE_LEN = 18,
    USB_DESC_DEVICE_QUAL_LEN = 10,
    USB_DESC_CONF_LEN = 9,
    USB_DESC_OTHER_SPEED_CONF_LEN = 9,
    USB_DESC_INTERFACE_LEN = 9,
    USB_DESC_HID_LEN = 9,
    USB_DESCR_HID_REP_LEN = 56,
    USB_DESC_ENDPOINT_LEN = 7,

    // Total length.
    USB_DESC_DEVICE_TOTAL_LEN = USB_DESC_DEVICE_LEN,
    USB_DESC_DEVICE_QUAL_TOTAL_LEN = USB_DESC_DEVICE_QUAL_LEN,

    USB_DESC_CONF_TOTAL_LEN = USB_DESC_CONF_LEN
        + USB_DESC_INTERFACE_LEN
        + USB_DESC_HID_LEN + USB_DESC_ENDPOINT_LEN,

    USB_DESC_OTHER_SPEED_CONF_TOTAL_LEN = USB_DESC_OTHER_SPEED_CONF_LEN
        + USB_DESC_INTERFACE_LEN,
};

enum usb_descr_attributes {
    // Attributes (b7 - buspwr, b6 - selfpwr, b5 - rwu).
    USB_DESC_ATTRIBUTES = 0xA0,
    // 100 mA (div 2).
    USB_DESC_POWER_CONSUMPTION = 50
};

enum usb_descr_numbers {
    USB_DESC_CONFIG_COUNT = 1
};

static const BYTE CODE
g_hid_report_desc[USB_DESCR_HID_REP_LEN] = {
    // Pad #1 configuration.
    0x05, 0x01, // Usage page (Generic Desktop).
    0x09, 0x05, // Usage (Game Pad).
    0xa1, 0x01, // Collection (Application).
    0xa1, 0x00, //   Collection (Physical).
    0x85, HID_REPORT_ID_GAMEPAD1, // Report id (1).
    0x05, 0x09, //     Usage page (Button).
    0x19, 0x01, //     Usage minimum (Button 1).
    0x29, 0x08, //     Usage maximum (Button 8).
    0x15, 0x00, //     Logical minimum (0).
    0x25, 0x01, //     Logical maximum (1).
    0x95, HID_REPORT_BITS_COUNT, // Report count (8).
    0x75, 0x01, //     Report size (1).
    0x81, 0x02, //     Input (Data,Var,Abs).
    0xc0,       //   End collection.
    0xc0,       // End collection.
    // Pad #2 configuration.
    0x05, 0x01, // Usage page (Generic Desktop).
    0x09, 0x05, // Usage (Game Pad).
    0xa1, 0x01, // Collection (Application).
    0xa1, 0x00, //   Collection (Physical).
    0x85, HID_REPORT_ID_GAMEPAD2, // Report id (2).
    0x05, 0x09, //     Usage page (Button).
    0x19, 0x01, //     Usage minimum (Button 1).
    0x29, 0x08, //     Usage maximum (Button 8).
    0x15, 0x00, //     Logical minimum (0).
    0x25, 0x01, //     Logical maximum (1).
    0x95, HID_REPORT_BITS_COUNT, // Report count (8).
    0x75, 0x01, //     Report size (1).
    0x81, 0x02, //     Input (Data,Var,Abs).
    0xc0,       //   End collection
    0xc0        // End collection.
};

static const BYTE CODE
g_device_desc[USB_DESC_DEVICE_TOTAL_LEN] = {
    USB_DESC_DEVICE_LEN, // Descriptor length.
    USB_DESC_DEVICE, // Descriptor type.
    usb_word_lsb_get(USB_SPEC_BCD_VERSION), // USB BCD version, lo.
    usb_word_msb_get(USB_SPEC_BCD_VERSION), // USB BCD version, hi.
    0x00, // Device class.
    0x00, // Device sub-class.
    0x00, // Device protocol.
    MAX_EP0_SIZE, // Maximum packet size (ep0 size).
    usb_word_lsb_get(USB_DEVICE_VID), // Vendor ID, lo.
    usb_word_msb_get(USB_DEVICE_VID), // Vendor ID, hi.
    usb_word_lsb_get(USB_DEVICE_PID), // Product ID, lo.
    usb_word_msb_get(USB_DEVICE_PID), // Product ID, hi.
    usb_word_lsb_get(USB_DEVICE_BCD_VERSION), // Device BCD version, lo.
    usb_word_msb_get(USB_DEVICE_BCD_VERSION), // Device BCD version, hi.
    USB_DESC_MFG_STRING_INDEX,
    USB_DESC_PRODUCT_STRING_INDEX,
    USB_DESC_SERIAL_STRING_INDEX,
    USB_DESC_CONFIG_COUNT // Configurations count.
};

static const BYTE CODE
g_device_qual_desc[USB_DESC_DEVICE_QUAL_TOTAL_LEN] = {
    USB_DESC_DEVICE_QUAL_LEN, // Descriptor length.
    USB_DESC_DEVICE_QUAL, // Descriptor type.
    usb_word_lsb_get(USB_SPEC_BCD_VERSION), // USB BCD version, lo.
    usb_word_msb_get(USB_SPEC_BCD_VERSION), // USB BCD version, hi.
    0x00, // Device class.
    0x00, // Device sub-class.
    0x00, // Device protocol.
    MAX_EP0_SIZE, // Maximum packet size (ep0 size).
    USB_DESC_CONFIG_COUNT, // Configurations count.
    0x00 // Reserved.
};

static const BYTE CODE
g_config_desc[USB_DESC_CONF_TOTAL_LEN] = {
    USB_DESC_CONF_LEN, // Descriptor length.
    USB_DESC_CONF, // Descriptor type.
    usb_word_lsb_get(USB_DESC_CONF_TOTAL_LEN), // Total length, lo.
    usb_word_msb_get(USB_DESC_CONF_TOTAL_LEN), // Total length, hi.
    0x01, // Interfaces count.
    HID_CONFIG_NUMBER, // Configuration number.
    0x00, // Configuration string index.
    USB_DESC_ATTRIBUTES, // Attributes.
    USB_DESC_POWER_CONSUMPTION, // Power consumption.

    // Interface descriptor.
    USB_DESC_INTERFACE_LEN, // Descriptor length.
    USB_DESC_INTERFACE, // Descriptor type.
    HID_IFACE_NUMBER, // Zero-based index of this interfacce.
    HID_ALT_IFACE_NUMBER, // Alternate setting.
    0x01, // End points count (ep1 in).
    0x03, // Class code (HID).
    0x00, // Subclass code (boot).
    0x00, // Protocol code (none).
    0x00, // Interface string descriptor index.

    // HID descriptor.
    USB_DESC_HID_LEN, // Descriptor length.
    USB_DESC_HID, // Descriptor type.
    usb_word_lsb_get(USB_HID_BCD_VERSION), // HID class BCD version, lo.
    usb_word_msb_get(USB_HID_BCD_VERSION), // HID class BCD version, hi.
    0x00, // Country code (HW country code).
    0x01, // Number of HID class descriptors to follow.
    USB_DESC_REPORT, // Repord descriptor type (HID).
    usb_word_lsb_get(USB_DESCR_HID_REP_LEN), // Report descriptor total length, lo.
    usb_word_msb_get(USB_DESCR_HID_REP_LEN), // Report descriptor total length, hi.

    // End point descriptor.
    USB_DESC_ENDPOINT_LEN, // Descriptor length.
    USB_DESC_ENDPOINT, // Descriptor type.
    HID_EP_IN, // End point address (ep1 in).
    0x03, // End point type (interrupt).
    usb_word_lsb_get(MAX_EP1_SIZE), // Maximum packet size, lo (ep1 size).
    usb_word_msb_get(MAX_EP1_SIZE), // Maximum packet size, hi (ep1 size).
    0x01 // Polling interval (1 ms).
};

static const BYTE CODE
g_other_config_desc[USB_DESC_OTHER_SPEED_CONF_TOTAL_LEN] = {
    USB_DESC_OTHER_SPEED_CONF_LEN, // Descriptor length.
    USB_DESC_OTHER_SPEED_CONF, // Descriptor type.
    usb_word_lsb_get(USB_DESC_OTHER_SPEED_CONF_TOTAL_LEN), // Total length, lo.
    usb_word_msb_get(USB_DESC_OTHER_SPEED_CONF_TOTAL_LEN), // Total length, hi.
    0x01, // Interfaces number.
    0x01, // Configuration number.
    0x00, // Configuration string index.
    USB_DESC_ATTRIBUTES, // Attributes.
    USB_DESC_POWER_CONSUMPTION, // Power consumption.

    // Interface descriptor.
    USB_DESC_INTERFACE_LEN, // Descriptor length.
    USB_DESC_INTERFACE, // Descriptor type.
    0x00, // Zero-based index of this interfacce.
    0x00, // Alternate setting.
    0x00, // End points number (only ep0).
    0x00, // Class code.
    0x00, // Subclass code.USB_DESC_OTHER_SPEED_CONFIGURATION
    0x00, // Protocol code.
    0x00 // Interface string descriptor index.
};

static const BYTE CODE
g_lang_id_desc[] = {
    0x04, // Descriptor length.
    USB_DESC_STRING, // Descriptor type.
    usb_word_lsb_get(USB_LANG_ID), // Language id, lo.
    usb_word_msb_get(USB_LANG_ID) // Language id, hi.
};

static const BYTE CODE
g_manuf_str_desc[] = {
    0x18, // Descriptor length.
    USB_DESC_STRING, // Descriptor type.
    // Unicode string:
    'Q', 0, 'B', 0, 'S', 0, ' ', 0,
    'e', 0, 'x', 0, 'a', 0, 'm', 0, 'p', 0, 'l', 0, 'e', 0
};

static const BYTE CODE
g_product_str_desc[] = {
    0x1A, // Descriptor length.
    USB_DESC_STRING, // Descriptor type.
    // Unicode string:
    'N', 0, 'E', 0, 'S', 0, ' ', 0,
    'G', 0, 'a', 0, 'm', 0, 'e', 0,
    'P', 0, 'a', 0, 'd', 0, 's', 0
};

static const BYTE CODE
g_serialno_str_desc[] = {
    0x1A, // Descriptor length.
    USB_DESC_STRING, // Descriptor type.
    // Unicode string:
    '0', 0 ,'1', 0, '0', 0, '2', 0, '0', 0, '3', 0,
    '0', 0, '4', 0, '0', 0, '5', 0, '0', 0, '6', 0
};

static const BYTE CODE *ep0_string_desc_get(void)
{
    switch (SETUPDAT[2]) {
    case USB_DESC_LANGID_STRING_INDEX:
        return g_lang_id_desc;
    case USB_DESC_MFG_STRING_INDEX:
        return g_manuf_str_desc;
    case USB_DESC_PRODUCT_STRING_INDEX:
        return g_product_str_desc;
    case USB_DESC_SERIAL_STRING_INDEX:
        return g_serialno_str_desc;
    default:
        break;
    }

    return NULL;
}

static const BYTE CODE *ep0_config_desc_get(BOOL other)
{
    if (!other) {
        if (SETUPDAT[2] == 0) {
            return usb_is_high_speed() ? g_config_desc
                                       : g_other_config_desc;
        }
    } else {
        return usb_is_high_speed() ? g_other_config_desc
                                   : g_config_desc;
    }

    return NULL;
}

const BYTE CODE *hid_ep0_std_desc_get(void)
{
    switch (SETUPDAT[3]) {
    case USB_DESC_DEVICE:
        return g_device_desc;
    case USB_DESC_CONF:
        return ep0_config_desc_get(FALSE);
    case USB_DESC_STRING:
        return ep0_string_desc_get();
    case USB_DESC_DEVICE_QUAL:
        return g_device_qual_desc;
    case USB_DESC_OTHER_SPEED_CONF:
        return ep0_config_desc_get(TRUE);
    case USB_DESC_HID:
        return &g_config_desc[USB_DESC_CONF_LEN
                + USB_DESC_INTERFACE_LEN];
    default:
        break;
    }

    return NULL;
}

const BYTE CODE *hid_ep0_report_desc_get(WORD *length)
{
    *length = sizeof(g_hid_report_desc);
    return g_hid_report_desc;
}
