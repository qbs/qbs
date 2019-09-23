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

#ifndef QBS_PLUGINS_H
#define QBS_PLUGINS_H

#include "qbs_export.h"

#include <memory>
#include <string>
#include <vector>

namespace qbs {
namespace Internal {
class Logger;

typedef void (*QbsPluginLoadFunction)();
typedef void (*QbsPluginUnloadFunction)();

class QbsPluginManagerPrivate;

class QBS_EXPORT QbsPluginManager
{
public:
    ~QbsPluginManager();
    static QbsPluginManager *instance();
    void registerStaticPlugin(QbsPluginLoadFunction, QbsPluginUnloadFunction);
    void loadStaticPlugins();
    void unloadStaticPlugins();
    void loadPlugins(const std::vector<std::string> &paths, const Logger &logger);

protected:
    QbsPluginManager();

private:
    std::unique_ptr<QbsPluginManagerPrivate> d;
};

} // namespace Internal
} // namespace qbs

#ifdef QBS_STATIC_LIB
#define QBS_REGISTER_STATIC_PLUGIN(exportmacro, name, load, unload) \
    extern "C" bool qbs_static_plugin_register_##name = [] { \
        qbs::Internal::QbsPluginManager::instance()->registerStaticPlugin(load, unload); \
        return true; \
    }();
#else
#define QBS_REGISTER_STATIC_PLUGIN(exportmacro, name, load, unload) \
    exportmacro void QbsPluginLoad() { load(); } \
    exportmacro void QbsPluginUnload() { unload(); }
#endif

#endif
