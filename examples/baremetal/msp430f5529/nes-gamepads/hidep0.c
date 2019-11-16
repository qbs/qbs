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
#include "hwdefs.h"
#include "usb.h"

enum usb_ep0_action {
    STATUS_ACTION_NOTHING,
    STATUS_ACTION_DATA_IN
};

struct usb_ep0 {
    uint8_t IEPCNFG; // Input ep0 configuration register.
    uint8_t IEPBCNT; // Input ep0 buffer byte count.
    uint8_t OEPCNFG; // Output ep0 configuration register.
    uint8_t OEPBCNT; // Output ep0 buffer byte count.
};

struct usb_ep0_data {
    uint8_t data[EP_MAX_FIFO_SIZE];
    uint16_t length;
    uint16_t offset;
    enum usb_ep0_action action;
    bool host_ask_more_data_than_available;
};

#if defined(__ICC430__)
#pragma location = 0x0920 // Input ep0 configuration address.
__no_init struct usb_ep0 __data16 g_ep0;
#pragma location = 0x2378 // Input ep0 buffer address.
__no_init uint8_t __data16 g_ep0_in_buf[EP0_MAX_PACKET_SIZE];
#pragma location = 0x2380 // Setup packet block address.
__no_init uint8_t __data16 g_setupdat[EP0_MAX_PACKET_SIZE];
#elif defined(__GNUC__)
extern struct usb_ep0 g_ep0;
extern uint8_t g_ep0_in_buf[EP0_MAX_PACKET_SIZE];
extern uint8_t g_setupdat[EP0_MAX_PACKET_SIZE];
#endif

static volatile bool g_enumerated = false;
static bool g_rwuen = false;
static bool g_selfpwr = false;
static struct usb_ep0_data g_ep0_response;

static bool ep0_in_response_create(const uint8_t *data, uint16_t length)
{
    if (sizeof(g_ep0_response.data) < length)
        return false;

    for (uint16_t i = 0; i < length; ++i)
        g_ep0_response.data[i] = data[i];

    g_ep0_response.length = length;
    g_ep0_response.offset = 0;

    const uint16_t setup_length = (g_setupdat[7] << 8) | g_setupdat[6];
    if (g_ep0_response.length >= setup_length) {
        g_ep0_response.length = setup_length;
        g_ep0_response.host_ask_more_data_than_available = false;
    } else {
        g_ep0_response.host_ask_more_data_than_available = true;
    }

    return true;
}

void ep0_in_frame_send(void)
{
    uint8_t frame_size = 0;

    if (g_ep0_response.length != EP_NO_MORE_DATA) {
        if (g_ep0_response.length > EP0_MAX_PACKET_SIZE) {
            frame_size = EP0_MAX_PACKET_SIZE;
            g_ep0_response.length -= EP0_MAX_PACKET_SIZE;
            g_ep0_response.action = STATUS_ACTION_DATA_IN;
        } else if (g_ep0_response.length < EP0_MAX_PACKET_SIZE) {
            frame_size = g_ep0_response.length;
            g_ep0_response.length = EP_NO_MORE_DATA;
            g_ep0_response.action = STATUS_ACTION_NOTHING;
        } else {
            frame_size = EP0_MAX_PACKET_SIZE;
            if (g_ep0_response.host_ask_more_data_than_available) {
                g_ep0_response.length = 0;
                g_ep0_response.action = STATUS_ACTION_DATA_IN;
            } else {
                g_ep0_response.length = EP_NO_MORE_DATA;
                g_ep0_response.action = STATUS_ACTION_NOTHING;
            }
        }

        for (uint8_t i = 0; i < frame_size; ++i) {
            g_ep0_in_buf[i] = g_ep0_response.data[g_ep0_response.offset];
            ++g_ep0_response.offset;
        }

        g_ep0.IEPBCNT = frame_size;
    } else {
        g_ep0_response.action = STATUS_ACTION_NOTHING;
    }
}

static bool ep0_dev_status_get(void)
{
    uint16_t status = 0;
    if (g_selfpwr)
        status |= STATUS_SELF_POWERED;
    if (g_rwuen)
        status |= STATUS_REMOTE_WAKEUP;
    ep0_in_response_create((uint8_t *)&status, sizeof(status));
    return true;
}

static bool ep0_iface_status_get(void)
{
    uint16_t status = 0;
    ep0_in_response_create((uint8_t *)&status, sizeof(status));
    return true;
}

static bool ep0_ep_status_get(void)
{
    uint16_t status = 0;
    const uint8_t ep = hid_ep0_setup_lindex_get();
    const uint8_t ep_num = ep & ~SETUP_DIR;
    if (ep_num == 0) {
        status = hid_ep0_in_is_stalled() ? 1 : 0;
    } else if ((ep_num == 1) && (ep & SETUP_DIR) == SETUP_INPUT) {
        // We support only one input ep1 in.
        status = hid_ep1_in_is_stalled() ? 1 : 0;
    } else {
        return false;
    }

    ep0_in_response_create((uint8_t *)&status, sizeof(status));
    return true;
}

