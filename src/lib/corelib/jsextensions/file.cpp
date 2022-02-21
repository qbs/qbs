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

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

namespace qbs {
namespace Internal {

class File : public JsExtension<File>
{
public:
    enum Filter {
        Dirs = 0x001,
        Files = 0x002,
        Drives = 0x004,
        NoSymLinks = 0x008,
        AllEntries = Dirs | Files | Drives,
        TypeMask = 0x00f,

        Readable = 0x010,
        Writable = 0x020,
        Executable = 0x040,
        PermissionMask = 0x070,

        Modified = 0x080,
        Hidden = 0x100,
        System = 0x200,

        AccessMask  = 0x3F0,

        AllDirs = 0x400,
        CaseSensitive = 0x800,
        NoDot = 0x2000,
        NoDotDot = 0x4000,
        NoDotAndDotDot = NoDot | NoDotDot,

        NoFilter = -1
    };
    Q_DECLARE_FLAGS(Filters, Filter)

    static const char *name() { return "File"; }
    static void setupStaticMethods(JSContext *ctx, JSValue classObj);
    static void declareEnums(JSContext *ctx, JSValue classObj);
    static JSValue jsCopy(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsExists(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsDirectoryEntries(JSContext *ctx, JSValueConst,
                                      int argc, JSValueConst *argv);
    static JSValue jsLastModified(JSContext *ctx, JSValueConst,
                                  int argc, JSValueConst *argv);
    static JSValue jsMakePath(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsMove(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsRemove(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsCanonicalFilePath(JSContext *ctx, JSValueConst,
                                       int argc, JSValueConst *argv);
};

void File::setupStaticMethods(JSContext *ctx, JSValue classObj)
{
    setupMethod(ctx, classObj, "copy", &File::jsCopy, 2);
    setupMethod(ctx, classObj, "exists", &File::jsExists, 1);
    setupMethod(ctx, classObj, "directoryEntries", File::jsDirectoryEntries, 2);
    setupMethod(ctx, classObj, "lastModified", File::jsLastModified, 1);
    setupMethod(ctx, classObj, "makePath", &File::jsMakePath, 1);
    setupMethod(ctx, classObj, "move", &File::jsMove, 3);
    setupMethod(ctx, classObj, "remove", &File::jsRemove, 1);
    setupMethod(ctx, classObj, "canonicalFilePath", &File::jsCanonicalFilePath, 1);
}

void File::declareEnums(JSContext *ctx, JSValue classObj)
{
    DECLARE_ENUM(ctx, classObj, Dirs);
    DECLARE_ENUM(ctx, classObj, Files);
    DECLARE_ENUM(ctx, classObj, Drives);
    DECLARE_ENUM(ctx, classObj, NoSymLinks);
    DECLARE_ENUM(ctx, classObj, AllEntries);
    DECLARE_ENUM(ctx, classObj, TypeMask);
    DECLARE_ENUM(ctx, classObj, Readable);
    DECLARE_ENUM(ctx, classObj, Writable);
    DECLARE_ENUM(ctx, classObj, Executable);
    DECLARE_ENUM(ctx, classObj, PermissionMask);
    DECLARE_ENUM(ctx, classObj, Modified);
    DECLARE_ENUM(ctx, classObj, Hidden);
    DECLARE_ENUM(ctx, classObj, System);
    DECLARE_ENUM(ctx, classObj, AccessMask);
    DECLARE_ENUM(ctx, classObj, AllDirs);
    DECLARE_ENUM(ctx, classObj, CaseSensitive);
    DECLARE_ENUM(ctx, classObj, NoDot);
    DECLARE_ENUM(ctx, classObj, NoDotDot);
    DECLARE_ENUM(ctx, classObj, NoDotAndDotDot);
    DECLARE_ENUM(ctx, classObj, NoFilter);
}

JSValue File::jsCopy(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts({
                DubiousContext(EvalContext::PropertyEvaluation),
                DubiousContext(EvalContext::RuleExecution, DubiousContext::SuggestMoving)
        });
        se->checkContext(QStringLiteral("File.copy()"), dubiousContexts);

        const auto args = getArguments<QString, QString>(ctx, "File.copy", argc, argv);
        QString errorMessage;
        if (Q_UNLIKELY(!copyFileRecursion(std::get<0>(args), std::get<1>(args), true, true,
                                          &errorMessage))) {
            throw errorMessage;
        }
        return JS_TRUE;
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue File::jsExists(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "File.exists", argc, argv);
        const bool exists = FileInfo::exists(filePath);
        ScriptEngine::engineForContext(ctx)->addFileExistsResult(filePath, exists);
        return JS_NewBool(ctx, exists);
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue File::jsDirectoryEntries(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts({
                DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
        });
        se->checkContext(QStringLiteral("File.directoryEntries()"), dubiousContexts);

        const auto args = getArguments<QString, qint32>(ctx, "Environment.directoryEntries",
                                                        argc, argv);
        const QString path = std::get<0>(args);
        const QDir dir(path);
        const auto filters = static_cast<QDir::Filters>(std::get<1>(args));
        const QStringList entries = dir.entryList(filters, QDir::Name);
        se->addDirectoryEntriesResult(path, filters, entries);
        return makeJsStringList(ctx, entries);
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue File::jsRemove(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts{DubiousContext(EvalContext::PropertyEvaluation)};
        se->checkContext(QStringLiteral("File.remove()"), dubiousContexts);
        const auto fileName = getArgument<QString>(ctx, "Environment.remove", argc, argv);
        QString errorMessage;
        if (Q_UNLIKELY(!removeFileRecursion(QFileInfo(fileName), &errorMessage)))
            throw errorMessage;
        return JS_TRUE;
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue File::jsLastModified(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "Environment.lastModified", argc, argv);
        const FileTime timestamp = FileInfo(filePath).lastModified();
        const auto se = ScriptEngine::engineForContext(ctx);
        se->addFileLastModifiedResult(filePath, timestamp);
        return JS_NewFloat64(ctx, timestamp.asDouble());
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue File::jsMakePath(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts{DubiousContext(EvalContext::PropertyEvaluation)};
        se->checkContext(QStringLiteral("File.makePath()"), dubiousContexts);
        const auto path = getArgument<QString>(ctx, "File.makePath", argc, argv);
        return JS_NewBool(ctx, QDir::root().mkpath(path));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue File::jsMove(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts{DubiousContext(EvalContext::PropertyEvaluation)};
        se->checkContext(QStringLiteral("File.move()"), dubiousContexts);

        const auto args = getArguments<QString, QString>(ctx, "File.move", argc, argv);
        const QString sourceFile = std::get<0>(args);
        const QString targetFile = std::get<1>(args);
        const auto overwrite = argc > 2 ? fromArg<bool>(ctx, "File.move", 3, argv[2]) : true;

        if (Q_UNLIKELY(QFileInfo(sourceFile).isDir())) {
            throw QStringLiteral("Could not move '%1' to '%2': "
                                 "Source file path is a directory.").arg(sourceFile, targetFile);
        }
        if (Q_UNLIKELY(QFileInfo(targetFile).isDir())) {
            throw QStringLiteral("Could not move '%1' to '%2': "
                                 "Destination file path is a directory.")
                    .arg(sourceFile, targetFile);
        }

        QFile f(targetFile);
        if (overwrite && f.exists() && !f.remove()) {
            throw QStringLiteral("Could not move '%1' to '%2': %3")
                .arg(sourceFile, targetFile, f.errorString());
        }
        if (QFile::exists(targetFile)) {
            throw QStringLiteral("Could not move '%1' to '%2': "
                                 "Destination file exists.").arg(sourceFile, targetFile);
        }

        QFile f2(sourceFile);
        if (Q_UNLIKELY(!f2.rename(targetFile))) {
            throw QStringLiteral("Could not move '%1' to '%2': %3")
                .arg(sourceFile, targetFile, f2.errorString());
        }
        return JS_TRUE;
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue File::jsCanonicalFilePath(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto path = getArgument<QString>(ctx, "File.canonicalFilePath", argc, argv);
        return makeJsString(ctx, QFileInfo(path).canonicalFilePath());
    } catch (const QString &error) { return throwError(ctx, error); }
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionFile(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::File::registerClass(engine, extensionObject);
}
