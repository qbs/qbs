/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef ABSTRACTCOMMANDEXECUTOR_H
#define ABSTRACTCOMMANDEXECUTOR_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {
namespace Internal {
class AbstractCommand;
class Transformer;

class AbstractCommandExecutor : public QObject
{
    Q_OBJECT
public:
    explicit AbstractCommandExecutor(QObject *parent = 0);

    void setMainThreadScriptEngine(QScriptEngine *engine) { m_mainThreadScriptEngine = engine; }
    void setDryRunEnabled(bool enabled) { m_dryRun = enabled; }

    virtual void waitForFinished() = 0;

public slots:
    void start(Transformer *transformer, const AbstractCommand *cmd);

signals:
    void error(QString errorString);
    void finished();

protected:
    const AbstractCommand *command() const { return m_command; }
    Transformer *transformer() const { return m_transformer; }
    QScriptEngine *scriptEngine() const { return m_mainThreadScriptEngine; }

private:
    virtual void doStart() = 0;
    void printCommandInfo();

private:
    const AbstractCommand *m_command;
    Transformer *m_transformer;
    QScriptEngine *m_mainThreadScriptEngine;
    bool m_dryRun; // TODO: Use
};

} // namespace Internal
} // namespace qbs

#endif // ABSTRACTCOMMANDEXECUTOR_H
