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
#include "hid.h"
#include "usb.h"

void usb_init(void)
{
    // Disable all USB interrupts.
    USBIE = 0;
    sync_delay();
    // Disable all end point interrupts.
    EPIE = 0;
    sync_delay();
    // Disable all end point ping-nak interrupts.
    NAKIE = 0;
    sync_delay();
    // Disable all USB error interrupts.
    USBERRIE = 0;
    sync_delay();
    // Disable USB && GPIF autovectoring.
    INTSETUP = 0;
    sync_delay();
    // Clear USB interrupt requests.
    USBIRQ = bmSUDAV | bmSOF | bmSUTOK | bmSUSP
            | bmURES | bmHSGRANT | bmEP0ACK;
    sync_delay();
    // Clear end point interrupt requests.
    EPIRQ = bmEP0IN | bmEP0OUT | bmEP1IN | bmEP1OUT
            | bmEP2 | bmEP4 | bmEP6 | bmEP8;
    sync_delay();
    // Set USB interrupt to high priority.
    PUSB = 1;
    sync_delay();
    // Enable USB interrupts.
    EUSB = 1;
    sync_delay();
    // As we use SOFs to detect disconnect we have
    // to disable SOF synthesizing.
    USBCS |= bmNOSYNSOF;
    sync_delay();

    hid_init();

    // Disable FX2-internal enumeration support.
    USBCS |= bmRENUM;
    sync_delay();
}

void usb_task(void)
{
    if (USBIRQ & bmSUDAV) {
        USBIRQ = bmSUDAV;
        hid_ep0_setup_task();
    }

    if (USBIRQ & bmEP0ACK) {
        USBIRQ = bmEP0ACK;
    }

    if (USBIRQ & bmURES) {
        USBIRQ = bmURES;
    }

    if (USBIRQ & bmSUSP) {
        USBIRQ = bmSUSP;
    }

    hid_ep1_task();
}
