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
#include "core.h"
#include "usb.h"

static BOOL g_rwuen = FALSE;
static BOOL g_selfpwr = FALSE;

static void ep0_ep1in_reset(void)
{
    EP1INCS &= ~bmEPBUSY;
    sync_delay();
    usp_ep_reset_toggle(HID_EP_IN);
    sync_delay();
}

// Get status handle.

static BYTE *ep_address_get(BYTE ep)
{
    const BYTE ep_num = ep & ~bmSETUP_DIR;
    switch (ep_num) {
    case 0:
        return (BYTE *)&EP0CS;
    case 1:
        return (ep & bmSETUP_DIR)
                ? (BYTE *)&EP1INCS
                : (BYTE *)&EP1OUTCS;
    default:
        break;
    }
    return NULL;
}

static BOOL ep0_dev_status_get(void)
{
    BYTE status = 0;
    if (g_selfpwr)
        status |= USB_STATUS_SELF_POWERED;
    if (g_rwuen)
        status |= USB_STATUS_REMOTE_WAKEUP;
    EP0BUF[0] = status;
    EP0BUF[1] = 0;
    EP0BCH = 0;
    EP0BCL = 2;
    return TRUE;
}

static BOOL ep0_iface_status_get(void)
{
    EP0BUF[0] = 0;
    EP0BUF[1] = 0;
    EP0BCH = 0;
    EP0BCL = 2;
    return TRUE;
}

static BOOL ep0_ep_status_get(void)
{
    const volatile BYTE XDATA *pep =
            (BYTE XDATA *)ep_address_get(SETUPDAT[4]);
    if (pep) {
        EP0BUF[0] = *pep & bmEPSTALL ? 1 : 0;
        EP0BUF[1] = 0;
        EP0BCH = 0;
        EP0BCL = 2;
        return TRUE;
    }

    return FALSE;
}

static BOOL ep0_get_status_proc(void)
{
    if ((SETUPDAT[0] & bmSETUP_TO_HOST) == 0)
        return FALSE;

    switch (SETUPDAT[0] & bmSETUP_RECIPIENT) {
    case bmSETUP_DEVICE:
        return ep0_dev_status_get();
    case bmSETUP_IFACE:
        return ep0_iface_status_get();
    case bmSETUP_EP:
        return ep0_ep_status_get();
    default:
        break;
    }

    return FALSE;
}

// Clear feature handle.

static BOOL ep0_dev_feature_clear(void)
{
    if (SETUPDAT[2] == USB_FEATURE_REMOTE_WAKEUP) {
        g_rwuen = FALSE;
        return TRUE;
    }

    return FALSE;
}

static BOOL ep0_ep_feature_clear(void)
{
    if (SETUPDAT[2] == USB_FEATURE_STALL) {
        volatile BYTE XDATA *pep =
                (BYTE XDATA *)ep_address_get(SETUPDAT[4]);
        if (!pep)
            return FALSE;
        *pep &= ~bmEPSTALL;
        return TRUE;
    }

    return FALSE;
}

static BOOL ep0_clear_feature_proc(void)
{
    switch (SETUPDAT[0] & bmSETUP_RECIPIENT) {
    case bmSETUP_DEVICE:
        return ep0_dev_feature_clear();
    case bmSETUP_EP:
        return ep0_ep_feature_clear();
    default:
        break;
    }

    return FALSE;
}

// Set feature handle.

static BOOL ep0_dev_feature_set(void)
{
    switch (SETUPDAT[2]) {
    case USB_FEATURE_REMOTE_WAKEUP:
        g_rwuen = TRUE;
        return TRUE;
    case USB_FEATRUE_TEST_MODE:
        // This is "test mode", just return the handshake.
        return TRUE;
    default:
        break;
    }

    return FALSE;
}

static BOOL ep0_ep_feature_set(void)
{
    if (SETUPDAT[2] == USB_FEATURE_STALL) {
        volatile BYTE XDATA *pep =
                (BYTE XDATA *)ep_address_get(SETUPDAT[4]);
        if (!pep)
            return FALSE;
        *pep |= bmEPSTALL;
        usp_ep_reset_toggle(SETUPDAT[4]);
        return TRUE;
    }

    return FALSE;
}

