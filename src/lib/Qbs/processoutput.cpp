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


#include "processoutput.h"
#include <QSharedData>
#include <QStringList>

#include <QtDebug>


namespace Qbs {

class ProcessOutputData : public QSharedData
{
public:
    QByteArray standardOutput;
    QByteArray standardError;
    QString commandLine;
    QStringList filePaths;
};

ProcessOutput::ProcessOutput()
    : data(new ProcessOutputData)
{
}

ProcessOutput::ProcessOutput(const ProcessOutput &other)
    : data(other.data)
{
}

ProcessOutput &ProcessOutput::operator=(const ProcessOutput &other)
{
    if (this != &other)
        data = other.data;

    return *this;
}

ProcessOutput::~ProcessOutput()
{
}

void ProcessOutput::setStandardOutput(const QByteArray &standardOutput)
{
    data->standardOutput = standardOutput;
}

QByteArray ProcessOutput::standardOutput() const
{
    return data->standardOutput;
}

void ProcessOutput::setStandardError(const QByteArray &standardError)
{
    data->standardError = standardError;
}

QByteArray ProcessOutput::standardError() const
{
    return data->standardError;
}

void ProcessOutput::setCommandLine(const QString &commandLine)
{
    data->commandLine = commandLine;
}

QString ProcessOutput::commandLine() const
{
    return data->commandLine;
}

void ProcessOutput::setFilePaths(const QStringList &filePathList)
{
    data->filePaths = filePathList;
}

QStringList ProcessOutput::filePaths() const
{
    return data->filePaths;
}

QDataStream &operator<<(QDataStream &out, const ProcessOutput &processOutput)
{
    out << processOutput.commandLine();
    out << processOutput.standardOutput();
    out << processOutput.standardError();

    return out;
}

QDataStream &operator>>(QDataStream &in, ProcessOutput &processOutput)
{
    in >> processOutput.data->commandLine;
    in >> processOutput.data->standardOutput;
    in >> processOutput.data->standardError;

    return in;
}

} // namespace Qbs
