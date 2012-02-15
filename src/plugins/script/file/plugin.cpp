/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "file.h"
#include "textfile.h"

#include <QtScript/QScriptExtensionPlugin>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptEngine>

void qtscript_initialize_com_nokia_qbs_fileapi_bindings(QScriptValue &);

class com_nokia_qbs_fileapi_ScriptPlugin : public QScriptExtensionPlugin
{
public:
    QStringList keys() const;
    void initialize(const QString &key, QScriptEngine *engine);
};

QStringList com_nokia_qbs_fileapi_ScriptPlugin::keys() const
{
    QStringList list;
    list << QLatin1String("qbs");
    list << QLatin1String("qbs.fileapi");
    return list;
}

void com_nokia_qbs_fileapi_ScriptPlugin::initialize(const QString &key, QScriptEngine *engine)
{
    if (key == QLatin1String("qbs")) {
    } else if (key == QLatin1String("qbs.fileapi")) {
        QScriptValue extensionObject = engine->globalObject();
        File::init(extensionObject, engine);
        TextFile::init(extensionObject, engine);
    } else {
        Q_ASSERT_X(false, "qbs.fileapi::initialize", qPrintable(key));
    }
}

Q_EXPORT_STATIC_PLUGIN(com_nokia_qbs_fileapi_ScriptPlugin)
Q_EXPORT_PLUGIN2(qtscript_com_nokia_qbs_fileapi, com_nokia_qbs_fileapi_ScriptPlugin)
