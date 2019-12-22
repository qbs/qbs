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

#ifndef QBS_EXECUTORJOB_H
#define QBS_EXECUTORJOB_H

#include <language/forward_decls.h>
#include <tools/commandechomode.h>
#include <tools/error.h>
#include <tools/set.h>

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

namespace qbs {
class CodeLocation;
class ProcessResult;

namespace Internal {
class AbstractCommandExecutor;
class ProductBuildData;
class JsCommandExecutor;
class Logger;
class ProcessCommandExecutor;
class ScriptEngine;
class Transformer;

class ExecutorJob : public QObject
{
    Q_OBJECT
public:
    explicit ExecutorJob(const Logger &logger, QObject *parent = nullptr);
    ~ExecutorJob() override;

    void setMainThreadScriptEngine(ScriptEngine *engine);
    void setDryRun(bool enabled);
    void setEchoMode(CommandEchoMode echoMode);
    void run(Transformer *t);
    void cancel();
    const Transformer *transformer() const { return m_transformer; }
    Set<QString> jobPools() const { return m_jobPools; }

signals:
    void reportCommandDescription(const QString &highlight, const QString &message);
    void reportProcessResult(const qbs::ProcessResult &result);
    void finished(const qbs::ErrorInfo &error = ErrorInfo()); // !hasError() <=> command successful

private:
    void runNextCommand();
    void onCommandFinished(const qbs::ErrorInfo &err);

    void setFinished();
    void reset();

    AbstractCommandExecutor *m_currentCommandExecutor = nullptr;
    ProcessCommandExecutor *m_processCommandExecutor = nullptr;
    JsCommandExecutor *m_jsCommandExecutor = nullptr;
    Transformer *m_transformer = nullptr;
    Set<QString> m_jobPools;
    int m_currentCommandIdx = 0;
    ErrorInfo m_error;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_EXECUTORJOB_H
