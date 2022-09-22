/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#include "deprecationwarningmode.h"

/*!
 * \enum DeprecationWarningMode
 * This enum type specifies how \QBS should behave on encountering deprecated items or properties.
 * \value DeprecationWarningMode::Error Project resolving will stop with an error message.
 * \value DeprecationWarningMode::On A warning will be printed.
 * \value DeprecationWarningMode::BeforeRemoval A warning will be printed if and only if this is
 *            the last \QBS version before the removal version. This is the default behavior.
 *            \note If the removal version's minor version number is zero, the behavior is
 *                  the same as for ErrorHandlingMode::On.
 * \value DeprecationWarningMode::Off No warnings will be emitted for deprecated constructs.
 */

namespace qbs {

DeprecationWarningMode defaultDeprecationWarningMode()
{
    return DeprecationWarningMode::BeforeRemoval;
}

QString deprecationWarningModeName(DeprecationWarningMode mode)
{
    switch (mode) {
    case DeprecationWarningMode::Error:
        return QStringLiteral("error");
    case DeprecationWarningMode::On:
        return QStringLiteral("on");
    case DeprecationWarningMode::BeforeRemoval:
        return QStringLiteral("before-removal");
    case DeprecationWarningMode::Off:
        return QStringLiteral("off");
    default:
        break;
    }
    return {};
}

DeprecationWarningMode deprecationWarningModeFromName(const QString &name)
{
    DeprecationWarningMode mode = defaultDeprecationWarningMode();
    for (int i = 0; i <= int(DeprecationWarningMode::Sentinel); ++i) {
        if (deprecationWarningModeName(static_cast<DeprecationWarningMode>(i)) == name) {
            mode = static_cast<DeprecationWarningMode>(i);
            break;
        }
    }
    return mode;
}

QStringList allDeprecationWarningModeStrings()
{
    QStringList result;
    for (int i = 0; i <= int(DeprecationWarningMode::Sentinel); ++i)
        result << deprecationWarningModeName(static_cast<DeprecationWarningMode>(i));
    return result;
}

} // namespace qbs