static bool ep0_get_status_proc(void)
{
    const uint8_t bm_req_type = hid_ep0_setup_bm_request_type_get();
    if ((bm_req_type & SETUP_DIR) != SETUP_INPUT)
        return false;
    const uint8_t recipient = bm_req_type & SETUP_RECIPIENT;
    switch (recipient) {
    case SETUP_DEVICE:
        return ep0_dev_status_get();
    case SETUP_IFACE:
        return ep0_iface_status_get();
    case SETUP_EP:
        return ep0_ep_status_get();
    default:
        break;
    }
    return false;
}

static bool ep0_dev_feature_clear(void)
{
    const uint8_t feature = hid_ep0_setup_lvalue_get();
    if (feature != FEATURE_REMOTE_WAKEUP)
        return false;
    g_rwuen = false;
    return true;
}

static bool ep0_ep_feature_clear(void)
{
    const uint8_t feature = hid_ep0_setup_lvalue_get();
    if (feature != FEATURE_STALL)
        return false;
    const uint8_t ep = hid_ep0_setup_lindex_get();
    const uint8_t ep_num = ep & ~SETUP_DIR;
    if (ep_num == 0) {
        // Do nothing.
    } else if ((ep_num == 1) && (ep & SETUP_DIR) == SETUP_INPUT) {
        // We support only one input ep1 in.
        hid_ep1_in_unstall();
    } else {
        return false;
    }

    hid_ep0_in_clear();
    return true;
}

static bool ep0_clear_feature_proc(void)
{
    const uint8_t bm_req_type = hid_ep0_setup_bm_request_type_get();
    if ((bm_req_type & SETUP_DIR) != SETUP_INPUT)
        return false;
    const uint8_t recipient = bm_req_type & SETUP_RECIPIENT;
    switch (recipient) {
    case SETUP_DEVICE:
        return ep0_dev_feature_clear();
    case SETUP_EP:
        return ep0_ep_feature_clear();
    default:
        break;
    }
    return false;
}

static bool ep0_dev_feature_set(void)
{
    const uint8_t feature = hid_ep0_setup_lvalue_get();
    switch (feature) {
    case FEATURE_REMOTE_WAKEUP:
        g_rwuen = true;
        return true;
    case FEATURE_TEST_MODE:
        // This is "test mode", just return the handshake.
        return true;
    default:
        break;
    }
    return false;
}

static bool ep0_ep_feature_set(void)
{
    const uint8_t feature = hid_ep0_setup_lvalue_get();
    if (feature != FEATURE_STALL)
        return false;
    const uint8_t ep = hid_ep0_setup_lindex_get();
    const uint8_t ep_num = ep & ~SETUP_DIR;
    if (ep_num == 0) {
        // Do nothing.
    } else if ((ep_num == 1) && (ep & SETUP_DIR) == SETUP_INPUT) {
        // We support only one input ep1 in.
        hid_ep1_in_stall();
    } else {
        return false;
    }
    hid_ep0_in_clear();
    return true;
}

static bool ep0_set_feature_proc(void)
{
    const uint8_t bm_req_type = hid_ep0_setup_bm_request_type_get();
    switch (bm_req_type & SETUP_RECIPIENT) {
    case SETUP_DEVICE:
        return ep0_dev_feature_set();
    case SETUP_EP:
        return ep0_ep_feature_set();
    default:
        break;
    }
    return false;
}

static bool ep0_set_address_proc(void)
{
    hid_ep0_out_stall();
    const uint8_t address = hid_ep0_setup_lvalue_get();
    if (address >= 128)
        return false;
    USBFUNADR = address;
    hid_ep0_in_clear();
    return true;
}

static bool ep0_descriptor_proc(uint8_t type)
{
    uint16_t length = 0;
    const uint8_t *pdesc = hid_ep0_desc_get(type, &length);
    if (!pdesc)
        return false;
    ep0_in_response_create(pdesc, length);
    return true;
}

static bool ep0_get_descriptor_proc(void)
{
    const uint8_t descr_type = hid_ep0_setup_hvalue_get();
    switch (descr_type) {
    case DESC_DEVICE:
    case DESC_CONF:
    case DESC_STRING:
    case DESC_DEVICE_QUAL:
    case DESC_OTHER_SPEED_CONF:
    case DESC_HID:
    case DESC_REPORT:
        return ep0_descriptor_proc(descr_type);
    }
    return false;
}

static bool ep0_get_config_proc(void)
{
    // We only support configuration 1.
    const uint8_t cfg_num = HID_CONFIG_NUMBER;
    ep0_in_response_create(&cfg_num, sizeof(cfg_num));
    return true;
}

static bool ep0_set_config_proc(void)
{
    const uint8_t cfg_num = hid_ep0_setup_lvalue_get();
    // We only support configuration 1.
    const bool is_valid = (cfg_num & HID_CONFIG_NUMBER);
    hid_ep0_enumerated_set(is_valid);
    return is_valid;
}

static bool ep0_get_iface_proc(void)
{
    const uint8_t iface_num = hid_ep0_setup_lindex_get();
    if (iface_num != HID_IFACE_NUMBER)
        return false;
    ep0_in_response_create(&iface_num, sizeof(iface_num));
    return true;
}

