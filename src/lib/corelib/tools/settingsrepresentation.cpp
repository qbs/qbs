/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "settingsrepresentation.h"

#include "jsliterals.h"

#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>

namespace qbs {

QString settingsValueToRepresentation(const QVariant &value)
{
    return toJSLiteral(value);
}

static QVariant variantFromString(const QString &str, bool &ok)
{
    // ### use Qt5's JSON reader at some point.
    QScriptEngine engine;
    QScriptValue sv = engine.evaluate(QLatin1String("(function(){return ")
                                      + str + QLatin1String(";})()"));
    ok = !sv.isError();
    return sv.toVariant();
}

QVariant representationToSettingsValue(const QString &representation)
{
    bool ok;
    QVariant variant = variantFromString(representation, ok);

    // We have no floating-point properties, so this is most likely intended to be a string.
    if (static_cast<QMetaType::Type>(variant.type()) == QMetaType::Float
            || static_cast<QMetaType::Type>(variant.type()) == QMetaType::Double) {
        variant = variantFromString(QLatin1Char('"') + representation + QLatin1Char('"'), ok);
    }

    if (ok)
        return variant;

    // If it's not valid JavaScript, interpret the value as a string.
    return representation;
}

} // namespace qbs
