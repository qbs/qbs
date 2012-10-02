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

#ifndef TEXTFILE_H
#define TEXTFILE_H

#include <QObject>
#include <QScriptable>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QFile;
class QTextStream;
QT_END_NAMESPACE

class TextFile : public QObject, public QScriptable
{
    Q_OBJECT
    Q_ENUMS(OpenMode)
public:
    static void init(QScriptValue &extensionObject);

    enum OpenMode { ReadOnly, WriteOnly, ReadWrite };
    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);
    TextFile(QScriptContext *context, const QString &file, OpenMode mode = ReadOnly, const QString &codec = QLatin1String("UTF8"));
    ~TextFile();
    Q_INVOKABLE void close();
    Q_INVOKABLE void setCodec(const QString &codec);
    Q_INVOKABLE QString readLine();
    Q_INVOKABLE QString readAll();
    Q_INVOKABLE bool atEof() const;
    Q_INVOKABLE void truncate();
    Q_INVOKABLE void write(const QString &str);
    Q_INVOKABLE void writeLine(const QString &str);
private:
    QFile *qfile;
    QTextStream *qstream;
};

Q_DECLARE_METATYPE(TextFile *);

#endif // TEXTFILE_H