static bool ep0_set_iface_proc(void)
{
    const uint8_t iface_num = hid_ep0_setup_lindex_get();
    if (iface_num != HID_IFACE_NUMBER)
        return false;

    const uint8_t alt_iface_num = hid_ep0_setup_lvalue_get();
    if (alt_iface_num != HID_ALT_IFACE_NUMBER)
        return false;

    hid_ep0_out_stall();
    hid_ep0_in_clear();
    return true;
}

static bool ep0_std_proc(void)
{
    const uint8_t request_code = hid_ep0_setup_request_get();
    switch (request_code) {
    case SETUP_GET_STATUS:
        return ep0_get_status_proc();
    case SETUP_CLEAR_FEATURE:
        return ep0_clear_feature_proc();
    case SETUP_SET_FEATURE:
        return ep0_set_feature_proc();
    case SETUP_SET_ADDRESS:
        return ep0_set_address_proc();
    case SETUP_GET_DESCRIPTOR:
        return ep0_get_descriptor_proc();
    case SETUP_GET_CONFIGURATION:
        return ep0_get_config_proc();
    case SETUP_SET_CONFIGURATION:
        return ep0_set_config_proc();
    case SETUP_GET_INTERFACE:
        return ep0_get_iface_proc();
    case SETUP_SET_INTERFACE:
        return ep0_set_iface_proc();
    default:
        break;
    }
    return false;
}

static bool ep0_setup(void)
{
    const uint8_t bm_req_type = hid_ep0_setup_bm_request_type_get();
    const uint8_t setup_type = bm_req_type & SETUP_TYPE;
    switch (setup_type) {
    case SETUP_STANDARD:
        return ep0_std_proc();
    default:
        break;
    }
    return false;
}

static bool ep0_has_another_setup(void)
{
    if ((USBIFG & STPOWIFG) != 0) {
        USBIFG &= ~(STPOWIFG | SETUPIFG);
        return true;
    }
    return false;
}

void hid_ep0_init(void)
{
    hid_ep0_in_nak();
    hid_ep0_out_nak();

    g_ep0.IEPCNFG = UBME | STALL | USBIIE;
    g_ep0.OEPCNFG = UBME | STALL | USBIIE;

    // Enable only ep0in interrupts.
    USBIEPIE |= BIT0;
}

void hid_ep0_setup_handler(void)
{
    USBCTL |= FRSTE;

    g_ep0_response.length = 0;
    g_ep0_response.offset = 0;
    g_ep0_response.action = STATUS_ACTION_NOTHING;
    g_ep0_response.host_ask_more_data_than_available = false;

    for (;;) {
        const uint8_t bm_req_type = hid_ep0_setup_bm_request_type_get();
        if ((bm_req_type & SETUP_INPUT) == SETUP_INPUT)
            USBCTL |= DIR;
        else
            USBCTL &= ~DIR;

        const bool success = ep0_setup();
        if (success) {
            hid_ep0_out_clear();
            ep0_in_frame_send();
        } else {
            hid_ep0_in_stall();
        }

        if (!ep0_has_another_setup())
            return;
    }
}

void hid_ep0_in_handler(void)
{
    USBCTL |= FRSTE;
    hid_ep0_out_clear();
    if (g_ep0_response.action == STATUS_ACTION_DATA_IN)
        ep0_in_frame_send();
    else
        hid_ep0_in_stall();
}

void hid_ep0_in_nak(void)
{
    g_ep0.IEPBCNT = NAK;
}

void hid_ep0_in_stall(void)
{
    g_ep0.IEPCNFG |= STALL;
}

void hid_ep0_in_clear(void)
{
    g_ep0_response.length = EP_NO_MORE_DATA;
    g_ep0_response.action = STATUS_ACTION_NOTHING;
    g_ep0.IEPBCNT = 0;
}

bool hid_ep0_in_is_stalled(void)
{
    return (g_ep0.IEPCNFG & STALL);
}

void hid_ep0_out_nak(void)
{
    g_ep0.OEPBCNT = NAK;
}

void hid_ep0_out_stall(void)
{
    g_ep0.OEPCNFG |= STALL;
}

void hid_ep0_out_clear(void)
{
    g_ep0.OEPBCNT = 0;
}

uint8_t hid_ep0_setup_bm_request_type_get(void)
{
    return g_setupdat[0];
}

uint8_t hid_ep0_setup_request_get(void)
{
    return g_setupdat[1];
}

uint8_t hid_ep0_setup_lvalue_get(void)
{
    return g_setupdat[2];
}

uint8_t hid_ep0_setup_hvalue_get(void)
{
    return g_setupdat[3];
}

uint8_t hid_ep0_setup_lindex_get(void)
{
    return g_setupdat[4];
}

uint8_t hid_ep0_setup_hindex_get(void)
{
    return g_setupdat[5];
}

void hid_ep0_enumerated_set(bool enumerated)
{
    g_enumerated = enumerated;
}

bool hid_ep0_is_enumerated(void)
{
    return g_enumerated;
}
