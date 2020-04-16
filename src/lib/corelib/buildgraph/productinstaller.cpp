/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#include "productinstaller.h"

#include "artifact.h"
#include "productbuilddata.h"

#include <language/language.h>
#include <language/propertymapinternal.h>
#include <logging/translator.h>
#include <tools/qbsassert.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/hostosinfo.h>
#include <tools/progressobserver.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>

namespace qbs {
namespace Internal {

ProductInstaller::ProductInstaller(TopLevelProjectPtr project,
        QVector<ResolvedProductPtr> products, InstallOptions options,
        ProgressObserver *observer, Logger logger)
    : m_project(std::move(project)),
      m_products(std::move(products)),
      m_options(std::move(options)),
      m_observer(observer),
      m_logger(std::move(logger))
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
    }
    initInstallRoot(m_project.get(), m_options);
}

void ProductInstaller::install()
{
    m_targetFilePathsMap.clear();

    if (m_options.removeExistingInstallation())
        removeInstallRoot();

    QList<const Artifact *> artifactsToInstall;
    for (const auto &product : qAsConst(m_products)) {
        QBS_CHECK(product->buildData);
        for (const Artifact *artifact : filterByType<Artifact>(product->buildData->allNodes())) {
            if (artifact->properties->qbsPropertyValue(StringConstants::installProperty()).toBool())
                artifactsToInstall.push_back(artifact);
        }
    }
    m_observer->initialize(Tr::tr("Installing"), artifactsToInstall.size());

    for (const Artifact * const a : qAsConst(artifactsToInstall)) {
        copyFile(a);
        m_observer->incrementProgressValue();
    }
}

QString ProductInstaller::targetFilePath(const TopLevelProject *project,
        const QString &productSourceDir,
        const QString &sourceFilePath, const PropertyMapConstPtr &properties,
        InstallOptions &options)
{
    if (!properties->qbsPropertyValue(StringConstants::installProperty()).toBool())
        return {};
    const QString relativeInstallDir
            = properties->qbsPropertyValue(StringConstants::installDirProperty()).toString();
    const QString installPrefix
            = properties->qbsPropertyValue(StringConstants::installPrefixProperty()).toString();
    const QString installSourceBase
            = properties->qbsPropertyValue(StringConstants::installSourceBaseProperty()).toString();
    initInstallRoot(project, options);
    QString targetDir = options.installRoot();
    if (targetDir.isEmpty())
        targetDir = properties->qbsPropertyValue(StringConstants::installRootProperty()).toString();
    targetDir.append(QLatin1Char('/')).append(installPrefix)
            .append(QLatin1Char('/')).append(relativeInstallDir);
    targetDir = QDir::cleanPath(targetDir);

    QString targetFilePath;
    if (installSourceBase.isEmpty()) {
        if (!targetDir.startsWith(options.installRoot())) {
            throw ErrorInfo(Tr::tr("Cannot install '%1', because target directory '%2' "
                                   "is outside of install root '%3'")
                            .arg(sourceFilePath, targetDir, options.installRoot()));
        }

        // This has the same effect as if installSourceBase would equal the directory of the file.
        targetFilePath = FileInfo::fileName(sourceFilePath);
    } else {
        const QString localAbsBasePath = FileInfo::resolvePath(QDir::cleanPath(productSourceDir),
                                                               QDir::cleanPath(installSourceBase));
        targetFilePath = sourceFilePath;
        if (!targetFilePath.startsWith(localAbsBasePath)) {
            throw ErrorInfo(Tr::tr("Cannot install '%1', because it doesn't start with the"
                                   " value of qbs.installSourceBase '%2'.").arg(sourceFilePath,
                                                                                localAbsBasePath));
        }

        // Since there is a difference between X: and X:\\ on Windows, absolute paths can sometimes
        // end with a slash, so only remove an extra character if there is no ending slash
        targetFilePath.remove(0, localAbsBasePath.length()
                                 + (localAbsBasePath.endsWith(QLatin1Char('/')) ? 0 : 1));
    }

    targetFilePath.prepend(targetDir + QLatin1Char('/'));
    return QDir::cleanPath(targetFilePath);
}

void ProductInstaller::initInstallRoot(const TopLevelProject *project,
                                       InstallOptions &options)
{
    if (!options.installRoot().isEmpty())
        return;

    options.setInstallRoot(effectiveInstallRoot(options, project));
}

void ProductInstaller::removeInstallRoot()
{
    const QString nativeInstallRoot = QDir::toNativeSeparators(m_options.installRoot());
    if (m_options.dryRun()) {
        m_logger.qbsInfo() << Tr::tr("Would remove install root '%1'.").arg(nativeInstallRoot);
        return;
    }
    m_logger.qbsDebug() << QStringLiteral("Removing install root '%1'.")
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
                    .arg(m_products.front()->project->topLevelProject()->id()));
    }

    const QString targetFilePath = this->targetFilePath(m_project.get(),
            artifact->product->sourceDirectory, artifact->filePath(),
            artifact->properties, m_options);
    const QString targetDir = FileInfo::path(targetFilePath);
    const QString nativeFilePath = QDir::toNativeSeparators(artifact->filePath());
    const QString nativeTargetDir = QDir::toNativeSeparators(targetDir);
    if (m_options.dryRun()) {
        m_logger.qbsDebug() << Tr::tr("Would copy file '%1' into target directory '%2'.")
                               .arg(nativeFilePath, nativeTargetDir);
        return;
    }
    m_logger.qbsDebug() << QStringLiteral("Copying file '%1' into target directory '%2'.")
                           .arg(nativeFilePath, nativeTargetDir);

    if (!QDir::root().mkpath(targetDir)) {
        handleError(Tr::tr("Directory '%1' could not be created.").arg(nativeTargetDir));
        return;
    }
    QFileInfo fi(artifact->filePath());
    if (fi.isDir() && !(HostOsInfo::isAnyUnixHost() && fi.isSymLink())) {
        m_logger.qbsWarning() << Tr::tr("Not recursively copying directory '%1' into target "
                                        "directory '%2'. Install the individual file artifacts "
                                        "instead.")
                                 .arg(nativeFilePath, nativeTargetDir);
    }

    if (m_targetFilePathsMap.contains(targetFilePath)) {
        // We only want this error message when installing artifacts pointing to different file
        // paths, to the same location. We do NOT want it when installing different artifacts
        // pointing to the same file, to the same location. This reduces unnecessary noise: for
        // example, when installing headers from a multiplexed product, the user does not need to
        // do extra work to ensure the files are installed by only one of the instances.
        if (artifact->filePath() != m_targetFilePathsMap[targetFilePath]) {
            handleError(Tr::tr("Cannot install files '%1' and '%2' to the same location '%3'. "
                               "If you are attempting to install a directory hierarchy, consider "
                               "using the qbs.installSourceBase property.")
                        .arg(artifact->filePath(), m_targetFilePathsMap[targetFilePath],
                             targetFilePath));
        }
    }
    m_targetFilePathsMap.insert(targetFilePath, artifact->filePath());

    QString errorMessage;
    if (!copyFileRecursion(artifact->filePath(), targetFilePath, true, false, &errorMessage))
        handleError(Tr::tr("Installation error: %1").arg(errorMessage));
}

void ProductInstaller::handleError(const QString &message)
{
    if (!m_options.keepGoing())
        throw ErrorInfo(message);
    m_logger.qbsWarning() << message;
}

} // namespace Internal
} // namespace qbs
