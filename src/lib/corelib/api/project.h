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
** a written agreement between you and The Qt Company. For licensing terms and
** conditions see http://www.qt.io/terms-conditions. For further information
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
#ifndef QBS_PROJECT_H
#define QBS_PROJECT_H

#include "rulecommand.h"
#include "../language/forward_decls.h"
#include "../tools/qbs_export.h"

#include <QExplicitlySharedDataPointer>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
class QIODevice;
class QObject;
class QProcessEnvironment;
QT_END_NAMESPACE

namespace qbs {
class BuildJob;
class BuildOptions;
class CleanJob;
class CleanOptions;
class ErrorInfo;
class GroupData;
class ILogSink;
class InstallableFile;
class InstallJob;
class InstallOptions;
class ProductData;
class ProjectData;
class RunEnvironment;
class Settings;
class SetupProjectJob;
class SetupProjectParameters;

namespace Internal {
class Logger;
class ProjectPrivate;
} // namespace Internal;

class QBS_EXPORT Project
{
    friend class SetupProjectJob;
    friend uint qHash(const Project &p);
public:
    SetupProjectJob *setupProject(const SetupProjectParameters &parameters,
                                  ILogSink *logSink, QObject *jobOwner);

    Project();
    Project(const Project &other);
    Project &operator=(const Project &other);
    ~Project();

    bool isValid() const;
    QString profile() const;
    ProjectData projectData() const;
    QString targetExecutable(const ProductData &product,
                             const InstallOptions &installoptions) const;
    RunEnvironment getRunEnvironment(const ProductData &product,
            const InstallOptions &installOptions,
            const QProcessEnvironment &environment, Settings *settings) const;

    enum ProductSelection { ProductSelectionDefaultOnly, ProductSelectionWithNonDefault };
    BuildJob *buildAllProducts(const BuildOptions &options,
                               ProductSelection productSelection = ProductSelectionDefaultOnly,
                               QObject *jobOwner = 0) const;
    BuildJob *buildSomeProducts(const QList<ProductData> &products, const BuildOptions &options,
                                QObject *jobOwner = 0) const;
    BuildJob *buildOneProduct(const ProductData &product, const BuildOptions &options,
                              QObject *jobOwner = 0) const;

    CleanJob *cleanAllProducts(const CleanOptions &options, QObject *jobOwner = 0) const;
    CleanJob *cleanSomeProducts(const QList<ProductData> &products, const CleanOptions &options,
                                QObject *jobOwner = 0) const;
    CleanJob *cleanOneProduct(const ProductData &product, const CleanOptions &options,
                              QObject *jobOwner = 0) const;

    InstallJob *installAllProducts(const InstallOptions &options,
                                   ProductSelection productSelection = ProductSelectionDefaultOnly,
                                   QObject *jobOwner = 0) const;
    InstallJob *installSomeProducts(const QList<ProductData> &products,
                                    const InstallOptions &options, QObject *jobOwner = 0) const;
    InstallJob *installOneProduct(const ProductData &product, const InstallOptions &options,
                                  QObject *jobOwner = 0) const;

    QList<InstallableFile> installableFilesForProduct(const ProductData &product,
                                                      const InstallOptions &options) const;
    QList<InstallableFile> installableFilesForProject(const ProjectData &project,
                                                      const InstallOptions &options) const;

    void updateTimestamps(const QList<ProductData> &products);

    bool operator==(const Project &other) const { return d.data() == other.d.data(); }

    QStringList generatedFiles(const ProductData &product, const QString &file,
                               const QStringList &tags = QStringList()) const;

    QVariantMap projectConfiguration() const;
    QHash<QString, QString> usedEnvironment() const;

    QSet<QString> buildSystemFiles() const;

    RuleCommandList ruleCommands(const ProductData &product, const QString &inputFilePath,
                                 const QString &outputFileTag, ErrorInfo *error = 0) const;

    ErrorInfo dumpNodesTree(QIODevice &outDevice, const QList<ProductData> &products);

#ifdef QBS_ENABLE_PROJECT_FILE_UPDATES
    ErrorInfo addGroup(const ProductData &product, const QString &groupName);
    ErrorInfo addFiles(const ProductData &product, const GroupData &group,
                       const QStringList &filePaths);
    ErrorInfo removeFiles(const ProductData &product, const GroupData &group,
                          const QStringList &filePaths);
    ErrorInfo removeGroup(const ProductData &product, const GroupData &group);
#endif // QBS_ENABLE_PROJECT_FILE_UPDATES

private:
    Project(const Internal::TopLevelProjectPtr &internalProject, const Internal::Logger &logger);

    QExplicitlySharedDataPointer<Internal::ProjectPrivate> d;
};

inline bool operator!=(const Project &p1, const Project &p2) { return !(p1 == p2); }
inline uint qHash(const Project &p) { return QT_PREPEND_NAMESPACE(qHash)(p.d.data()); }

} // namespace qbs

#endif // QBS_PROJECT_H
