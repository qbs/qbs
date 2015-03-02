/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "scripttools.h"

#include <QDataStream>
#include <QScriptEngine>
#include <QScriptValueIterator>

QT_BEGIN_NAMESPACE

QDataStream &operator<< (QDataStream &s, const QScriptProgram &script)
{
    s << script.sourceCode()
      << script.fileName()
      << script.firstLineNumber();
    return s;
}

QDataStream &operator>> (QDataStream &s, QScriptProgram &script)
{
    QString fileName, sourceCode;
    int lineNumber;
    s >> sourceCode
      >> fileName
      >> lineNumber;
    script = QScriptProgram(sourceCode, fileName, lineNumber);
    return s;
}

QT_END_NAMESPACE

namespace qbs {

QString toJSLiteral(const bool b)
{
    return b ? QLatin1String("true") : QLatin1String("false");
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
    QString js = QLatin1String("[");
    for (int i = 0; i < strs.count(); ++i) {
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
        return QLatin1String("undefined");
    if (val.type() == QVariant::List || val.type() == QVariant::StringList) {
        QString res;
        foreach (const QVariant &child, val.toList()) {
            if (res.length()) res.append(QLatin1String(", "));
            res.append(toJSLiteral(child));
        }
        res.prepend(QLatin1Char('['));
        res.append(QLatin1Char(']'));
        return res;
    }
    if (val.type() == QVariant::Map) {
        const QVariantMap &vm = val.toMap();
        QString str = QLatin1String("{");
        for (QVariantMap::const_iterator it = vm.begin(); it != vm.end(); ++it) {
            if (it != vm.begin())
                str += QLatin1Char(',');
            str += toJSLiteral(it.key()) + QLatin1Char(':') + toJSLiteral(it.value());
        }
        str += QLatin1Char('}');
        return str;
    }
    if (val.type() == QVariant::Bool)
        return val.toBool() ? QLatin1String("true") : QLatin1String("false");
    if (val.canConvert(QVariant::String))
        return toJSLiteral(val.toString());
    return QString::fromLatin1("Unconvertible type %1").arg(QLatin1String(val.typeName()));
}

namespace Internal {

void setConfigProperty(QVariantMap &cfg, const QStringList &name, const QVariant &value)
{
    if (name.length() == 1) {
        cfg.insert(name.first(), value);
    } else {
        QVariant &subCfg = cfg[name.first()];
        QVariantMap subCfgMap = subCfg.toMap();
        setConfigProperty(subCfgMap, name.mid(1), value);
        subCfg = subCfgMap;
    }
}

QVariant getConfigProperty(const QVariantMap &cfg, const QStringList &name)
{
    if (name.length() == 1)
        return cfg.value(name.first());
    else
        return getConfigProperty(cfg.value(name.first()).toMap(), name.mid(1));
}

} // namespace Internal
} // namespace qbs
