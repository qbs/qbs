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

#ifndef TST_BLACKBOX_H
#define TST_BLACKBOX_H

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtTest>

class QbsRunParameters
{
public:
    QbsRunParameters()
    {
        init();
    }

    QbsRunParameters(const QString &cmd, const QStringList &args = QStringList())
        : command(cmd), arguments(args)
    {
        init();
    }

    QbsRunParameters(const QStringList &args)
        : arguments(args)
    {
        init();
    }

    void init()
    {
        expectFailure = false;
        useProfile = true;
        environment = QProcessEnvironment::systemEnvironment();
    }

    QString command;
    QStringList arguments;
    QProcessEnvironment environment;
    bool expectFailure;
    bool useProfile;
};

class TestBlackbox : public QObject
{
    Q_OBJECT
    const QString testDataDir;
    const QString testSourceDir;
    const QString qbsExecutableFilePath;
    const QString defaultInstallRoot;

public:
    TestBlackbox();

protected:
    int runQbs(const QbsRunParameters &params = QbsRunParameters());
    void rmDirR(const QString &dir);
    static QByteArray unifiedLineEndings(const QByteArray &ba);
    static void sanitizeOutput(QByteArray *ba);

public slots:
    void initTestCase();

private slots:
    void alwaysRun();
    void alwaysRun_data();
    void android();
    void android_data();
    void assembly();
    void assetCatalog();
    void badInterpreter();
    void buildDirectories();
    void changedFiles_data();
    void changedFiles();
    void changeInDisabledProduct();
    void checkProjectFilePath();
    void clean();
    void cli();
    void dependenciesProperty();
    void dynamicMultiplexRule();
    void dynamicRuleOutputs();
    void embedInfoPlist();
    void erroneousFiles_data();
    void erroneousFiles();
    void exportRule();
    void fileDependencies();
    void frameworkStructure();
    void groupsInModules();
    void iconset();
    void iconsetApp();
    void inputsFromDependencies();
    void installable();
    void installedApp();
    void installedSourceFiles();
    void installedTransformerOutput();
    void installPackage();
    void installTree();
    void java();
    void jsExtensionsFile();
    void jsExtensionsFileInfo();
    void jsExtensionsProcess();
    void jsExtensionsPropertyList();
    void jsExtensionsTemporaryDir();
    void jsExtensionsTextFile();
    void listPropertiesWithOuter();
    void listPropertyOrder();
    void loadableModule();
    void missingProfile();
    void mixedBuildVariants();
    void multipleChanges();
    void nestedProperties();
    void nodejs();
    void nonBrokenFilesInBrokenProduct();
    void nonDefaultProduct();
    void nsis();
    void objcArc();
    void overrideProjectProperties();
    void probesInNestedModules();
    void productDependenciesByType();
    void productProperties();
    void propertyChanges();
    void propertyPrecedence();
    void properQuoting();
    void qbsVersion();
    void qmlDebugging();
    void qobjectInObjectiveCpp();
    void radAfterIncompleteBuild_data();
    void radAfterIncompleteBuild();
    void recursiveRenaming();
    void recursiveWildcards();
    void ruleConditions();
    void ruleCycle();
    void subProfileChangeTracking();
    void symlinkRemoval();
    void renameDependency();
    void separateDebugInfo();
    void sevenZip();
    void tar();
    void toolLookup();
    void track_qobject_change();
    void track_qrc();
    void trackAddFile();
    void trackAddFileTag();
    void trackAddMocInclude();
    void trackAddProduct();
    void trackExternalProductChanges();
    void trackRemoveFile();
    void trackRemoveFileTag();
    void trackRemoveProduct();
    void transitiveOptionalDependencies();
    void typescript();
    void usingsAsSoleInputsNonMultiplexed();
    void wildCardsAndRules();
    void wildcardRenaming();
    void wix();
    void xcode();
    void zip();
    void zip_data();

private:
    QString findArchiver(const QString &fileName, int *status = nullptr);

    QByteArray m_qbsStderr;
    QByteArray m_qbsStdout;
};

#endif // TST_BLACKBOX_H
