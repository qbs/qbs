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

#include "jsliterals.h"

#include <tools/stringconstants.h>

#include <QtCore/qregexp.h>

namespace qbs {

QString toJSLiteral(const bool b)
{
    return b ? Internal::StringConstants::trueValue() : Internal::StringConstants::falseValue();
}

QString toJSLiteral(const QString &str)
{
    QString js = str;
    js.replace(QRegExp(QLatin1String("([\\\\\"])")), QLatin1String("\\\\1"));
    js.prepend(QLatin1Char('"'));
    js.append(QLatin1Char('"'));
    return js;
}

QString toJSLiteral(const QStringList &strs)
{
    QString js = QStringLiteral("[");
    for (int i = 0; i < strs.size(); ++i) {
        if (i != 0)
            js.append(QLatin1String(", "));
        js.append(toJSLiteral(strs.at(i)));
    }
    js.append(QLatin1Char(']'));
    return js;
}

QString toJSLiteral(const QVariant &val)
{
    if (!val.isValid())
        return Internal::StringConstants::undefinedValue();
    if (val.type() == QVariant::List || val.type() == QVariant::StringList) {
        QString res;
        const auto list = val.toList();
        for (const QVariant &child : list) {
            if (res.length()) res.append(QLatin1String(", "));
            res.append(toJSLiteral(child));
        }
        res.prepend(QLatin1Char('['));
        res.append(QLatin1Char(']'));
        return res;
    }
    if (val.type() == QVariant::Map) {
        const QVariantMap &vm = val.toMap();
        QString str = QStringLiteral("{");
        for (QVariantMap::const_iterator it = vm.begin(); it != vm.end(); ++it) {
            if (it != vm.begin())
                str += QLatin1Char(',');
            str += toJSLiteral(it.key()) + QLatin1Char(':') + toJSLiteral(it.value());
        }
        str += QLatin1Char('}');
        return str;
    }
    if (val.type() == QVariant::Bool)
        return toJSLiteral(val.toBool());
    if (val.canConvert(QVariant::String))
        return toJSLiteral(val.toString());
    return QStringLiteral("Unconvertible type %1").arg(QLatin1String(val.typeName()));
}

} // namespace qbs
