/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "runsupport.h"

#include "exception.h"

#include <QByteArray>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace qbsBenchmarker {

void runProcess(const QStringList &commandLine, const QString &workingDir, QByteArray *output)
{
    QStringList args = commandLine;
    const QString command = args.takeFirst();
    QProcess p;
    if (!workingDir.isEmpty())
        p.setWorkingDirectory(workingDir);
    p.start(command, args);
    if (!p.waitForStarted())
        throw Exception(QString::fromLatin1("Process '%1' failed to start.").arg(command));
    p.waitForFinished(-1);
    if (p.exitStatus() != QProcess::NormalExit) {
        throw Exception(QString::fromLatin1("Error running '%1': %2")
                        .arg(command, p.errorString()));
    }
    if (p.exitCode() != 0) {
        QString errorString = QString::fromLatin1("Command '%1' finished with exit code %2.")
                .arg(command).arg(p.exitCode());
        const QByteArray stdErr = p.readAllStandardError();
        if (!stdErr.isEmpty()) {
            errorString += QString::fromLatin1("\nStandard error output was: '%1'")
                    .arg(QString::fromLocal8Bit(stdErr));
        }
        throw Exception(errorString);
    }
    if (output)
        *output = p.readAllStandardOutput().trimmed();
}

} // namespace qbsBenchmarker
