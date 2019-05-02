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

#ifndef QBS_TST_CLANGDB_H
#define QBS_TST_CLANGDB_H

#include "tst_blackbox.h"

#include <tools/version.h>

class TestClangDb : public TestBlackboxBase
{
    Q_OBJECT

public:
    TestClangDb();

private slots:
    void initTestCase() override;
    void ensureBuildTreeCreated();
    void checkCanGenerateDb();
    void checkDbIsValidJson();
    void checkDbIsConsistentWithProject();
    void checkClangDetectsSourceCodeProblems();

private:
    int runProcess(const QString &exec, const QStringList &args, QByteArray &stdErr,
                   QByteArray &stdOut);
    qbs::Version clangVersion();

    const QString projectDir;
    const QString projectFileName;
    const QString buildDir;
    const QString sourceFilePath;
    const QString dbFilePath;
    QProcessEnvironment processEnvironment;
};

#endif // Include guard.
