/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "qbssettings.h"

#include <tools/scripttools.h>

#include <QScriptEngine>
#include <QScriptValue>

using qbs::toJSLiteral;

static QString mapToString(const QVariantMap &vm)
{
    QString str = QLatin1String("{");
    for (QVariantMap::const_iterator it = vm.begin(); it != vm.end(); ++it) {
        if (it != vm.begin())
            str += QLatin1Char(',');
        if (it.value().type() == QVariant::Map) {
            str += toJSLiteral(it.key()) + QLatin1Char(':');
            str += mapToString(it.value().toMap());
        } else {
            str += toJSLiteral(it.key()) + QLatin1Char(':') + toJSLiteral(it.value());
        }
    }
    str += QLatin1Char('}');
    return str;
}

QString settingsValueToRepresentation(const QVariant &value)
{
    if (value.type() == QVariant::Bool)
        return QLatin1String(value.toBool() ? "true" : "false");
    if (value.type() == QVariant::Map)
        return mapToString(value.toMap());
    return value.toStringList().join(QLatin1String(","));
}

static QVariantMap mapFromString(const QString &str)
{
    // ### use Qt5's JSON reader at some point.
    QScriptEngine engine;
    QScriptValue sv = engine.evaluate(QLatin1String("(function(){return ")
                                      + str + QLatin1String(";})()"));
    if (sv.isError())
        return QVariantMap();
    return sv.toVariant().toMap();
}

QVariant representationToSettingsValue(const QString &representation)
{
    if (representation == QLatin1String("true"))
        return QVariant(true);
    if (representation == QLatin1String("false"))
        return QVariant(false);
    if (representation.startsWith(QLatin1Char('{')))
        return mapFromString(representation);
    const QStringList list = representation.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (list.count() > 1)
        return list;
    return representation;
}
