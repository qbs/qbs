/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "runsupport.h"

#include "exception.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

namespace qbsBenchmarker {

void runProcess(const QStringList &commandLine, const QString &workingDir, QByteArray *output,
                int *exitCode)
{
    QStringList args = commandLine;
    const QString command = args.takeFirst();
    QProcess p;
    if (!workingDir.isEmpty())
        p.setWorkingDirectory(workingDir);
    p.start(command, args);
    if (!p.waitForStarted())
        throw Exception(QStringLiteral("Process '%1' failed to start.").arg(command));
    p.waitForFinished(-1);
    if (p.exitStatus() != QProcess::NormalExit) {
        throw Exception(QStringLiteral("Error running '%1': %2")
                        .arg(command, p.errorString()));
    }
    if (exitCode)
        *exitCode = p.exitCode();
    if (p.exitCode() != 0) {
        QString errorString = QStringLiteral("Command '%1' finished with exit code %2.")
                .arg(command).arg(p.exitCode());
        const QByteArray stdErr = p.readAllStandardError();
        if (!stdErr.isEmpty()) {
            errorString += QStringLiteral("\nStandard error output was: '%1'")
                    .arg(QString::fromLocal8Bit(stdErr));
        }
        throw Exception(errorString);
    }
    if (output)
        *output = p.readAllStandardOutput().trimmed();
}

} // namespace qbsBenchmarker
