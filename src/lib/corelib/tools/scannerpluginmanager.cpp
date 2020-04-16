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

#include "scannerpluginmanager.h"

#include <language/filetags.h>

#include <plugins/scanner/scanner.h>

namespace qbs {
namespace Internal {

class ScannerPluginManagerPrivate {
public:
    std::map<FileTag, std::vector<ScannerPlugin* >> scannerPlugins;
};

ScannerPluginManager::~ScannerPluginManager() = default;

ScannerPluginManager *ScannerPluginManager::instance()
{
    static ScannerPluginManager scannerPlugin;
    return &scannerPlugin;
}

ScannerPluginManager::ScannerPluginManager()
    : d(new ScannerPluginManagerPrivate)
{
}

std::vector<ScannerPlugin *> ScannerPluginManager::scannersForFileTag(const FileTag &fileTag)
{
    auto it = instance()->d->scannerPlugins.find(fileTag);
    if (it != instance()->d->scannerPlugins.cend())
        return it->second;
    return {};
}

void ScannerPluginManager::registerPlugins(ScannerPlugin **plugins)
{
    for (int i = 0; plugins[i] != nullptr; ++i) {
        const FileTags &fileTags = FileTags::fromStringList(
                    QString::fromLatin1(plugins[i]->fileTags).split(QLatin1Char(',')));
        for (const FileTag &tag : fileTags)
            d->scannerPlugins[tag].push_back(plugins[i]);
    }
}

} // namespace Internal
} // namespace qbs
