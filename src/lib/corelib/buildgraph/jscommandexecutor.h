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

#ifndef QBS_JSCOMMANDEXECUTOR_H
#define QBS_JSCOMMANDEXECUTOR_H

#include "abstractcommandexecutor.h"

#include <QtCore/qstring.h>

namespace qbs {
class CodeLocation;

namespace Internal {
class JavaScriptCommand;
class JsCommandExecutorThreadObject;

class JsCommandExecutor : public AbstractCommandExecutor
{
    Q_OBJECT
public:
    explicit JsCommandExecutor(const Logger &logger, QObject *parent = nullptr);
    ~JsCommandExecutor() override;

signals:
    void startRequested(const JavaScriptCommand *cmd, Transformer *transformer);

private:
    void onJavaScriptCommandFinished();

    void doReportCommandDescription(const QString &productName) override;
    bool doStart() override;
    void cancel(const qbs::ErrorInfo &reason) override;

    void waitForFinished();

    const JavaScriptCommand *jsCommand() const;

    QThread *m_thread;
    JsCommandExecutorThreadObject *m_objectInThread;
    bool m_running;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_JSCOMMANDEXECUTOR_H
