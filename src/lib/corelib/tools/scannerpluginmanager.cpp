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

#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>

#include <QCoreApplication>
#include <QDirIterator>
#include <QLibrary>

namespace qbs {
namespace Internal {

ScannerPluginManager::~ScannerPluginManager()
{
    foreach (QLibrary * const lib, m_libs) {
        lib->unload();
        delete lib;
    }
}

ScannerPluginManager *ScannerPluginManager::instance()
{
    static ScannerPluginManager scannerPlugin;
    return &scannerPlugin;
}

ScannerPluginManager::ScannerPluginManager()
{
}

QList<ScannerPlugin *> ScannerPluginManager::scannersForFileTag(const FileTag &fileTag)
{
    return instance()->m_scannerPlugins.value(fileTag);
}

void ScannerPluginManager::loadPlugins(const QStringList &pluginPaths, const Logger &logger)
{
    QStringList filters;

    if (HostOsInfo::isWindowsHost())
        filters << QLatin1String("*.dll");
    else if (HostOsInfo::isMacosHost())
        filters << QLatin1String("*.dylib");
    else
        filters << QLatin1String("*.so");

    foreach (const QString &pluginPath, pluginPaths) {
        logger.qbsTrace() << QString::fromLatin1("pluginmanager: loading plugins from '%1'.")
                             .arg(QDir::toNativeSeparators(pluginPath));
        QDirIterator it(pluginPath, filters, QDir::Files);
        while (it.hasNext()) {
            const QString fileName = it.next();
            QScopedPointer<QLibrary> lib(new QLibrary(fileName));
            if (!lib->load()) {
                logger.qbsWarning() << Tr::tr("Pluginmanager: Cannot load plugin '%1': %2")
                                       .arg(QDir::toNativeSeparators(fileName), lib->errorString());
                continue;
            }

            getScanners_f getScanners = reinterpret_cast<getScanners_f>(lib->resolve("getScanners"));
            if (!getScanners) {
                logger.qbsWarning() << Tr::tr("Pluginmanager: Cannot resolve "
                        "symbol in '%1'.").arg(QDir::toNativeSeparators(fileName));
                continue;
            }

            ScannerPlugin **plugins = getScanners();
            if (plugins == 0) {
                logger.qbsWarning() << Tr::tr("pluginmanager: no scanners "
                        "returned from '%1'.").arg(QDir::toNativeSeparators(fileName));
                continue;
            }

            logger.qbsTrace() << QString::fromLatin1("pluginmanager: scanner plugin '%1' loaded.")
                                 .arg(QDir::toNativeSeparators(fileName));

            for (int i = 0; plugins[i] != 0; ++i)
                m_scannerPlugins[FileTag(plugins[i]->fileTag)] += plugins[i];
            m_libs.append(lib.take());
        }
    }
}

} // namespace Internal
} // namespace qbs
