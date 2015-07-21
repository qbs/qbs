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

#include "file.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>

#include <QDir>
#include <QFileInfo>
#include <QScriptEngine>

namespace qbs {
namespace Internal {

class File
{
    friend void initializeJsExtensionFile(QScriptValue extensionObject);

private:
    static QScriptValue js_ctor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_copy(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_exists(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_lastModified(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_makePath(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_remove(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_canonicalFilePath(QScriptContext *context, QScriptEngine *engine);
};


void initializeJsExtensionFile(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue fileObj = engine->newFunction(File::js_ctor);
    fileObj.setProperty(QLatin1String("copy"), engine->newFunction(File::js_copy));
    fileObj.setProperty(QLatin1String("exists"), engine->newFunction(File::js_exists));
    fileObj.setProperty(QLatin1String("lastModified"), engine->newFunction(File::js_lastModified));
    fileObj.setProperty(QLatin1String("makePath"), engine->newFunction(File::js_makePath));
    fileObj.setProperty(QLatin1String("remove"), engine->newFunction(File::js_remove));
    fileObj.setProperty(QLatin1String("canonicalFilePath"),
                        engine->newFunction(File::js_canonicalFilePath));
    extensionObject.setProperty(QLatin1String("File"), fileObj);
}

QScriptValue File::js_ctor(QScriptContext *context, QScriptEngine *engine)
{
    // Add instance variables here etc., if needed.
    Q_UNUSED(context);
    Q_UNUSED(engine);
    return true;
}

QScriptValue File::js_copy(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 2)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("copy expects 2 arguments"));
    }

    const QString sourceFile = context->argument(0).toString();
    const QString targetFile = context->argument(1).toString();
    QString errorMessage;
    if (Q_UNLIKELY(!copyFileRecursion(sourceFile, targetFile, false, &errorMessage)))
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

QScriptValue File::js_remove(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("remove expects 1 argument"));
    }
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
    return static_cast<qsreal>(timestamp.m_fileTime);
}

QScriptValue File::js_makePath(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   Tr::tr("makePath expects 1 argument"));
    }
    return QDir::root().mkpath(context->argument(0).toString());
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
