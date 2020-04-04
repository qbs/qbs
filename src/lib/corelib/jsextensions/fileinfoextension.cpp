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

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>

#include <regex>

namespace qbs {
namespace Internal {

// removes duplicate separators from the path
static QString uniqueSeparators(QString path)
{
    const auto it = std::unique(path.begin(), path.end(), [](QChar c1, QChar c2) {
        return c1 == c2 && c1 == QLatin1Char('/');
    });
    path.resize(int(it - path.begin()));
    return path;
}

class FileInfoExtension : public QObject, QScriptable
{
    Q_OBJECT
public:
    static QScriptValue js_ctor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_path(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_fileName(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_baseName(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_suffix(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_completeSuffix(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_canonicalPath(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_cleanPath(QScriptContext *context, QScriptEngine *engine);
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

QScriptValue FileInfoExtension::js_ctor(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    return context->throwError(Tr::tr("'FileInfo' cannot be instantiated."));
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

QScriptValue FileInfoExtension::js_suffix(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("suffix expects 1 argument"));
    }
    return FileInfo::suffix(context->argument(0).toString());
}

QScriptValue FileInfoExtension::js_completeSuffix(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("completeSuffix expects 1 argument"));
    }
    return FileInfo::completeSuffix(context->argument(0).toString());
}

QScriptValue FileInfoExtension::js_canonicalPath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("canonicalPath expects 1 argument"));
    }
    return QFileInfo(context->argument(0).toString()).canonicalFilePath();
}

QScriptValue FileInfoExtension::js_cleanPath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("cleanPath expects 1 argument"));
    }
    return QDir::cleanPath(context->argument(0).toString());
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
    const QString baseDir = context->argument(0).toString();
    const QString filePath = context->argument(1).toString();
    if (!FileInfo::isAbsolute(baseDir)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("FileInfo.relativePath() expects an absolute path as "
                                          "its first argument, but it is '%1'.").arg(baseDir));
    }
    if (!FileInfo::isAbsolute(filePath)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("FileInfo.relativePath() expects an absolute path as "
                                          "its second argument, but it is '%1'.").arg(filePath));
    }
    return QDir(baseDir).relativeFilePath(filePath);
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
                paths.push_back(arg);
        }
    }
    return engine->toScriptValue(uniqueSeparators(paths.join(QLatin1Char('/'))));
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionFileInfo(QScriptValue extensionObject)
{
    using namespace qbs::Internal;
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue fileInfoObj = engine->newQMetaObject(&FileInfoExtension::staticMetaObject,
                                                  engine->newFunction(&FileInfoExtension::js_ctor));
    fileInfoObj.setProperty(StringConstants::fileInfoPath(),
                            engine->newFunction(FileInfoExtension::js_path));
    fileInfoObj.setProperty(StringConstants::fileInfoFileName(),
                            engine->newFunction(FileInfoExtension::js_fileName));
    fileInfoObj.setProperty(StringConstants::baseNameProperty(),
                            engine->newFunction(FileInfoExtension::js_baseName));
    fileInfoObj.setProperty(QStringLiteral("suffix"),
                            engine->newFunction(FileInfoExtension::js_suffix));
    fileInfoObj.setProperty(QStringLiteral("completeSuffix"),
                            engine->newFunction(FileInfoExtension::js_completeSuffix));
    fileInfoObj.setProperty(QStringLiteral("canonicalPath"),
                            engine->newFunction(FileInfoExtension::js_canonicalPath));
    fileInfoObj.setProperty(QStringLiteral("cleanPath"),
                            engine->newFunction(FileInfoExtension::js_cleanPath));
    fileInfoObj.setProperty(StringConstants::completeBaseNameProperty(),
                            engine->newFunction(FileInfoExtension::js_completeBaseName));
    fileInfoObj.setProperty(QStringLiteral("relativePath"),
                            engine->newFunction(FileInfoExtension::js_relativePath));
    fileInfoObj.setProperty(QStringLiteral("resolvePath"),
                            engine->newFunction(FileInfoExtension::js_resolvePath));
    fileInfoObj.setProperty(QStringLiteral("isAbsolutePath"),
                            engine->newFunction(FileInfoExtension::js_isAbsolutePath));
    fileInfoObj.setProperty(QStringLiteral("toWindowsSeparators"),
                            engine->newFunction(FileInfoExtension::js_toWindowsSeparators));
    fileInfoObj.setProperty(QStringLiteral("fromWindowsSeparators"),
                            engine->newFunction(FileInfoExtension::js_fromWindowsSeparators));
    fileInfoObj.setProperty(QStringLiteral("toNativeSeparators"),
                            engine->newFunction(FileInfoExtension::js_toNativeSeparators));
    fileInfoObj.setProperty(QStringLiteral("fromNativeSeparators"),
                            engine->newFunction(FileInfoExtension::js_fromNativeSeparators));
    fileInfoObj.setProperty(QStringLiteral("joinPaths"),
                            engine->newFunction(FileInfoExtension::js_joinPaths));
    extensionObject.setProperty(QStringLiteral("FileInfo"), fileInfoObj);
}

Q_DECLARE_METATYPE(qbs::Internal::FileInfoExtension *)

#include "fileinfoextension.moc"
