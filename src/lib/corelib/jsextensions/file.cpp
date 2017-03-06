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

#include "jsextensions_p.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>

namespace qbs {
namespace Internal {

class File : public QObject, QScriptable
{
    Q_OBJECT
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
    Q_ENUMS(Filter)

    static QScriptValue js_ctor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_copy(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_exists(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_directoryEntries(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_lastModified(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_makePath(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_move(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_remove(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_canonicalFilePath(QScriptContext *context, QScriptEngine *engine);
};

static void initializeJsExtensionFile(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue fileObj = engine->newQMetaObject(&File::staticMetaObject,
                                                  engine->newFunction(&File::js_ctor));
    fileObj.setProperty(QLatin1String("copy"), engine->newFunction(File::js_copy));
    fileObj.setProperty(QLatin1String("exists"), engine->newFunction(File::js_exists));
    fileObj.setProperty(QLatin1String("directoryEntries"),
                        engine->newFunction(File::js_directoryEntries));
    fileObj.setProperty(QLatin1String("lastModified"), engine->newFunction(File::js_lastModified));
    fileObj.setProperty(QLatin1String("makePath"), engine->newFunction(File::js_makePath));
    fileObj.setProperty(QLatin1String("move"), engine->newFunction(File::js_move));
    fileObj.setProperty(QLatin1String("remove"), engine->newFunction(File::js_remove));
    fileObj.setProperty(QLatin1String("canonicalFilePath"),
                        engine->newFunction(File::js_canonicalFilePath));
    extensionObject.setProperty(QLatin1String("File"), fileObj);
}

QBS_JSEXTENSION_REGISTER(File, &initializeJsExtensionFile)

QScriptValue File::js_ctor(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    return context->throwError(Tr::tr("'File' cannot be instantiated."));
}

QScriptValue File::js_copy(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 2)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("copy expects 2 arguments"));
    }

    ScriptEngine * const se = static_cast<ScriptEngine *>(engine);
    const DubiousContextList dubiousContexts({
            DubiousContext(EvalContext::PropertyEvaluation),
            DubiousContext(EvalContext::RuleExecution, DubiousContext::SuggestMoving)
    });
    se->checkContext(QLatin1String("File.copy()"), dubiousContexts);

    const QString sourceFile = context->argument(0).toString();
    const QString targetFile = context->argument(1).toString();
    QString errorMessage;
    if (Q_UNLIKELY(!copyFileRecursion(sourceFile, targetFile, true, true, &errorMessage)))
        return context->throwError(errorMessage);
    return true;
}

QScriptValue File::js_exists(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("exist expects 1 argument"));
    }
    const QString filePath = context->argument(0).toString();
    const bool exists = FileInfo::exists(filePath);
    ScriptEngine * const se = static_cast<ScriptEngine *>(engine);
    se->addFileExistsResult(filePath, exists);
    return exists;
}

QScriptValue File::js_directoryEntries(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 2)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("directoryEntries expects 2 arguments"));
    }

    ScriptEngine * const se = static_cast<ScriptEngine *>(engine);
    const DubiousContextList dubiousContexts({
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
    });
    se->checkContext(QLatin1String("File.directoryEntries()"), dubiousContexts);

    const QString path = context->argument(0).toString();
    const QDir::Filters filters = static_cast<QDir::Filters>(context->argument(1).toUInt32());
    QDir dir(path);
    const QStringList entries = dir.entryList(filters, QDir::Name);
    se->addDirectoryEntriesResult(path, filters, entries);
    return qScriptValueFromSequence(engine, entries);
}

QScriptValue File::js_remove(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("remove expects 1 argument"));
    }

    ScriptEngine * const se = static_cast<ScriptEngine *>(engine);
    const DubiousContextList dubiousContexts({ DubiousContext(EvalContext::PropertyEvaluation) });
    se->checkContext(QLatin1String("File.remove()"), dubiousContexts);

    QString fileName = context->argument(0).toString();

    QString errorMessage;
    if (Q_UNLIKELY(!removeFileRecursion(QFileInfo(fileName), &errorMessage)))
        return context->throwError(errorMessage);
    return true;
}

QScriptValue File::js_lastModified(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("File.lastModified() expects an argument"));
    }
    const QString filePath = context->argument(0).toString();
    const FileTime timestamp = FileInfo(filePath).lastModified();
    ScriptEngine * const se = static_cast<ScriptEngine *>(engine);
    se->addFileLastModifiedResult(filePath, timestamp);
    return timestamp.asDouble();
}

QScriptValue File::js_makePath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("makePath expects 1 argument"));
    }

    ScriptEngine * const se = static_cast<ScriptEngine *>(engine);
    const DubiousContextList dubiousContexts({ DubiousContext(EvalContext::PropertyEvaluation) });
    se->checkContext(QLatin1String("File.makePath()"), dubiousContexts);

    return QDir::root().mkpath(context->argument(0).toString());
}

QScriptValue File::js_move(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 2)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("move expects 2 arguments"));
    }

    ScriptEngine * const se = static_cast<ScriptEngine *>(engine);
    const DubiousContextList dubiousContexts({ DubiousContext(EvalContext::PropertyEvaluation) });
    se->checkContext(QLatin1String("File.move()"), dubiousContexts);

    const QString sourceFile = context->argument(0).toString();
    const QString targetFile = context->argument(1).toString();
    const bool overwrite = context->argumentCount() > 2 ? context->argument(2).toBool() : true;

    if (Q_UNLIKELY(QFileInfo(sourceFile).isDir()))
        return context->throwError(QString(QLatin1String("Could not move '%1' to '%2': "
                                                         "Source file path is a directory."))
                                   .arg(sourceFile, targetFile));

    if (Q_UNLIKELY(QFileInfo(targetFile).isDir())) {
        return context->throwError(QString(QLatin1String("Could not move '%1' to '%2': "
                                                         "Destination file path is a directory."))
                                   .arg(sourceFile, targetFile));
    }

    if (!QFile(sourceFile).isReadable())
        return context->throwError(QString(QLatin1String("Could not move '%1' to '%2': "
                                                         "Source file is not accessible."))
                                   .arg(sourceFile, targetFile));

    QFile f(targetFile);
    if (overwrite && !f.remove())
        return context->throwError(QString(QLatin1String("Could not move '%1' to '%2': %3"))
                                   .arg(sourceFile, targetFile, f.errorString()));

    if (QFile::exists(targetFile))
        return context->throwError(QString(QLatin1String("Could not move '%1' to '%2': "
                                                         "Destination file exists."))
                                   .arg(sourceFile, targetFile));

    QFile f2(sourceFile);
    if (Q_UNLIKELY(!f2.rename(targetFile)))
        return context->throwError(QString(QLatin1String("Could not move '%1' to '%2': %3"))
                                   .arg(sourceFile, targetFile, f2.errorString()));
    return true;
}

QScriptValue File::js_canonicalFilePath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("canonicalFilePath expects 1 argument"));
    }
    return QFileInfo(context->argument(0).toString()).canonicalFilePath();
}

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::File *)

#include "file.moc"
