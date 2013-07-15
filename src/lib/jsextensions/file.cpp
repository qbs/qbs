/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include "file.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/fileinfo.h>

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
    static QScriptValue js_remove(QScriptContext *context, QScriptEngine *engine);
};


void initializeJsExtensionFile(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue fileObj = engine->newFunction(File::js_ctor);
    fileObj.setProperty("copy", engine->newFunction(File::js_copy));
    fileObj.setProperty("exists", engine->newFunction(File::js_exists));
    fileObj.setProperty("remove", engine->newFunction(File::js_remove));
    extensionObject.setProperty("File", fileObj);
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

} // namespace Internal
} // namespace qbs
