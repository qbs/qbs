/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "tst_blackboxbase.h"

#include "../shared.h"
#include <tools/profile.h>

class TestBlackboxJobLimits : public TestBlackboxBase
{
    Q_OBJECT

public:
    TestBlackboxJobLimits();

private slots:
    void jobLimits_data();
    void jobLimits();
};

TestBlackboxJobLimits::TestBlackboxJobLimits()
    : TestBlackboxBase (SRCDIR "/testdata-joblimits", "blackbox-joblimits")
{
}

void TestBlackboxJobLimits::jobLimits_data()
{
    QTest::addColumn<int>("projectJobCount");
    QTest::addColumn<int>("productJobCount");
    QTest::addColumn<int>("moduleJobCount");
    QTest::addColumn<int>("prefsJobCount");
    QTest::addColumn<int>("cliJobCount");
    QTest::addColumn<bool>("projectPrecedence");
    QTest::addColumn<bool>("expectSuccess");
    for (int projectJobCount = -1; projectJobCount <= 1; ++projectJobCount) {
        for (int productJobCount = -1; productJobCount <= 1; ++productJobCount) {
            for (int moduleJobCount = -1; moduleJobCount <= 1; ++moduleJobCount) {
                for (int prefsJobCount = -1; prefsJobCount <= 1; ++prefsJobCount) {
                    for (int cliJobCount = -1; cliJobCount <= 1; ++cliJobCount) {
                        QString description = QString("project:%1/"
                                "product:%2/module:%3/prefs:%4/cli:%5/project precedence")
                                .arg(projectJobCount).arg(productJobCount).arg(moduleJobCount)
                                .arg(prefsJobCount).arg(cliJobCount).toLocal8Bit();
                        bool expectSuccess;
                        switch (productJobCount) {
                        case 1: expectSuccess = true; break;
                        case 0: expectSuccess = false; break;
                        case -1:
                            switch (projectJobCount) {
                            case 1: expectSuccess = true; break;
                            case 0: expectSuccess = false; break;
                            case -1:
                                switch (moduleJobCount) {
                                case 1: expectSuccess = true; break;
                                case 0: expectSuccess = false; break;
                                case -1:
                                    switch (cliJobCount) {
                                    case 1: expectSuccess = true; break;
                                    case 0: expectSuccess = false; break;
                                    case -1: expectSuccess = prefsJobCount == 1; break;
                                    }
                                    break;
                                }
                                break;
                            }
                            break;
                        }
                        QTest::newRow(qPrintable(description))
                                << projectJobCount << productJobCount << moduleJobCount
                                << prefsJobCount << cliJobCount << true << expectSuccess;
                    description = QString("project:%1/"
                            "product:%2/module:%3/prefs:%4/cli:%5/default precedence")
                            .arg(projectJobCount).arg(productJobCount).arg(moduleJobCount)
                            .arg(prefsJobCount).arg(cliJobCount).toLocal8Bit();
                    switch (cliJobCount) {
                    case 1: expectSuccess = true; break;
                    case 0: expectSuccess = false; break;
                    case -1:
                        switch (prefsJobCount) {
                        case 1: expectSuccess = true; break;
                        case 0: expectSuccess = false; break;
                        case -1:
                            switch (productJobCount) {
                            case 1: expectSuccess = true; break;
                            case 0: expectSuccess = false; break;
                            case -1:
                                switch (projectJobCount) {
                                case 1: expectSuccess = true; break;
                                case 0: expectSuccess = false; break;
                                case -1: expectSuccess = moduleJobCount == 1; break;
                                }
                                break;
                            }
                            break;
                        }
                        break;
                    }
                    QTest::newRow(qPrintable(description))
                            << projectJobCount << productJobCount << moduleJobCount
                            << prefsJobCount << cliJobCount << false << expectSuccess;
                }
                }
            }
        }
    }
}

void TestBlackboxJobLimits::jobLimits()
{
    QDir::setCurrent(testDataDir + "/job-limits");
    QFETCH(int, projectJobCount);
    QFETCH(int, productJobCount);
    QFETCH(int, moduleJobCount);
    QFETCH(int, prefsJobCount);
    QFETCH(int, cliJobCount);
    QFETCH(bool, projectPrecedence);
    QFETCH(bool, expectSuccess);
    SettingsPtr theSettings = settings();
    qbs::Internal::TemporaryProfile profile("jobLimitsProfile", theSettings.get());
    profile.p.setValue("preferences.jobLimit.singleton", prefsJobCount);
    theSettings->sync();
    QbsRunParameters resolveParams("resolve");
    resolveParams.profile = profile.p.name();
    resolveParams.arguments << ("project.projectJobCount:" + QString::number(projectJobCount))
                            << ("project.productJobCount:" + QString::number(productJobCount))
                            << ("project.moduleJobCount:" + QString::number(moduleJobCount));
    QCOMPARE(runQbs(resolveParams), 0);
    QbsRunParameters buildParams;
    buildParams.expectFailure = !expectSuccess;
    if (cliJobCount != -1)
        buildParams.arguments << "--job-limits" << ("singleton:" + QString::number(cliJobCount));
    if (projectPrecedence)
        buildParams.arguments << "--enforce-project-job-limits";
    buildParams.profile = profile.p.name();
    const int exitCode = runQbs(buildParams);
    if (expectSuccess)
        QCOMPARE(exitCode, 0);
    else if (exitCode == 0)
        QSKIP("no failure with no limit in place, result inconclusive");
    else
        QVERIFY2(m_qbsStderr.contains("exclusive"), m_qbsStderr.constData());
    if (exitCode == 0)
        QCOMPARE(m_qbsStdout.count("Running tool"), 5);
}

QTEST_MAIN(TestBlackboxJobLimits)

#include <tst_blackboxjoblimits.moc>
