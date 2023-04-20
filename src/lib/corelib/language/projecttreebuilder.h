/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#pragma once

#include "forward_decls.h"
#include "moduleproviderinfo.h"
#include "moduleproviderloader.h"
#include "qualifiedid.h"

#include <QString>
#include <QVariant>

namespace qbs {
class SetupProjectParameters;
namespace Internal {
class Evaluator;
class FileTime;
class Item;
class ItemPool;
class ProgressObserver;

using ModulePropertiesPerGroup = std::unordered_map<const Item *, QualifiedIdSet>;

// TODO: This class only needs to be known inside the ProjectResolver; no need to
//       instantiate them separately when they always appear together.
//       Possibly we can get rid of the Loader class altogether.
class ProjectTreeBuilder
{
public:
    ProjectTreeBuilder(const SetupProjectParameters &parameters, ItemPool &itemPool,
                       Evaluator &evaluator, Logger &logger);
    ~ProjectTreeBuilder();

    struct Result
    {
        struct ProductInfo
        {
            std::vector<ProbeConstPtr> probes;
            ModulePropertiesPerGroup modulePropertiesSetInGroups;
            ErrorInfo delayedError;
        };

        Item *root = nullptr;
        std::unordered_map<Item *, ProductInfo> productInfos;
        std::vector<ProbeConstPtr> projectProbes;
        StoredModuleProviderInfo storedModuleProviderInfo;
        Set<QString> qbsFiles;
        QVariantMap profileConfigs;
    };
    Result load();

    void setProgressObserver(ProgressObserver *progressObserver);
    void setSearchPaths(const QStringList &searchPaths);
    void setOldProjectProbes(const std::vector<ProbeConstPtr> &oldProbes);
    void setOldProductProbes(const QHash<QString, std::vector<ProbeConstPtr>> &oldProbes);
    void setLastResolveTime(const FileTime &time);
    void setStoredProfiles(const QVariantMap &profiles);
    void setStoredModuleProviderInfo(const StoredModuleProviderInfo &moduleProviderInfo);

private:
    class Private;
    Private * const d;
};

} // namespace Internal
} // namespace qbs
