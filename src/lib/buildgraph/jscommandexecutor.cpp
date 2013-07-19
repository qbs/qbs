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

#include "jscommandexecutor.h"

#include "artifact.h"
#include "buildgraph.h"
#include "command.h"
#include "transformer.h"

#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <tools/codelocation.h>
#include <tools/error.h>

#include <QDir>
#include <QEventLoop>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

namespace qbs {
namespace Internal {

struct JavaScriptCommandResult
{
    bool success;
    QString errorMessage;
    CodeLocation errorLocation;
};

class JsCommandExecutorThreadObject : public QObject
{
    Q_OBJECT
public:
    JsCommandExecutorThreadObject(const Logger &logger)
        : m_logger(logger)
    {
    }

    const JavaScriptCommandResult &result() const
    {
        return m_result;
    }

signals:
    void finished();

public slots:
    void start(const JavaScriptCommand *cmd, Transformer *transformer)
    {
        m_result.success = true;
        m_result.errorMessage.clear();
        ScriptEngine * const scriptEngine = lookupEngine();
        QString trafoPtrStr = QString::number((qulonglong)transformer);
        if (scriptEngine->globalObject().property("_qbs_transformer_ptr").toString() != trafoPtrStr) {
            scriptEngine->globalObject().setProperty("_qbs_transformer_ptr", scriptEngine->toScriptValue(trafoPtrStr));

            Artifact *someOutputArtifact = *transformer->outputs.begin();
            const ResolvedProductConstPtr product = someOutputArtifact->product.toStrongRef();
            if (product) {
                setupScriptEngineForProduct(scriptEngine, product, transformer->rule,
                                            scriptEngine->globalObject());
            }
            transformer->setupInputs(scriptEngine, scriptEngine->globalObject());
            transformer->setupOutputs(scriptEngine, scriptEngine->globalObject());
        }

        scriptEngine->pushContext();
        for (QVariantMap::const_iterator it = cmd->properties().constBegin();
                it != cmd->properties().constEnd(); ++it) {
            scriptEngine->currentContext()->activationObject().setProperty(it.key(),
                    scriptEngine->toScriptValue(it.value()));
        }

        scriptEngine->evaluate(cmd->sourceCode());
        if (scriptEngine->hasUncaughtException()) {
            m_result.success = false;
            m_result.errorMessage = scriptEngine->uncaughtException().toString();
            const CodeLocation &origLocation = cmd->codeLocation();
            m_result.errorLocation = CodeLocation(origLocation.fileName(),
                    origLocation.line() + scriptEngine->uncaughtExceptionLineNumber(),
                    origLocation.column());
        }
        scriptEngine->popContext();
        scriptEngine->clearExceptions();
        emit finished();
    }

private:
    ScriptEngine *lookupEngine()
    {
        QThread * const currentThread = QThread::currentThread();
        QMutexLocker locker(&m_cacheMutex);
        ScriptEngine * scriptEngine = m_enginesPerThread.value(currentThread);
        if (!scriptEngine) {
            scriptEngine = new ScriptEngine(m_logger);
            m_enginesPerThread.insert(currentThread, scriptEngine);
        } else {
            scriptEngine->setLogger(m_logger);
        }
        return scriptEngine;
    }

    static QHash<QThread *, ScriptEngine *> m_enginesPerThread;
    static QMutex m_cacheMutex;
    Logger m_logger;
    JavaScriptCommandResult m_result;
};

QHash<QThread *, ScriptEngine *> JsCommandExecutorThreadObject::m_enginesPerThread;
QMutex JsCommandExecutorThreadObject::m_cacheMutex;


JsCommandExecutor::JsCommandExecutor(const Logger &logger, QObject *parent)
    : AbstractCommandExecutor(logger, parent)
    , m_thread(new QThread(this))
    , m_objectInThread(new JsCommandExecutorThreadObject(logger))
    , m_running(false)
{
    m_objectInThread->moveToThread(m_thread);
    connect(m_objectInThread, SIGNAL(finished()), this, SLOT(onJavaScriptCommandFinished()));
    connect(this, SIGNAL(startRequested(const JavaScriptCommand *, Transformer *)),
            m_objectInThread, SLOT(start(const JavaScriptCommand *, Transformer *)));
}

JsCommandExecutor::~JsCommandExecutor()
{
    waitForFinished();
    delete m_objectInThread;
    m_thread->quit();
    m_thread->wait();
}

void JsCommandExecutor::waitForFinished()
{
    if (!m_running)
        return;
    QEventLoop loop;
    loop.connect(m_objectInThread, SIGNAL(finished()), SLOT(quit()));
    loop.exec();
}

void JsCommandExecutor::doStart()
{
    QBS_ASSERT(!m_running, return);
    m_thread->start();

    if (dryRun()) {
        QTimer::singleShot(0, this, SIGNAL(finished())); // Don't call back on the caller.
        return;
    }

    m_running = true;
    emit startRequested(jsCommand(), transformer());
}

void JsCommandExecutor::onJavaScriptCommandFinished()
{
    m_running = false;
    const JavaScriptCommandResult &result = m_objectInThread->result();
    if (!result.success) {
        logger().qbsDebug() << "JS context:\n" << jsCommand()->properties();
        logger().qbsDebug() << "JS code:\n" << jsCommand()->sourceCode();
        QString msg = "Error while executing JavaScriptCommand:\n";
        msg += result.errorMessage;
        emit error(ErrorInfo(msg, result.errorLocation));
    }
    emit finished();
}

const JavaScriptCommand *JsCommandExecutor::jsCommand() const
{
    return static_cast<const JavaScriptCommand *>(command());
}

} // namespace Internal
} // namespace qbs

#include <jscommandexecutor.moc>
