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

#include "jsextension.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

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

class FileInfoExtension : public JsExtension<FileInfoExtension>
{
public:
    static const char *name() { return "FileInfo"; }
    static void setupStaticMethods(JSContext *ctx, JSValue classObj);
    static JSValue jsPath(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsFileName(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsBaseName(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsSuffix(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsCompleteSuffix(JSContext *ctx, JSValueConst,
                                    int argc, JSValueConst *argv);
    static JSValue jsCanonicalPath(JSContext *ctx, JSValueConst,
                                   int argc, JSValueConst *argv);
    static JSValue jsCleanPath(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsCompleteBaseName(JSContext *ctx, JSValueConst,
                                      int argc, JSValueConst *argv);
    static JSValue jsRelativePath(JSContext *ctx, JSValueConst,
                                  int argc, JSValueConst *argv);
    static JSValue jsResolvePath(JSContext *ctx, JSValueConst,
                                 int argc, JSValueConst *argv);
    static JSValue jsIsAbsolutePath(JSContext *ctx, JSValueConst,
                                    int argc, JSValueConst *argv);
    static JSValue jsToWindowsSeparators(JSContext *ctx, JSValueConst,
                                         int argc, JSValueConst *argv);
    static JSValue jsFromWindowsSeparators(JSContext *ctx, JSValueConst,
                                           int argc, JSValueConst *argv);
    static JSValue jsToNativeSeparators(JSContext *ctx, JSValueConst,
                                        int argc, JSValueConst *argv);
    static JSValue jsFromNativeSeparators(JSContext *ctx, JSValueConst,
                                          int argc, JSValueConst *argv);
    static JSValue jsJoinPaths(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsPathListSeparator(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsPathSeparator(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue jsExecutableSuffix(JSContext *ctx, JSValueConst, int, JSValueConst *);
};

void FileInfoExtension::setupStaticMethods(JSContext *ctx, JSValue classObj)
{
    setupMethod(ctx, classObj, StringConstants::fileInfoPath(), &FileInfoExtension::jsPath, 1);
    setupMethod(ctx, classObj, StringConstants::fileInfoFileName(),
                      &FileInfoExtension::jsFileName, 1);
    setupMethod(ctx, classObj, StringConstants::baseNameProperty(),
                      &FileInfoExtension::jsBaseName, 1);
    setupMethod(ctx, classObj, "suffix", &FileInfoExtension::jsSuffix, 1);
    setupMethod(ctx, classObj, "completeSuffix", &FileInfoExtension::jsCompleteSuffix, 1);
    setupMethod(ctx, classObj, "canonicalPath", &FileInfoExtension::jsCanonicalPath, 1);
    setupMethod(ctx, classObj, "cleanPath", &FileInfoExtension::jsCleanPath, 1);
    setupMethod(ctx, classObj, StringConstants::completeBaseNameProperty(),
                      &FileInfoExtension::jsCompleteBaseName, 1);
    setupMethod(ctx, classObj, "relativePath", &FileInfoExtension::jsRelativePath, 1);
    setupMethod(ctx, classObj, "resolvePath", &FileInfoExtension::jsResolvePath, 1);
    setupMethod(ctx, classObj, "isAbsolutePath", &FileInfoExtension::jsIsAbsolutePath, 1);
    setupMethod(ctx, classObj, "toWindowsSeparators",
                      &FileInfoExtension::jsToWindowsSeparators, 1);
    setupMethod(ctx, classObj, "fromWindowsSeparators",
                      &FileInfoExtension::jsFromWindowsSeparators, 1);
    setupMethod(ctx, classObj, "toNativeSeparators",
                      &FileInfoExtension::jsToNativeSeparators, 1);
    setupMethod(ctx, classObj, "fromNativeSeparators",
                      &FileInfoExtension::jsFromNativeSeparators, 1);
    setupMethod(ctx, classObj, "joinPaths", &FileInfoExtension::jsJoinPaths, 0);
    setupMethod(ctx, classObj, "pathListSeparator",
                      &FileInfoExtension::jsPathListSeparator, 0);
    setupMethod(ctx, classObj, "pathSeparator", &FileInfoExtension::jsPathSeparator, 0);
    setupMethod(ctx, classObj, "executableSuffix", &FileInfoExtension::jsExecutableSuffix, 0);
}

JSValue FileInfoExtension::jsPath(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.path", argc, argv);
        HostOsInfo::HostOs hostOs = HostOsInfo::hostOs();
        if (argc > 1) {
            const auto osList = fromArg<QStringList>(ctx, "FileInfo.path", 2, argv[1]);
            hostOs = osList.contains(QLatin1String("windows"))
                    ? HostOsInfo::HostOsWindows : HostOsInfo::HostOsOtherUnix;
        }
        return makeJsString(ctx, FileInfo::path(filePath, hostOs));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsFileName(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.fileName", argc, argv);
        return makeJsString(ctx, FileInfo::fileName(filePath));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsBaseName(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.baseName", argc, argv);
        return makeJsString(ctx, FileInfo::baseName(filePath));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsSuffix(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.suffix", argc, argv);
        return makeJsString(ctx, FileInfo::suffix(filePath));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsCompleteSuffix(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.completeSuffix", argc, argv);
        return makeJsString(ctx, FileInfo::completeSuffix(filePath));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsCanonicalPath(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.canonicalPath", argc, argv);
        return makeJsString(ctx, QFileInfo(filePath).canonicalFilePath());
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsCleanPath(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.cleanPath", argc, argv);
        return makeJsString(ctx, QDir::cleanPath(filePath));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsCompleteBaseName(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.completeBaseName", argc, argv);
        return makeJsString(ctx, FileInfo::completeBaseName(filePath));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsRelativePath(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto args = getArguments<QString, QString>(ctx, "File.relativePath", argc, argv);
        const QString baseDir = std::get<0>(args);
        const QString filePath = std::get<1>(args);
        if (!FileInfo::isAbsolute(baseDir)) {
            throw Tr::tr("FileInfo.relativePath() expects an absolute path as "
                         "its first argument, but it is '%1'.").arg(baseDir);
        }
        if (!FileInfo::isAbsolute(filePath)) {
            throw Tr::tr("FileInfo.relativePath() expects an absolute path as "
                         "its second argument, but it is '%1'.").arg(filePath);
        }
        return makeJsString(ctx, QDir(baseDir).relativeFilePath(filePath));

    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsResolvePath(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto args = getArguments<QString, QString>(ctx, "File.resolvePath", argc, argv);
        const QString base = std::get<0>(args);
        const QString rel = std::get<1>(args);
        return makeJsString(ctx, FileInfo::resolvePath(base, rel));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsIsAbsolutePath(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.isAbsolutePath", argc, argv);
        HostOsInfo::HostOs hostOs = HostOsInfo::hostOs();
        if (argc > 1) {
            const auto osList = fromArg<QStringList>(ctx, "FileInfo.isAbsolutePath", 2, argv[1]);
            hostOs = osList.contains(QLatin1String("windows"))
                    ? HostOsInfo::HostOsWindows : HostOsInfo::HostOsOtherUnix;
        }
        return JS_NewBool(ctx, FileInfo::isAbsolute(filePath, hostOs));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsToWindowsSeparators(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        auto filePath = getArgument<QString>(ctx, "FileInfo.toWindowsSeparators", argc, argv);
        return makeJsString(ctx, filePath.replace(QLatin1Char('/'), QLatin1Char('\\')));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsFromWindowsSeparators(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        auto filePath = getArgument<QString>(ctx, "FileInfo.fromWindowsSeparators", argc, argv);
        return makeJsString(ctx, filePath.replace(QLatin1Char('\\'), QLatin1Char('/')));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsToNativeSeparators(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.toWindowsSeparators", argc, argv);
        return makeJsString(ctx, QDir::toNativeSeparators(filePath));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsFromNativeSeparators(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "FileInfo.fromWindowsSeparators",
                                                   argc, argv);
        return makeJsString(ctx, QDir::fromNativeSeparators(filePath));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsJoinPaths(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        QStringList paths;
        for (int i = 0; i < argc; ++i) {
            const auto value = fromArg<QString>(ctx, "FileInfo.joinPaths", i + 1, argv[i]);
            if (!value.isEmpty())
                paths.push_back(value);
        }
        return makeJsString(ctx, uniqueSeparators(paths.join(QLatin1Char('/'))));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue FileInfoExtension::jsPathListSeparator(JSContext *ctx, JSValue, int, JSValue *)
{
    return makeJsString(ctx, QString(HostOsInfo::pathListSeparator()));
}

JSValue FileInfoExtension::jsPathSeparator(JSContext *ctx, JSValue, int, JSValue *)
{
    return makeJsString(ctx, QString(HostOsInfo::pathSeparator()));
}

JSValue FileInfoExtension::jsExecutableSuffix(JSContext *ctx, JSValue, int, JSValue *)
{
    static QString executableSuffix = HostOsInfo::isWindowsHost() ?
                QLatin1String(QBS_HOST_EXE_SUFFIX) : QString();
    return makeJsString(ctx, executableSuffix);
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionFileInfo(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::FileInfoExtension::registerClass(engine, extensionObject);
}
