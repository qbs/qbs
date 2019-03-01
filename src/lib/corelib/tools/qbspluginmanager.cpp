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
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>
#include <tools/qbsassert.h>
#include <tools/qttools.h>

#include <QtCore/qdir.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qlibrary.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <memory>
#include <mutex>

namespace qbs {
namespace Internal {

struct QbsPlugin {
    QbsPluginLoadFunction load;
    QbsPluginUnloadFunction unload;
    bool loaded;
};

class QbsPluginManagerPrivate {
public:
    std::vector<QbsPlugin> staticPlugins;
    std::vector<QLibrary *> libs;
};

QbsPluginManager::QbsPluginManager()
    : d(new QbsPluginManagerPrivate)
{
}

QbsPluginManager::~QbsPluginManager()
{
    unloadStaticPlugins();

    for (QLibrary * const lib : qAsConst(d->libs)) {
        auto unload = reinterpret_cast<QbsPluginUnloadFunction>(lib->resolve("QbsPluginUnload"));
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
    if (none_of(d->staticPlugins, [&load](const QbsPlugin &p) { return p.load == load; }))
        d->staticPlugins.push_back(QbsPlugin { load, unload, false });
}

void QbsPluginManager::loadStaticPlugins()
{
    for (const auto &plugin : d->staticPlugins) {
        if (!plugin.loaded && plugin.load)
            plugin.load();
    }
}

void QbsPluginManager::unloadStaticPlugins()
{
    for (auto &plugin : d->staticPlugins) {
        if (plugin.loaded && plugin.unload)
            plugin.unload();
    }

    d->staticPlugins.clear();
}

void QbsPluginManager::loadPlugins(const std::vector<std::string> &pluginPaths,
                                   const Logger &logger)
{
    QStringList filters;

    if (HostOsInfo::isWindowsHost())
        filters << QStringLiteral("*.dll");
    else if (HostOsInfo::isMacosHost())
        filters << QStringLiteral("*.bundle") << QStringLiteral("*.dylib");
    else
        filters << QStringLiteral("*.so");

    for (const std::string &pluginPath : pluginPaths) {
        const QString qtPluginPath = QString::fromStdString(pluginPath);
        qCDebug(lcPluginManager) << "loading plugins from"
                                 << QDir::toNativeSeparators(qtPluginPath);
        QDirIterator it(qtPluginPath, filters, QDir::Files);
        while (it.hasNext()) {
            const QString fileName = it.next();
            std::unique_ptr<QLibrary> lib(new QLibrary(fileName));
            if (!lib->load()) {
                logger.qbsWarning() << Tr::tr("plugin manager: Cannot load plugin '%1': %2")
                                       .arg(QDir::toNativeSeparators(fileName), lib->errorString());
                continue;
            }

            auto load = reinterpret_cast<QbsPluginLoadFunction>(lib->resolve("QbsPluginLoad"));
            if (load) {
                load();
                qCDebug(lcPluginManager) << "plugin" << QDir::toNativeSeparators(fileName)
                                         << "loaded.";
                d->libs.push_back(lib.release());
            } else {
                logger.qbsWarning() << Tr::tr("plugin manager: not a qbs plugin");
            }
        }
    }
}

} // namespace Internal
} // namespace qbs
