/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include "scannerpluginmanager.h"
#include "logger.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDirIterator>
#include <QtCore/QLibrary>

namespace qbs {
ScannerPluginManager *ScannerPluginManager::instance()
{
    static ScannerPluginManager *i = new ScannerPluginManager;
    return i;
}

ScannerPluginManager::ScannerPluginManager()
{
}

QList<ScannerPlugin *> ScannerPluginManager::scannersForFileTag(const QString &fileTag)
{
    return instance()->m_scannerPlugins.value(fileTag);
}

void ScannerPluginManager::loadPlugins(const QStringList &pluginPaths)
{
    QStringList filters;
#if defined(Q_OS_WIN)
    QString sharedLibSuffix = ".dll";
#elif defined(Q_OS_DARWIN)
    QString sharedLibSuffix = ".dylib";
#else
    QString sharedLibSuffix = ".so";
#endif
    filters << "*" + sharedLibSuffix;

    foreach (const QString &pluginPath, pluginPaths) {
        qbsTrace("pluginmanager: loading plugins from '%s'.", qPrintable(QDir::toNativeSeparators(pluginPath)));
        QDirIterator it(pluginPath, filters, QDir::Files);
        while (it.hasNext()) {
            const QString fileName = it.next();
            QScopedPointer<QLibrary> lib(new QLibrary(fileName));
            if (!lib->load()) {
                qbsWarning("pluginmanager: couldn't load '%s'.",
                           qPrintable(QDir::toNativeSeparators(fileName)));
                continue;
            }

            getScanners_f getScanners = reinterpret_cast<getScanners_f>(lib->resolve("getScanners"));
            if (!getScanners) {
                qbsWarning("pluginmanager: couldn't resolve symbol in '%s'.",
                           qPrintable(QDir::toNativeSeparators(fileName)));
                continue;
            }

            ScannerPlugin **plugins = getScanners();
            if (plugins == 0) {
                qbsWarning("pluginmanager: no scanners returned from '%s'.",
                           qPrintable(QDir::toNativeSeparators(fileName)));
                continue;
            }

            qbsTrace("pluginmanager: scanner plugin '%s' loaded.",
                     qPrintable(QDir::toNativeSeparators(fileName)));

            int i = 0;
            while (plugins[i] != 0) {
                m_scannerPlugins[QString::fromLocal8Bit(plugins[i]->fileTag)] += plugins[i];
                ++i;
            }
            m_libs.append(lib.take());
        }
    }
}

} // namespace qbs
