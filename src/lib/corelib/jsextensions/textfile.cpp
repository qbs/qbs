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

#include "textfile.h"

#include <logging/translator.h>
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
    QScriptValue obj = engine->newQMetaObject(&TextFile::staticMetaObject,
                                              engine->newFunction(&TextFile::ctor));
    extensionObject.setProperty(QLatin1String("TextFile"), obj);
}

QScriptValue TextFile::ctor(QScriptContext *context, QScriptEngine *engine)
{
    TextFile *t;
    switch (context->argumentCount()) {
    case 0:
        return context->throwError(Tr::tr("TextFile constructor needs path of file to be opened."));
    case 1:
        t = new TextFile(context, context->argument(0).toString());
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
        return context->throwError(Tr::tr("TextFile constructor takes at most three parameters."));
    }

    return engine->newQObject(t, QScriptEngine::ScriptOwnership);
}

TextFile::~TextFile()
{
    delete m_stream;
    delete m_file;
}

TextFile::TextFile(QScriptContext *context, const QString &filePath, OpenMode mode,
                   const QString &codec)
{
    Q_UNUSED(codec)
    Q_ASSERT(thisObject().engine() == engine());

    m_file = new QFile(filePath);
    m_stream = new QTextStream(m_file);
    QIODevice::OpenMode m;
    switch (mode) {
    case ReadWrite:
        m = QIODevice::ReadWrite;
        break;
    case ReadOnly:
        m = QIODevice::ReadOnly;
        break;
    case WriteOnly:
        m = QIODevice::WriteOnly;
        break;
    }

    if (Q_UNLIKELY(!m_file->open(m))) {
        context->throwError(Tr::tr("Unable to open file '%1': %2")
                            .arg(filePath, m_file->errorString()));
        delete m_file;
        m_file = 0;
    }
}

void TextFile::close()
{
    if (checkForClosed())
        return;
    m_file->close();
    delete m_file;
    m_file = 0;
    delete m_stream;
    m_stream = 0;
}

void TextFile::setCodec(const QString &codec)
{
    if (checkForClosed())
        return;
    m_stream->setCodec(qPrintable(codec));
}

QString TextFile::readLine()
{
    if (checkForClosed())
        return QString();
    return m_stream->readLine();
}

QString TextFile::readAll()
{
    if (checkForClosed())
        return QString();
    return m_stream->readAll();
}

bool TextFile::atEof() const
{
    if (checkForClosed())
        return true;
    return m_stream->atEnd();
}

void TextFile::truncate()
{
    if (checkForClosed())
        return;
    m_file->resize(0);
    m_stream->reset();
}

void TextFile::write(const QString &str)
{
    if (checkForClosed())
        return;
    (*m_stream) << str;
}

void TextFile::writeLine(const QString &str)
{
    if (checkForClosed())
        return;
    (*m_stream) << str;
    if (HostOsInfo::isWindowsHost())
        (*m_stream) << '\r';
    (*m_stream) << '\n';
}

bool TextFile::checkForClosed() const
{
    if (m_file)
        return false;
    context()->throwError(Tr::tr("Access to TextFile object that was already closed."));
    return true;
}

} // namespace Internal
} // namespace qbs
