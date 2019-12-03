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

#ifndef QBS_MODULEMERGER_H
#define QBS_MODULEMERGER_H

#include "item.h"
#include "qualifiedid.h"

#include <logging/logger.h>
#include <tools/set.h>
#include <tools/version.h>

#include <QtCore/qhash.h>

namespace qbs {
namespace Internal {

class ModuleMerger {
public:
    static void merge(Logger &logger, Item *productItem, const QString &productName,
                      Item::Modules *topSortedModules);

private:
    ModuleMerger(Logger &logger, Item *productItem, const QString &productName,
                 const Item::Modules::iterator &modulesBegin,
                 const Item::Modules::iterator &modulesEnd);

    void appendPrototypeValueToNextChain(Item *moduleProto, const QString &propertyName,
            const ValuePtr &sv);
    void mergeModule(Item::PropertyMap *props, const Item::Module &m);
    void replaceItemInValues(QualifiedId moduleName, Item *containerItem, Item *toReplace);
    void start();

    static ValuePtr lastInNextChain(const ValuePtr &v);
    static const Item::Module *findModule(const Item *item, const QualifiedId &name);

    Logger &m_logger;
    Item * const m_productItem;
    Item::Module &m_mergedModule;
    Item *m_clonedModulePrototype = nullptr;
    Set<const Item *> m_seenInstances;
    Set<Item *> m_moduleInstanceContainers;
    const bool m_isBaseModule;
    const bool m_isShadowProduct;
    const Item::Modules::iterator m_modulesBegin;
    const Item::Modules::iterator m_modulesEnd;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_MODULEMERGER_H