static BOOL ep0_set_feature_proc(void)
{
    switch (SETUPDAT[0] & bmSETUP_RECIPIENT) {
    case bmSETUP_DEVICE:
        return ep0_dev_feature_set();
    case bmSETUP_EP:
        return ep0_ep_feature_set();
    default:
        break;
    }

    return FALSE;
}

// Get descriptor handle.

static BOOL ep0_std_descriptor_proc(void)
{
    const BYTE XDATA *pdesc =
            (BYTE XDATA *)hid_ep0_std_desc_get();
    if (pdesc) {
        SUDPTRH = usb_word_msb_get(pdesc);
        SUDPTRL = usb_word_lsb_get(pdesc);
        return TRUE;
    }

    return FALSE;
}

static BOOL ep0_report_descriptor_proc(void)
{
    WORD i = 0;
    WORD length = 0;
    const BYTE XDATA *pdesc =
            (BYTE XDATA *)hid_ep0_report_desc_get(&length);
    if (pdesc) {
        AUTOPTRH1 = usb_word_msb_get(pdesc);
        AUTOPTRL1 = usb_word_lsb_get(pdesc);

        for (i = 0; i < length; ++i)
           EP0BUF[i] = XAUTODAT1;

        EP0BCH = usb_word_msb_get(length);
        EP0BCL = usb_word_lsb_get(length);
        return TRUE;
    }

    return FALSE;
}

static BOOL ep0_get_descriptor_proc(void)
{
    switch (SETUPDAT[3]) {
    case USB_DESC_DEVICE:
    case USB_DESC_CONF:
    case USB_DESC_STRING:
    case USB_DESC_DEVICE_QUAL:
    case USB_DESC_OTHER_SPEED_CONF:
    case USB_DESC_HID:
        return ep0_std_descriptor_proc();
    case USB_DESC_REPORT:
        return ep0_report_descriptor_proc();
    }

    return FALSE;
}

// Get configuration handle.

static BOOL ep0_get_config_proc(void)
{
    // We only support configuration 1.
    EP0BUF[0] = HID_CONFIG_NUMBER;
    EP0BCH = 0;
    EP0BCL = 1;
    return TRUE;
}

// Set configuration handle.

static BOOL ep0_set_config_proc(void)
{
    // We only support configuration 1.
    if (SETUPDAT[2] != HID_CONFIG_NUMBER)
        return FALSE;

    return TRUE;
}

// Get interface handle.

static BOOL ep0_get_iface_proc(void)
{
    if (SETUPDAT[4] != HID_IFACE_NUMBER)
        return FALSE;

    EP0BUF[0] = HID_ALT_IFACE_NUMBER;
    EP0BCH = 0;
    EP0BCL = 1;
    return TRUE;
}

// Set interface handle.

static BOOL ep0_set_iface_proc(void)
{
    if (SETUPDAT[4] != HID_IFACE_NUMBER)
        return FALSE;

    if (SETUPDAT[2] != HID_ALT_IFACE_NUMBER)
        return FALSE;

    ep0_ep1in_reset();
    return TRUE;
}

static BOOL ep0_std_proc(void)
{
    switch (SETUPDAT[1]) {
    case USB_SETUP_GET_STATUS:
        return ep0_get_status_proc();
    case USB_SETUP_CLEAR_FEATURE:
        return ep0_clear_feature_proc();
    case USB_SETUP_SET_FEATURE:
        return ep0_set_feature_proc();
    case USB_SETUP_SET_ADDRESS:
        // Handles automatically by FX2.
        return TRUE;
    case USB_SETUP_GET_DESCRIPTOR:
        return ep0_get_descriptor_proc();
    case USB_SETUP_GET_CONFIGURATION:
        return ep0_get_config_proc();
    case USB_SETUP_SET_CONFIGURATION:
        return ep0_set_config_proc();
    case USB_SETUP_GET_INTERFACE:
        return ep0_get_iface_proc();
    case USB_SETUP_SET_INTERFACE:
        return ep0_set_iface_proc();
    default:
        break;
    }

    return FALSE;
}

static BOOL ep0_setup(void)
{
    switch (SETUPDAT[0] & bmSETUP_TYPE) {
    case bmSETUP_STANDARD:
        return ep0_std_proc();
    default:
        break;
    }

    return FALSE;
}

void hid_ep0_setup_task(void)
{
    if (!ep0_setup())
        usb_ep0_stall();

    usb_ep0_hsnack();
}
