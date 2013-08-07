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
#ifndef QBS_PROJECT_H
#define QBS_PROJECT_H

#include "../language/forward_decls.h"
#include "../tools/qbs_export.h"

#include <QExplicitlySharedDataPointer>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
class QObject;
class QProcessEnvironment;
QT_END_NAMESPACE

namespace qbs {
class BuildJob;
class BuildOptions;
class CleanJob;
class CleanOptions;
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
    static SetupProjectJob *setupProject(const SetupProjectParameters &parameters,
                                         ILogSink *logSink, QObject *jobOwner);

    Project(const Project &other);
    Project &operator=(const Project &other);
    ~Project();

    ProjectData projectData() const;
    QString targetExecutable(const ProductData &product,
                             const InstallOptions &installoptions) const;
    RunEnvironment getRunEnvironment(const ProductData &product,
            const QProcessEnvironment &environment, Settings *settings) const;

    BuildJob *buildAllProducts(const BuildOptions &options, QObject *jobOwner = 0) const;
    BuildJob *buildSomeProducts(const QList<ProductData> &products, const BuildOptions &options,
                                QObject *jobOwner = 0) const;
    BuildJob *buildOneProduct(const ProductData &product, const BuildOptions &options,
                              QObject *jobOwner = 0) const;

    CleanJob *cleanAllProducts(const CleanOptions &options, QObject *jobOwner = 0) const;
    CleanJob *cleanSomeProducts(const QList<ProductData> &products, const CleanOptions &options,
                                QObject *jobOwner = 0) const;
    CleanJob *cleanOneProduct(const ProductData &product, const CleanOptions &options,
                              QObject *jobOwner = 0) const;

    InstallJob *installAllProducts(const InstallOptions &options, QObject *jobOwner = 0) const;
    InstallJob *installSomeProducts(const QList<ProductData> &products,
                                    const InstallOptions &options, QObject *jobOwner = 0) const;
    InstallJob *installOneProduct(const ProductData &product, const InstallOptions &options,
                                  QObject *jobOwner = 0) const;

    void updateTimestamps(const QList<ProductData> &products);

    bool operator==(const Project &other) const { return d.data() == other.d.data(); }

    QStringList generatedFiles(const ProductData &product, const QString &file,
                               const QStringList &tags = QStringList()) const;

    QVariantMap projectConfiguration() const;
    QHash<QString, QString> usedEnvironment() const;

    QSet<QString> buildSystemFiles() const;

private:
    Project();
    Project(const Internal::TopLevelProjectPtr &internalProject, const Internal::Logger &logger);

    QExplicitlySharedDataPointer<Internal::ProjectPrivate> d;
};

inline bool operator!=(const Project &p1, const Project &p2) { return !(p1 == p2); }
inline uint qHash(const Project &p) { return QT_PREPEND_NAMESPACE(qHash)(p.d.data()); }

} // namespace qbs

#endif // QBS_PROJECT_H
