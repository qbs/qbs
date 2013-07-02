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

#include "textfile.h"

#include <tools/hostosinfo.h>

#include <QFile>
#include <QScriptEngine>
#include <QScriptValue>
#include <QTextStream>

namespace qbs {
namespace Internal {

void initializeJsExtensionTextFile(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue obj = engine->newQMetaObject(&TextFile::staticMetaObject, engine->newFunction(&TextFile::ctor));
    extensionObject.setProperty("TextFile", obj);
}

QScriptValue TextFile::ctor(QScriptContext *context, QScriptEngine *engine)
{
    TextFile *t;
    switch (context->argumentCount()) {
        case 1:
            t = new TextFile(context,
                    context->argument(0).toString());
            break;
        case 2:
            t = new TextFile(context,
                    context->argument(0).toString(),
                    static_cast<OpenMode>(context->argument(1).toInt32())
                    );
            break;
        case 3:
            t = new TextFile(context,
                    context->argument(0).toString(),
                    static_cast<OpenMode>(context->argument(1).toInt32()),
                    context->argument(2).toString()
                    );
            break;
        default:
            return context->throwError("TextFile(QString file, OpenMode mode = ReadOnly, QString codec = QLatin1String(\"UTF8\"))");
    }

    QScriptValue obj = engine->newQObject(t, QScriptEngine::ScriptOwnership);
//    obj.setProperty("d", engine->newQObject(new FileImplementation(t),
//                QScriptEngine::QScriptEngine::QtOwnership));
    return obj;
}

TextFile::~TextFile()
{
    delete qstream;
    delete qfile;
}

TextFile::TextFile(QScriptContext *context, const QString &file, OpenMode mode, const QString &codec)
{
    Q_UNUSED(codec)
    Q_ASSERT(thisObject().engine() == engine());
    TextFile *t = this;

    t->qfile = new QFile(file);
    QIODevice::OpenMode m = QIODevice::ReadOnly;
    if (mode == ReadWrite) {
        m = QIODevice::ReadWrite;
    } else if (mode == ReadOnly) {
        m = QIODevice::ReadOnly;
    } else if (mode == WriteOnly) {
       m = QIODevice::WriteOnly;
    }
    if (Q_UNLIKELY(!t->qfile->open(m))) {
        delete t->qfile;
        t->qfile = 0;
        context->throwError(QString::fromLatin1("unable to open '%1'")
                .arg(file)
                );
    }

   t->qstream = new QTextStream(t->qfile);
}

void TextFile::close()
{
    Q_ASSERT(thisObject().engine() == engine());
    TextFile *t = qscriptvalue_cast<TextFile*>(thisObject());
    if (t->qfile)
        t->qfile->close();
    delete t->qfile;
    t->qfile = 0;
    delete t->qstream;
    t->qstream = 0;
}

void TextFile::setCodec(const QString &codec)
{
    Q_ASSERT(thisObject().engine() == engine());
    TextFile *t = qscriptvalue_cast<TextFile*>(thisObject());
    if (!t->qstream)
        return;
    t->qstream->setCodec(qPrintable(codec));
}

QString TextFile::readLine()
{
    TextFile *t = qscriptvalue_cast<TextFile*>(thisObject());
    if (!t->qfile)
        return QString();
    return t->qstream->readLine();
}

QString TextFile::readAll()
{
    TextFile *t = qscriptvalue_cast<TextFile*>(thisObject());
    if (!t->qfile)
        return QString();
    return t->qstream->readAll();
}

bool TextFile::atEof() const
{
    TextFile *t = qscriptvalue_cast<TextFile*>(thisObject());
    if (!t->qstream)
        return true;
    return t->qstream->atEnd();
}

void TextFile::truncate()
{
    TextFile *t = qscriptvalue_cast<TextFile*>(thisObject());
    if (!t->qstream)
        return;
    t->qfile->resize(0);
    t->qstream->reset();
}

void TextFile::write(const QString &str)
{
    TextFile *t = qscriptvalue_cast<TextFile*>(thisObject());
    if (!t->qstream)
        return;
    (*t->qstream) << str;
}

void TextFile::writeLine(const QString &str)
{
    TextFile *t = qscriptvalue_cast<TextFile*>(thisObject());
    if (!t->qstream)
        return;
    (*t->qstream) << str;
    if (HostOsInfo::isWindowsHost())
        (*t->qstream) << '\r';
    (*t->qstream) << '\n';
}

} // namespace Internal
} // namespace qbs
