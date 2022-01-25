/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QBS_SPAN_H
#define QBS_SPAN_H

#include <qcompilerdetection.h>

// The (Apple) Clang implementation of span is incomplete until LLVM 15 / Xcode 14.3 / macOS 13
#if __cplusplus >= 202002L \
    && !(defined(__apple_build_version__) && __apple_build_version__ < 14030022)
#include <span>

namespace qbs {
namespace Internal {
using std::as_bytes;
using std::as_writable_bytes;
using std::get;
using std::span;
} // namespace Internal
} // namespace qbs

#else
QT_WARNING_PUSH

#if defined(Q_CC_MSVC)
#pragma system_header
#elif defined(Q_CC_GNU) || defined(Q_CC_CLANG)
#pragma GCC system_header
#endif

// disable automatic usage of std::span in span-lite
// since we make that decision ourselves at the top of this header
#define span_CONFIG_SELECT_SPAN span_SPAN_NONSTD

#include <span.hpp>
namespace qbs {
namespace Internal {
using namespace nonstd;
} // namespace Internal
} // namespace qbs

QT_WARNING_POP
#endif

#endif // QBS_SPAN_H