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

#ifndef QBS_TEXTFILE_H
#define QBS_TEXTFILE_H

#include <QObject>
#include <QScriptable>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QFile;
class QTextStream;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {

void initializeJsExtensionTextFile(QScriptValue extensionObject);

class TextFile : public QObject, public QScriptable
{
    Q_OBJECT
    Q_ENUMS(OpenMode)
public:
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

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::TextFile *)

#endif // QBS_TEXTFILE_H
