/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#ifndef TST_LANGUAGE_H
#define TST_LANGUAGE_H

#include <language/forward_decls.h>
#include <language/loader.h>
#include <logging/ilogsink.h>
#include <tools/setupprojectparameters.h>
#include <tools/qbs_export.h>
#include <QtTest>

namespace qbs {
namespace Internal {

class QBS_EXPORT TestLanguage : public QObject
{
    Q_OBJECT
public:
    TestLanguage(ILogSink *logSink);
    ~TestLanguage();

private:
    ILogSink *m_logSink;
    Logger m_logger;
    ScriptEngine *m_engine;
    Loader *loader;
    TopLevelProjectPtr project;
    SetupProjectParameters defaultParameters;
    const QString m_wildcardsTestDirPath;

    QHash<QString, ResolvedProductPtr> productsFromProject(ResolvedProjectPtr project);
    ResolvedModuleConstPtr findModuleByName(ResolvedProductPtr product, const QString &name);
    QVariant productPropertyValue(ResolvedProductPtr product, QString propertyName);
    void handleInitCleanupDataTags(const char *projectFileName, bool *handled);
    QString buildDir(const SetupProjectParameters &params) const;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void baseProperty();
    void buildConfigStringListSyntax();
    void conditionalDepends();
    void environmentVariable();
    void erroneousFiles_data();
    void erroneousFiles();
    void exports();
    void fileContextProperties();
    void groupConditions_data();
    void groupConditions();
    void groupName();
    void homeDirectory();
    void identifierSearch_data();
    void identifierSearch();
    void idUsage();
    void itemPrototype();
    void itemScope();
    void jsExtensions();
    void jsImportUsedInMultipleScopes_data();
    void jsImportUsedInMultipleScopes();
    void moduleProperties_data();
    void moduleProperties();
    void moduleScope();
    void modules_data();
    void modules();
    void outerInGroup();
    void pathProperties();
    void profileValuesAndOverriddenValues();
    void productConditions();
    void productDirectories();
    void propertiesBlocks_data();
    void propertiesBlocks();
    void fileTags_data();
    void fileTags();
    void wildcards_data();
    void wildcards();
};

} // namespace Internal
} // namespace qbs

#endif // TST_LANGUAGE_H
