/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2019 Jochen Ulrich <jochenulrich@t-online.de>
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

#ifndef QBS_PROCESSCOMMANDEXECUTOR_H
#define QBS_PROCESSCOMMANDEXECUTOR_H

#include "abstractcommandexecutor.h"

#include <tools/qbsprocess.h>

#include <QtCore/qstring.h>

namespace qbs {
class ProcessResult;

namespace Internal {
class ProcessCommand;

class ProcessCommandExecutor : public AbstractCommandExecutor
{
    Q_OBJECT
public:
    explicit ProcessCommandExecutor(const Internal::Logger &logger, QObject *parent = nullptr);

    void setProcessEnvironment(const QProcessEnvironment &processEnvironment) {
        m_buildEnvironment = processEnvironment;
    }

signals:
    void reportProcessResult(const qbs::ProcessResult &result);

private:
    void onProcessError();
    void onProcessFinished();

    void doSetup() override;
    void doReportCommandDescription(const QString &productName) override;
    bool doStart() override;
    void cancel(const qbs::ErrorInfo &reason) override;

    void startProcessCommand();
    QString filterProcessOutput(const QByteArray &output, const QString &filterFunctionSource);
    void getProcessOutput(bool stdOut, ProcessResult &result);

    void sendProcessOutput();
    void removeResponseFile();
    ProcessCommand *processCommand() const;

private:
    QString m_program;
    QStringList m_arguments;
    QString m_shellInvocation;

    QbsProcess m_process;
    QProcessEnvironment m_buildEnvironment;
    QProcessEnvironment m_commandEnvironment;
    QString m_responseFileName;
    qbs::ErrorInfo m_cancelReason;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_PROCESSCOMMANDEXECUTOR_H
