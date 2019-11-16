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
#ifndef QBS_PROJECT_H
#define QBS_PROJECT_H

#include "rulecommand.h"
#include "transformerdata.h"
#include "../language/forward_decls.h"
#include "../tools/error.h"
#include "../tools/qbs_export.h"

#include <QtCore/qshareddata.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <set>

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
class GroupData;
class ILogSink;
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
    RunEnvironment getRunEnvironment(const ProductData &product,
            const InstallOptions &installOptions,
            const QProcessEnvironment &environment,
            const QStringList &setupRunEnvConfig, Settings *settings) const;

    enum ProductSelection { ProductSelectionDefaultOnly, ProductSelectionWithNonDefault };
    BuildJob *buildAllProducts(const BuildOptions &options,
                               ProductSelection productSelection = ProductSelectionDefaultOnly,
                               QObject *jobOwner = nullptr) const;
    BuildJob *buildSomeProducts(const QList<ProductData> &products, const BuildOptions &options,
                                QObject *jobOwner = nullptr) const;
    BuildJob *buildOneProduct(const ProductData &product, const BuildOptions &options,
                              QObject *jobOwner = nullptr) const;

    CleanJob *cleanAllProducts(const CleanOptions &options, QObject *jobOwner = nullptr) const;
    CleanJob *cleanSomeProducts(const QList<ProductData> &products, const CleanOptions &options,
                                QObject *jobOwner = nullptr) const;
    CleanJob *cleanOneProduct(const ProductData &product, const CleanOptions &options,
                              QObject *jobOwner = nullptr) const;

    InstallJob *installAllProducts(const InstallOptions &options,
                                   ProductSelection productSelection = ProductSelectionDefaultOnly,
                                   QObject *jobOwner = nullptr) const;
    InstallJob *installSomeProducts(const QList<ProductData> &products,
                                    const InstallOptions &options,
                                    QObject *jobOwner = nullptr) const;
    InstallJob *installOneProduct(const ProductData &product, const InstallOptions &options,
                                  QObject *jobOwner = nullptr) const;

    void updateTimestamps(const QList<ProductData> &products);

    bool operator==(const Project &other) const { return d.data() == other.d.data(); }

    QStringList generatedFiles(const ProductData &product, const QString &file,
                               bool recursive, const QStringList &tags = QStringList()) const;

    QVariantMap projectConfiguration() const;

    std::set<QString> buildSystemFiles() const;

    RuleCommandList ruleCommands(const ProductData &product, const QString &inputFilePath,
                                 const QString &outputFileTag, ErrorInfo *error = nullptr) const;
    ProjectTransformerData transformerData(ErrorInfo *error = nullptr) const;

    ErrorInfo dumpNodesTree(QIODevice &outDevice, const QList<ProductData> &products);


    class BuildGraphInfo
    {
    public:
        QString bgFilePath;
        QVariantMap overriddenProperties;
        QVariantMap profileData;
        QVariantMap requestedProperties;
        ErrorInfo error;
    };
    static BuildGraphInfo getBuildGraphInfo(const QString &bgFilePath,
                                            const QStringList &requestedProperties);

    // Use with loaded project. Does not set requestedProperties.
    BuildGraphInfo getBuildGraphInfo() const;


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
