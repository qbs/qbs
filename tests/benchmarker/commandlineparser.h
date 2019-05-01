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
#ifndef QBS_BENCHMARKER_COMMANDLINEPARSER_H
#define QBS_BENCHMARKER_COMMANDLINEPARSER_H

#include "activities.h"

#include <QtCore/qstringlist.h>

namespace qbsBenchmarker {

class CommandLineParser
{
public:
    CommandLineParser();

    void parse();

    Activities activies() const { return m_activities; }
    QString oldCommit() const { return m_oldCommit; }
    QString newCommit() const { return m_newCommit; }
    QString testProjectFilePath() const { return m_testProjectFilePath; }
    QString qbsRepoDirPath() const { return m_qbsRepoDirPath; }
    int regressionThreshold() const { return m_regressionThreshold; }

private:
    [[noreturn]] void throwException(const QString &optionName, const QString &illegalValue,
                                   const QString &helpText);
    [[noreturn]] void throwException(const QString &missingOption, const QString &helpText);

    Activities m_activities;
    QString m_oldCommit;
    QString m_newCommit;
    QString m_testProjectFilePath;
    QString m_qbsRepoDirPath;
    int m_regressionThreshold = 0;
};

} // namespace qbsBenchmarker

#endif // Include guard.
