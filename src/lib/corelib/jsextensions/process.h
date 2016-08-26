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

#ifndef QBS_PROCESS_H
#define QBS_PROCESS_H

#include <logging/logger.h>

#include <QObject>
#include <QProcessEnvironment>
#include <QScriptable>
#include <QVariant>

QT_BEGIN_NAMESPACE
class QProcess;
class QTextStream;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
void initializeJsExtensionProcess(QScriptValue extensionObject);

class Process : public QObject, public QScriptable
{
    Q_OBJECT
public:
    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);
    Process(QScriptContext *context);
    ~Process();

    Q_INVOKABLE QString getEnv(const QString &name);
    Q_INVOKABLE void setEnv(const QString &name, const QString &value);
    Q_INVOKABLE void setCodec(const QString &codec);

    Q_INVOKABLE QString workingDirectory();
    Q_INVOKABLE void setWorkingDirectory(const QString &dir);

    Q_INVOKABLE bool start(const QString &program, const QStringList &arguments);
    Q_INVOKABLE int exec(const QString &program, const QStringList &arguments,
                         bool throwOnError = false);
    Q_INVOKABLE void close();
    Q_INVOKABLE bool waitForFinished(int msecs = 30000);
    Q_INVOKABLE void terminate();
    Q_INVOKABLE void kill();

    Q_INVOKABLE QString readLine();
    Q_INVOKABLE QString readStdOut();
    Q_INVOKABLE QString readStdErr();

    Q_INVOKABLE void closeWriteChannel();

    Q_INVOKABLE void write(const QString &str);
    Q_INVOKABLE void writeLine(const QString &str);

    Q_INVOKABLE int exitCode() const;

    static QScriptValue js_shellQuote(QScriptContext *context, QScriptEngine *engine);

private:
    Logger logger() const;
    QString findExecutable(const QString &filePath) const;

    QProcess *m_qProcess;
    QProcessEnvironment m_environment;
    QString m_workingDirectory;
    QTextStream *m_textStream;
};

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::Process *)

#endif // QBS_PROCESS_H
