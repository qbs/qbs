/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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

#include "fileinfoextension.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScriptable>
#include <QScriptEngine>

namespace qbs {
namespace Internal {

class FileInfoExtension : public QObject, QScriptable
{
    Q_OBJECT
public:
    friend void initializeJsExtensionFileInfo(QScriptValue extensionObject);

private:
    static QScriptValue js_ctor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_path(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_fileName(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_baseName(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_completeBaseName(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_relativePath(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_resolvePath(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_isAbsolutePath(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_toWindowsSeparators(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_fromWindowsSeparators(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_toNativeSeparators(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_fromNativeSeparators(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_joinPaths(QScriptContext *context, QScriptEngine *engine);
};

void initializeJsExtensionFileInfo(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue fileInfoObj = engine->newQMetaObject(&FileInfoExtension::staticMetaObject,
                                                  engine->newFunction(&FileInfoExtension::js_ctor));
    fileInfoObj.setProperty(QLatin1String("path"),
                            engine->newFunction(FileInfoExtension::js_path));
    fileInfoObj.setProperty(QLatin1String("fileName"),
                            engine->newFunction(FileInfoExtension::js_fileName));
    fileInfoObj.setProperty(QLatin1String("baseName"),
                            engine->newFunction(FileInfoExtension::js_baseName));
    fileInfoObj.setProperty(QLatin1String("completeBaseName"),
                            engine->newFunction(FileInfoExtension::js_completeBaseName));
    fileInfoObj.setProperty(QLatin1String("relativePath"),
                            engine->newFunction(FileInfoExtension::js_relativePath));
    fileInfoObj.setProperty(QLatin1String("resolvePath"),
                            engine->newFunction(FileInfoExtension::js_resolvePath));
    fileInfoObj.setProperty(QLatin1String("isAbsolutePath"),
                            engine->newFunction(FileInfoExtension::js_isAbsolutePath));
    fileInfoObj.setProperty(QLatin1String("toWindowsSeparators"),
                            engine->newFunction(FileInfoExtension::js_toWindowsSeparators));
    fileInfoObj.setProperty(QLatin1String("fromWindowsSeparators"),
                            engine->newFunction(FileInfoExtension::js_fromWindowsSeparators));
    fileInfoObj.setProperty(QLatin1String("toNativeSeparators"),
                            engine->newFunction(FileInfoExtension::js_toWindowsSeparators));
    fileInfoObj.setProperty(QLatin1String("fromNativeSeparators"),
                            engine->newFunction(FileInfoExtension::js_fromWindowsSeparators));
    fileInfoObj.setProperty(QLatin1String("joinPaths"),
                            engine->newFunction(FileInfoExtension::js_joinPaths));
    extensionObject.setProperty(QLatin1String("FileInfo"), fileInfoObj);
}

QScriptValue FileInfoExtension::js_ctor(QScriptContext *context, QScriptEngine *engine)
{
    // Add instance variables here etc., if needed.
    Q_UNUSED(context);
    Q_UNUSED(engine);
    return true;
}

QScriptValue FileInfoExtension::js_path(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("path expects 1 argument"));
    }
    HostOsInfo::HostOs hostOs = HostOsInfo::hostOs();
    if (context->argumentCount() > 1) {
        hostOs = context->argument(1).toVariant().toStringList().contains(QLatin1String("windows"))
                ? HostOsInfo::HostOsWindows : HostOsInfo::HostOsOtherUnix;
    }
    return FileInfo::path(context->argument(0).toString(), hostOs);
}

QScriptValue FileInfoExtension::js_fileName(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("fileName expects 1 argument"));
    }
    return FileInfo::fileName(context->argument(0).toString());
}

QScriptValue FileInfoExtension::js_baseName(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("baseName expects 1 argument"));
    }
    return FileInfo::baseName(context->argument(0).toString());
}

QScriptValue FileInfoExtension::js_completeBaseName(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("completeBaseName expects 1 argument"));
    }
    return FileInfo::completeBaseName(context->argument(0).toString());
}

QScriptValue FileInfoExtension::js_relativePath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("relativePath expects 2 arguments"));
    }
    const QString base = context->argument(0).toString();
    const QString rel = context->argument(1).toString();
    return QDir(base).relativeFilePath(rel);
}

QScriptValue FileInfoExtension::js_resolvePath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("resolvePath expects 2 arguments"));
    }
    const QString base = context->argument(0).toString();
    const QString rel = context->argument(1).toString();
    return FileInfo::resolvePath(base, rel);
}

QScriptValue FileInfoExtension::js_isAbsolutePath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("isAbsolutePath expects 1 argument"));
    }
    HostOsInfo::HostOs hostOs = HostOsInfo::hostOs();
    if (context->argumentCount() > 1) {
        hostOs = context->argument(1).toVariant().toStringList().contains(QLatin1String("windows"))
                ? HostOsInfo::HostOsWindows : HostOsInfo::HostOsOtherUnix;
    }
    return FileInfo::isAbsolute(context->argument(0).toString(), hostOs);
}

QScriptValue FileInfoExtension::js_toWindowsSeparators(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("toWindowsSeparators expects 1 argument"));
    }
    return context->argument(0).toString().replace(QLatin1Char('/'), QLatin1Char('\\'));
}

QScriptValue FileInfoExtension::js_fromWindowsSeparators(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("fromWindowsSeparators expects 1 argument"));
    }
    return context->argument(0).toString().replace(QLatin1Char('\\'), QLatin1Char('/'));
}

QScriptValue FileInfoExtension::js_toNativeSeparators(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("toNativeSeparators expects 1 argument"));
    }
    return QDir::toNativeSeparators(context->argument(0).toString());
}

QScriptValue FileInfoExtension::js_fromNativeSeparators(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("fromNativeSeparators expects 1 argument"));
    }
    return QDir::fromNativeSeparators(context->argument(0).toString());
}

QScriptValue FileInfoExtension::js_joinPaths(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    QStringList paths;
    for (int i = 0; i < context->argumentCount(); ++i) {
        const QScriptValue value = context->argument(i);
        if (!value.isUndefined() && !value.isNull()) {
            const QString arg = value.toString();
            if (!arg.isEmpty())
                paths.append(arg);
        }
    }
    return paths.join(QLatin1Char('/')).replace(QRegularExpression(QLatin1String("/{2,}")),
                                                QLatin1String("/"));
}

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::FileInfoExtension *)

#include "fileinfoextension.moc"
