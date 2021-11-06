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

#include "qttools.h"
#include "porting.h"

#include <QtCore/qprocess.h>

namespace std {

size_t hash<QVariant>::operator()(const QVariant &v) const noexcept
{
    switch (v.userType()) {
    case QMetaType::UnknownType: return 0;
    case QMetaType::Bool: return std::hash<bool>()(v.toBool());
    case QMetaType::Int: return std::hash<int>()(v.toInt());
    case QMetaType::UInt: return std::hash<uint>()(v.toUInt());
    case QMetaType::QString: return std::hash<QString>()(v.toString());
    case QMetaType::QStringList: return std::hash<QStringList>()(v.toStringList());
    case QMetaType::QVariantList: return std::hash<QVariantList>()(v.toList());
    case QMetaType::QVariantMap: return std::hash<QVariantMap>()(v.toMap());
    case QMetaType::QVariantHash: return std::hash<QVariantHash>()(v.toHash());
    default:
        QBS_ASSERT("Unsupported variant type" && false, return 0);
    }
}

} // namespace std

QT_BEGIN_NAMESPACE

qbs::QHashValueType qHash(const QStringList &list)
{
    qbs::QHashValueType s = 0;
    for (const QString &n : list)
        s ^= qHash(n) + 0x9e3779b9 + (s << 6) + (s >> 2);
    return s;
}

qbs::QHashValueType qHash(const QProcessEnvironment &env)
{
    return qHash(env.toStringList());
}

QT_END_NAMESPACE
