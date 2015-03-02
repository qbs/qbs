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
    else if (HostOsInfo::isOsxHost())
        filters << QLatin1String("*.dylib");
    else
        filters << QLatin1String("*.so");

    foreach (const QString &pluginPath, pluginPaths) {
        logger.qbsTrace() << QString::fromLocal8Bit("pluginmanager: loading plugins from '%1'.")
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

            logger.qbsTrace() << QString::fromLocal8Bit("pluginmanager: scanner plugin '%1' "
                    "loaded.").arg(QDir::toNativeSeparators(fileName));

            for (int i = 0; plugins[i] != 0; ++i)
                m_scannerPlugins[FileTag(plugins[i]->fileTag)] += plugins[i];
            m_libs.append(lib.take());
        }
    }
}

} // namespace Internal
} // namespace qbs
