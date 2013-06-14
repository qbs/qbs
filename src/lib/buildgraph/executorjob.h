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

#ifndef QBS_EXECUTORJOB_H
#define QBS_EXECUTORJOB_H

#include <language/forward_decls.h>
#include <tools/error.h>

#include <QObject>

QT_BEGIN_NAMESPACE
class QScriptEngine;
QT_END_NAMESPACE

namespace qbs {
class CodeLocation;
class ProcessResult;

namespace Internal {
class AbstractCommandExecutor;
class ProductBuildData;
class JsCommandExecutor;
class Logger;
class ProcessCommandExecutor;
class Transformer;

class ExecutorJob : public QObject
{
    Q_OBJECT
public:
    ExecutorJob(const Logger &logger, QObject *parent);
    ~ExecutorJob();

    void setMainThreadScriptEngine(QScriptEngine *engine);
    void setDryRun(bool enabled);
    void run(Transformer *t, const ResolvedProductPtr &product);
    void cancel();
    void waitForFinished();

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void reportProcessResult(const qbs::ProcessResult &result);
    void error(const qbs::ErrorInfo &error);
    void success();

private slots:
    void runNextCommand();
    void onCommandError(const qbs::ErrorInfo &err);
    void onCommandFinished();

private:
    void setInactive();

    AbstractCommandExecutor *m_currentCommandExecutor;
    ProcessCommandExecutor *m_processCommandExecutor;
    JsCommandExecutor *m_jsCommandExecutor;
    Transformer *m_transformer;
    int m_currentCommandIdx;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EXECUTORJOB_H
