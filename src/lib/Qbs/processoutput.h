/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/


#ifndef QBS_PROCESSOUTPUT_H
#define QBS_PROCESSOUTPUT_H

#include <QtCore/QByteArray>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QString>
#include <QtCore/QMetaType>

namespace Qbs {

class ProcessOutputData;

class ProcessOutput
{
    friend QDataStream &operator>>(QDataStream &in, ProcessOutput &processOutput);
public:
    ProcessOutput();
    ProcessOutput(const ProcessOutput &other);
    ProcessOutput &operator=(const ProcessOutput &other);
    ~ProcessOutput();
    
    void setStandardOutput(const QByteArray &standardOutput);
    QByteArray standardOutput() const;

    void setStandardError(const QByteArray &standardError);
    QByteArray standardError() const;

    void setCommandLine(const QString &commandLine);
    QString commandLine() const;

private:
    QSharedDataPointer<ProcessOutputData> data;
};

QDataStream &operator<<(QDataStream &out, const ProcessOutput &processOutput);
QDataStream &operator>>(QDataStream &in, ProcessOutput &processOutput);

} // namespace Qbs

Q_DECLARE_METATYPE(Qbs::ProcessOutput)

#endif // QBS_PROCESSOUTPUT_H
