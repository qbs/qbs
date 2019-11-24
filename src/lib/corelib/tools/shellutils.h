/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Petroules Corporation.
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

#ifndef QBS_SHELLUTILS_H
#define QBS_SHELLUTILS_H

#include "qbs_export.h"
#include "hostosinfo.h"
#include <QtCore/qstring.h>

#include <vector>

namespace qbs {
namespace Internal {

QBS_EXPORT QString shellInterpreter(const QString &filePath);
QBS_EXPORT std::string shellQuote(const std::string &arg, HostOsInfo::HostOs os = HostOsInfo::hostOs());
QBS_EXPORT QString shellQuote(const QString &arg, HostOsInfo::HostOs os = HostOsInfo::hostOs());
QBS_EXPORT QString shellQuote(const QStringList &args,
                              HostOsInfo::HostOs os = HostOsInfo::hostOs());
QBS_EXPORT std::string shellQuote(const std::vector<std::string> &args,
                                  HostOsInfo::HostOs os = HostOsInfo::hostOs());
QBS_EXPORT QString shellQuote(const QString &program, const QStringList &args,
                              HostOsInfo::HostOs os = HostOsInfo::hostOs());

class QBS_EXPORT CommandLine
{
public:
    void setProgram(const QString &program, bool raw = false);
    void setProgram(const std::string &program, bool raw = false);
    void appendArgument(const QString &value);
    void appendArgument(const std::string &value);
    void appendArguments(const QList<QString> &args);
    void appendRawArgument(const QString &value);
    void appendRawArgument(const std::string &value);
    void appendPathArgument(const QString &value);
    void clearArguments();
    QString toCommandLine(HostOsInfo::HostOs os = HostOsInfo::hostOs()) const;

private:
    struct Argument
    {
        Argument(QString value = QString()) : value(std::move(value)) { }
        QString value;
        bool isFilePath = false;
        bool shouldQuote = true;
    };

    bool m_isRawProgram = false;
    QString m_program;
    std::vector<Argument> m_arguments;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_SHELLUTILS_H
