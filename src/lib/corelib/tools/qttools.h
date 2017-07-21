/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QBSQTTOOLS_H
#define QBSQTTOOLS_H

#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>

QT_BEGIN_NAMESPACE
uint qHash(const QStringList &list);

#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
template <typename T1, typename T2> inline uint qHash(const std::pair<T1, T2> &key, uint seed = 0)
    Q_DECL_NOEXCEPT_EXPR(noexcept(qHash(key.first, seed)) && noexcept(qHash(key.second, seed)))
{
    QtPrivate::QHashCombine hash;
    seed = hash(seed, key.first);
    seed = hash(seed, key.second);
    return seed;
}

namespace QtPrivate {
template <typename T> struct QAddConst { typedef const T Type; };
}

// this adds const to non-const objects (like std::as_const)
template <typename T>
Q_DECL_CONSTEXPR typename QtPrivate::QAddConst<T>::Type &qAsConst(T &t) Q_DECL_NOTHROW { return t; }
// prevent rvalue arguments:
template <typename T>
void qAsConst(const T &&) Q_DECL_EQ_DELETE;
#endif // Qt Version < 5.7.0
QT_END_NAMESPACE

#ifndef Q_FALLTHROUGH
#ifndef QT_HAS_CPP_ATTRIBUTE
#ifdef __has_cpp_attribute
#  define QT_HAS_CPP_ATTRIBUTE(x)       __has_cpp_attribute(x)
#else
#  define QT_HAS_CPP_ATTRIBUTE(x)       0
#endif
#endif
#if defined(__cplusplus)
#if QT_HAS_CPP_ATTRIBUTE(fallthrough)
#  define Q_FALLTHROUGH() [[fallthrough]]
#elif QT_HAS_CPP_ATTRIBUTE(clang::fallthrough)
#    define Q_FALLTHROUGH() [[clang::fallthrough]]
#elif QT_HAS_CPP_ATTRIBUTE(gnu::fallthrough)
#    define Q_FALLTHROUGH() [[gnu::fallthrough]]
#endif
#endif
#ifndef Q_FALLTHROUGH
#  if (defined(Q_CC_GNU) && Q_CC_GNU >= 700) && !defined(Q_CC_INTEL)
#    define Q_FALLTHROUGH() __attribute__((fallthrough))
#  else
#    define Q_FALLTHROUGH() (void)0
#endif
#endif
#endif


#endif // QBSQTTOOLS_H
