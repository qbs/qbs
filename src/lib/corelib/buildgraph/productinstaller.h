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
#ifndef QBS_PRODUCT_INSTALLER_H
#define QBS_PRODUCT_INSTALLER_H

#include "forward_decls.h"

#include <language/forward_decls.h>
#include <logging/logger.h>
#include <tools/installoptions.h>

#include <QList>

namespace qbs {
namespace Internal {
class ProgressObserver;

class ProductInstaller
{
public:
    ProductInstaller(const TopLevelProjectPtr &project, const QList<ResolvedProductPtr> &products,
            const InstallOptions &options, ProgressObserver *observer, const Logger &logger);
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
    const QList<ResolvedProductPtr> m_products;
    InstallOptions m_options;
    ProgressObserver * const m_observer;
    Logger m_logger;
};

} // namespace Internal
} // namespace qbs

#endif // Header guard
