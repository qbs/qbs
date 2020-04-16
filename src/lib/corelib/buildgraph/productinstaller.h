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
#ifndef QBS_PRODUCT_INSTALLER_H
#define QBS_PRODUCT_INSTALLER_H

#include "forward_decls.h"

#include <language/forward_decls.h>
#include <logging/logger.h>
#include <tools/installoptions.h>

#include <QtCore/qhash.h>
#include <QtCore/qlist.h>

namespace qbs {
namespace Internal {
class ProgressObserver;

class ProductInstaller
{
public:
    ProductInstaller(TopLevelProjectPtr project,
            QVector<ResolvedProductPtr> products,
            InstallOptions options, ProgressObserver *observer, Logger logger);
    void install();

    static QString targetFilePath(const TopLevelProject *project, const QString &productSourceDir,
            const QString &sourceFilePath, const PropertyMapConstPtr &properties,
            InstallOptions &options);
    static void initInstallRoot(const TopLevelProject *project, InstallOptions &options);

    void removeInstallRoot();
    void copyFile(const Artifact *artifact);

private:
    void handleError(const QString &message);

    const TopLevelProjectConstPtr m_project;
    const QVector<ResolvedProductPtr> m_products;
    InstallOptions m_options;
    ProgressObserver * const m_observer;
    Logger m_logger;
    QHash<QString, QString> m_targetFilePathsMap;
};

} // namespace Internal
} // namespace qbs

#endif // Header guard
