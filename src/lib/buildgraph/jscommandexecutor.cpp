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

#include <jsextensions/file.h>
#include <jsextensions/process.h>
#include <jsextensions/textfile.h>
#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/logger.h>
#include <tools/codelocation.h>
#include <tools/error.h>

#include <QtConcurrentRun>
#include <QDir>
#include <QFutureWatcher>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

namespace qbs {
namespace Internal {

struct JavaScriptCommandFutureResult
{
    bool success;
    QString errorMessage;
    CodeLocation errorLocation;
};

class JavaScriptCommandFutureWatcher : public QFutureWatcher<JavaScriptCommandFutureResult>
{
public:
    JavaScriptCommandFutureWatcher(QObject *parent)
        : QFutureWatcher<JavaScriptCommandFutureResult>(parent)
    {}
};

class JSRunner
{
public:
    typedef JavaScriptCommandFutureResult result_type;

    JSRunner(const JavaScriptCommand *jsCommand, const Logger &logger)
        : m_jsCommand(jsCommand), m_logger(logger) {}

    JavaScriptCommandFutureResult operator() (Transformer *transformer)
    {
        result_type result;
        result.success = true;
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
        for (QVariantMap::const_iterator it = m_jsCommand->properties().constBegin(); it != m_jsCommand->properties().constEnd(); ++it)
            scriptEngine->currentContext()->activationObject().setProperty(it.key(), scriptEngine->toScriptValue(it.value()));

        scriptEngine->evaluate(m_jsCommand->sourceCode());
        if (scriptEngine->hasUncaughtException()) {
            result.success = false;
            result.errorMessage = scriptEngine->uncaughtException().toString();
            const CodeLocation &origLocation = m_jsCommand->codeLocation();
            result.errorLocation = CodeLocation(origLocation.fileName(),
                    origLocation.line() + scriptEngine->uncaughtExceptionLineNumber(),
                    origLocation.column());
        }
        scriptEngine->popContext();
        scriptEngine->clearExceptions();
        return result;
    }

    ScriptEngine *lookupEngine()
    {
        QThread * const currentThread = QThread::currentThread();
        QMutexLocker locker(&m_cacheMutex);
        ScriptEngine * scriptEngine = m_enginesPerThread.value(currentThread);
        if (!scriptEngine) {
            scriptEngine = new ScriptEngine(m_logger);
            const QScriptValue extensionObject = scriptEngine->globalObject();
            File::init(extensionObject);
            TextFile::init(extensionObject);
            Process::init(extensionObject);
            m_enginesPerThread.insert(currentThread, scriptEngine);
        } else {
            scriptEngine->setLogger(m_logger);
        }
        return scriptEngine;
    }

private:
    static QHash<QThread *, ScriptEngine *> m_enginesPerThread;
    static QMutex m_cacheMutex;
    const JavaScriptCommand *m_jsCommand;
    Logger m_logger;
};

QHash<QThread *, ScriptEngine *> JSRunner::m_enginesPerThread;
QMutex JSRunner::m_cacheMutex;


JsCommandExecutor::JsCommandExecutor(const Logger &logger, QObject *parent)
    : AbstractCommandExecutor(logger, parent)
    , m_jsFutureWatcher(0)
{
}

void JsCommandExecutor::waitForFinished()
{
    if (m_jsFutureWatcher && m_jsFutureWatcher->isRunning())
        m_jsFutureWatcher->waitForFinished();
}

void JsCommandExecutor::doStart()
{
    if (dryRun()) {
        QTimer::singleShot(0, this, SIGNAL(finished())); // Don't call back on the caller.
        return;
    }
    QFuture<JSRunner::result_type> future
            = QtConcurrent::run(JSRunner(jsCommand(), logger()), transformer());
    if (!m_jsFutureWatcher) {
        m_jsFutureWatcher = new JavaScriptCommandFutureWatcher(this);
        connect(m_jsFutureWatcher, SIGNAL(finished()), SLOT(onJavaScriptCommandFinished()));
    }
    m_jsFutureWatcher->setFuture(future);
}

void JsCommandExecutor::onJavaScriptCommandFinished()
{
    JavaScriptCommandFutureResult result = m_jsFutureWatcher->future().result();
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
