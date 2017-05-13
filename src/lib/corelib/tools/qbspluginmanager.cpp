/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qbspluginmanager.h"

#include <logging/logger.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qdiriterator.h>
#include <QtCore/qlibrary.h>

#include <mutex>

namespace qbs {
namespace Internal {

QbsPluginManager::QbsPluginManager()
{
}

QbsPluginManager::~QbsPluginManager()
{
    unloadStaticPlugins();

    for (QLibrary * const lib : qAsConst(m_libs)) {
        QbsPluginUnloadFunction unload = reinterpret_cast<QbsPluginUnloadFunction>(
                    lib->resolve("QbsPluginUnload"));
        if (unload)
            unload();
        lib->unload();
        delete lib;
    }
}

QbsPluginManager *QbsPluginManager::instance()
{
    static QbsPluginManager instance;
    return &instance;
}

void QbsPluginManager::registerStaticPlugin(QbsPluginLoadFunction load,
                                            QbsPluginUnloadFunction unload)
{
    auto begin = m_staticPlugins.cbegin(), end = m_staticPlugins.cend();
    if (std::find_if(begin, end, [&load](const QbsPlugin &p) { return p.load == load; }) == end)
        m_staticPlugins.push_back(QbsPlugin { load, unload, false });
}

void QbsPluginManager::loadStaticPlugins()
{
    for (const auto &plugin : m_staticPlugins) {
        if (!plugin.loaded && plugin.load)
            plugin.load();
    }
}

void QbsPluginManager::unloadStaticPlugins()
{
    for (auto &plugin : m_staticPlugins) {
        if (plugin.loaded && plugin.unload)
            plugin.unload();
    }

    m_staticPlugins.clear();
}

void QbsPluginManager::loadPlugins(const QStringList &pluginPaths, const Logger &logger)
{
    QStringList filters;

    if (HostOsInfo::isWindowsHost())
        filters << QLatin1String("*.dll");
    else if (HostOsInfo::isMacosHost())
        filters << QLatin1String("*.dylib");
    else
        filters << QLatin1String("*.so");

    for (const QString &pluginPath : pluginPaths) {
        logger.qbsTrace() << QString::fromLatin1("plugin manager: loading plugins from '%1'.")
                             .arg(QDir::toNativeSeparators(pluginPath));
        QDirIterator it(pluginPath, filters, QDir::Files);
        while (it.hasNext()) {
            const QString fileName = it.next();
            QScopedPointer<QLibrary> lib(new QLibrary(fileName));
            if (!lib->load()) {
                logger.qbsWarning() << Tr::tr("plugin manager: Cannot load plugin '%1': %2")
                                       .arg(QDir::toNativeSeparators(fileName), lib->errorString());
                continue;
            }

            QbsPluginLoadFunction load = reinterpret_cast<QbsPluginLoadFunction>(
                        lib->resolve("QbsPluginLoad"));
            if (load) {
                load();
                logger.qbsTrace() << QString::fromLatin1("pluginmanager: plugin '%1' loaded.")
                                     .arg(QDir::toNativeSeparators(fileName));
                m_libs.push_back(lib.take());
            } else {
                logger.qbsWarning() << Tr::tr("plugin manager: not a qbs plugin");
            }
        }
    }
}

} // namespace Internal
} // namespace qbs
