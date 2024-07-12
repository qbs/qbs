/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#if defined(WITH_ZLIB)
#include <zlib.h>
#endif

#ifdef WITH_PTHREAD
#include <pthread.h>
#elif defined(WITH_SETUPAPI)
#include <windows.h>
#include <Setupapi.h>
#endif

void b();
void c();

int d()
{
    b();
    c();
    int result = 0;
#if defined(WITH_ZLIB)
    const char source[] = "Hello, world";
    uLongf buffer_size = compressBound(sizeof(source));
    result += static_cast<int>(buffer_size);
#endif

#ifdef WITH_PTHREAD
    pthread_t self = pthread_self();
    result += static_cast<int>(self);
#elif defined(WITH_SETUPAPI)
    CABINET_INFO ci;
    ci.SetId = 0;
    SetupIterateCabinet(L"invalid-file-path", 0, NULL, NULL);
    result += ci.SetId;
#endif
    return result;
}
