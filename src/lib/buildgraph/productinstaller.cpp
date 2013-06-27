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
#include "productinstaller.h"

#include "artifact.h"
#include "productbuilddata.h"
#include <language/language.h>
#include <logging/translator.h>
#include <tools/qbsassert.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/propertyfinder.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>

#include <QDir>
#include <QFileInfo>

namespace qbs {
namespace Internal {

ProductInstaller::ProductInstaller(const TopLevelProjectPtr &project,
        const QList<ResolvedProductPtr> &products, const InstallOptions &options,
        ProgressObserver *observer, const Logger &logger)
    : m_products(products), m_options(options), m_observer(observer), m_logger(logger)
{
    if (!m_options.installRoot().isEmpty()) {
        QFileInfo installRootFileInfo(m_options.installRoot());
        QBS_ASSERT(installRootFileInfo.isAbsolute(), /* just complain */);
        if (m_options.removeExistingInstallation()) {
            const QString cfp = installRootFileInfo.canonicalFilePath();
            if (cfp == QFileInfo(QDir::rootPath()).canonicalFilePath())
                throw ErrorInfo(Tr::tr("Refusing to remove root directory."));
            if (cfp == QFileInfo(QDir::homePath()).canonicalFilePath())
                throw ErrorInfo(Tr::tr("Refusing to remove home directory."));
        }
        return;
    }

    if (m_options.installIntoSysroot()) {
        if (m_options.removeExistingInstallation())
            throw ErrorInfo(Tr::tr("Refusing to remove sysroot."));
        m_options.setInstallRoot(PropertyFinder().propertyValue(project->buildConfiguration(),
                QLatin1String("qbs"), QLatin1String("sysroot")).toString());
    } else {
        m_options.setInstallRoot(project->buildDirectory + QLatin1Char('/') +
                                 InstallOptions::defaultInstallRoot());
    }
}

void ProductInstaller::install()
{
    if (m_options.removeExistingInstallation())
        removeInstallRoot();

    QList<const Artifact *> artifactsToInstall;
    foreach (const ResolvedProductConstPtr &product, m_products) {
        QBS_CHECK(product->buildData);
        foreach (const Artifact *artifact, product->buildData->artifacts) {
            if (artifact->properties->qbsPropertyValue(QLatin1String("install")).toBool())
                artifactsToInstall += artifact;
        }
    }
    m_observer->initialize(Tr::tr("Installing"), artifactsToInstall.count());

    foreach (const Artifact * const a, artifactsToInstall) {
        copyFile(a);
        m_observer->incrementProgressValue();
    }
}

void ProductInstaller::removeInstallRoot()
{
    const QString nativeInstallRoot = QDir::toNativeSeparators(m_options.installRoot());
    if (m_options.dryRun()) {
        m_logger.qbsInfo() << Tr::tr("Would remove install root '%1'.").arg(nativeInstallRoot);
        return;
    }
    m_logger.qbsDebug() << QString::fromLocal8Bit("Removing install root '%1'.")
            .arg(nativeInstallRoot);

    QString errorMessage;
    if (!removeDirectoryWithContents(m_options.installRoot(), &errorMessage)) {
        const QString fullErrorMessage = Tr::tr("Cannot remove install root '%1': %2")
                .arg(QDir::toNativeSeparators(m_options.installRoot()), errorMessage);
        handleError(fullErrorMessage);
    }
}

void ProductInstaller::copyFile(const Artifact *artifact)
{
    if (m_observer->canceled()) {
        throw ErrorInfo(Tr::tr("Installation canceled for configuration '%1'.")
                    .arg(m_products.first()->project->topLevelProject()->id()));
    }
    const QString relativeInstallDir
            = artifact->properties->qbsPropertyValue(QLatin1String("installDir")).toString();
    const QString installPrefix
            = artifact->properties->qbsPropertyValue(QLatin1String("installPrefix")).toString();
    QString targetDir = m_options.installRoot();
    targetDir.append(QLatin1Char('/')).append(installPrefix)
            .append(QLatin1Char('/')).append(relativeInstallDir);
    targetDir = QDir::cleanPath(targetDir);
    const QString nativeFilePath = QDir::toNativeSeparators(artifact->filePath());
    const QString nativeTargetDir = QDir::toNativeSeparators(targetDir);
    if (m_options.dryRun()) {
        m_logger.qbsInfo() << Tr::tr("Would copy file '%1' into target directory '%2'.")
                              .arg(nativeFilePath, nativeTargetDir);
        return;
    }
    m_logger.qbsDebug() << QString::fromLocal8Bit("Copying file '%1' into target directory '%2'.")
                           .arg(nativeFilePath, nativeTargetDir);

    if (!QDir::root().mkpath(targetDir)) {
        handleError(Tr::tr("Directory '%1' could not be created.").arg(nativeTargetDir));
        return;
    }
    const QString targetFilePath = QDir::cleanPath(targetDir + QLatin1Char('/')
                                                   + FileInfo::fileName(artifact->filePath()));
    QString errorMessage;
    if (!copyFileRecursion(artifact->filePath(), targetFilePath, true, &errorMessage))
        handleError(Tr::tr("Installation error: %1").arg(errorMessage));
}

void ProductInstaller::handleError(const QString &message)
{
    if (!m_options.keepGoing())
        throw ErrorInfo(message);
    m_logger.qbsWarning() << message;
}

} // namespace Intern
} // namespace qbs
