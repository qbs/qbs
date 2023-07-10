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

#include "itemreader.h"

#include "itemreadervisitorstate.h"
#include "loaderutils.h"

#include <language/deprecationinfo.h>
#include <language/evaluator.h>
#include <language/filecontext.h>
#include <language/item.h>
#include <language/value.h>
#include <logging/categories.h>
#include <tools/fileinfo.h>
#include <tools/profiling.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>
#include <tools/stlutils.h>

#include <QtCore/qfileinfo.h>

#include <algorithm>

namespace qbs {
namespace Internal {

static void makePathsCanonical(QStringList &paths)
{
    Internal::removeIf(paths, [](QString &p) {
        p = QFileInfo(p).canonicalFilePath();
        return p.isEmpty();
    });
}

ItemReader::ItemReader(LoaderState &loaderState) : m_loaderState(loaderState) {}

void ItemReader::init()
{
    m_visitorState = std::make_unique<ItemReaderVisitorState>(
        m_loaderState.topLevelProject().itemReaderCache(), m_loaderState.logger());
    m_visitorState->setDeprecationWarningMode(m_loaderState.parameters().deprecationWarningMode());
    m_projectFilePath  = m_loaderState.parameters().projectFilePath();
    setSearchPaths(m_loaderState.parameters().searchPaths());
    m_elapsedTime = m_loaderState.parameters().logElapsedTime() ? 0 : -1;
}

ItemReader::~ItemReader() = default;

void ItemReader::setSearchPaths(const QStringList &searchPaths)
{
    qCDebug(lcModuleLoader) << "initial search paths:" << searchPaths;
    m_searchPaths = searchPaths;
    makePathsCanonical(m_searchPaths);
    m_allSearchPaths.clear();
}

void ItemReader::pushExtraSearchPaths(const QStringList &extraSearchPaths)
{
    m_extraSearchPaths.push_back(extraSearchPaths);
    makePathsCanonical(m_extraSearchPaths.back());
    m_allSearchPaths.clear();
}

void ItemReader::popExtraSearchPaths()
{
    m_extraSearchPaths.pop_back();
    m_allSearchPaths.clear();
}

const std::vector<QStringList> &ItemReader::extraSearchPathsStack() const
{
    return m_extraSearchPaths;
}

void ItemReader::setExtraSearchPathsStack(const std::vector<QStringList> &s)
{
    m_extraSearchPaths = s;
    m_allSearchPaths.clear();
}

void ItemReader::clearExtraSearchPathsStack()
{
    m_extraSearchPaths.clear();
    m_allSearchPaths.clear();
}

const QStringList &ItemReader::allSearchPaths() const
{
    if (m_allSearchPaths.empty()) {
        std::for_each(m_extraSearchPaths.crbegin(), m_extraSearchPaths.crend(),
                      [this] (const QStringList &paths) {
            m_allSearchPaths += paths;
        });
        m_allSearchPaths += m_searchPaths;
        m_allSearchPaths.removeDuplicates();
    }
    return m_allSearchPaths;
}

Item *ItemReader::readFile(const QString &filePath)
{
    AccumulatingTimer readFileTimer(m_elapsedTime != -1 ? &m_elapsedTime : nullptr);
    return m_visitorState->readFile(filePath, allSearchPaths(), &m_loaderState.itemPool());
}

Item *ItemReader::readFile(const QString &filePath, const CodeLocation &referencingLocation)
{
    try {
        return readFile(filePath);
    } catch (const ErrorInfo &e) {
        if (e.hasLocation())
            throw;
        throw ErrorInfo(e.toString(), referencingLocation);
    }
}

void ItemReader::handlePropertyOptions(Item *optionsItem)
{
    Evaluator &evaluator = m_loaderState.evaluator();
    const QString name = evaluator.stringValue(optionsItem, StringConstants::nameProperty());
    if (name.isEmpty()) {
        throw ErrorInfo(Tr::tr("PropertyOptions item needs a name property"),
                        optionsItem->location());
    }
    const QString description = evaluator.stringValue(
        optionsItem, StringConstants::descriptionProperty());
    const auto removalVersion = Version::fromString(
        evaluator.stringValue(optionsItem, StringConstants::removalVersionProperty()));
    const auto allowedValues = evaluator.stringListValue(
        optionsItem, StringConstants::allowedValuesProperty());

    PropertyDeclaration decl = optionsItem->parent()->propertyDeclaration(name);
    if (!decl.isValid()) {
        decl.setName(name);
        decl.setType(PropertyDeclaration::Variant);
    }
    decl.setDescription(description);
    if (removalVersion.isValid()) {
        DeprecationInfo di(removalVersion, description);
        decl.setDeprecationInfo(di);
    }
    decl.setAllowedValues(allowedValues);
    const ValuePtr property = optionsItem->parent()->property(name);
    if (!property && !decl.isExpired()) {
        throw ErrorInfo(Tr::tr("PropertyOptions item refers to non-existing property '%1'")
                            .arg(name), optionsItem->location());
    }
    if (property && decl.isExpired()) {
        ErrorInfo e(Tr::tr("Property '%1' was scheduled for removal in version %2, but "
                           "is still present.")
                        .arg(name, removalVersion.toString()),
                    property->location());
        e.append(Tr::tr("Removal version for '%1' specified here.").arg(name),
                 optionsItem->location());
        m_visitorState->logger().printWarning(e);
    }
    optionsItem->parent()->setPropertyDeclaration(name, decl);
}

void ItemReader::handleAllPropertyOptionsItems(Item *item)
{
    QList<Item *> childItems = item->children();
    auto childIt = childItems.begin();
    while (childIt != childItems.end()) {
        Item * const child = *childIt;
        if (child->type() == ItemType::PropertyOptions) {
            handlePropertyOptions(child);
            childIt = childItems.erase(childIt);
        } else {
            handleAllPropertyOptionsItems(child);
            ++childIt;
        }
    }
    item->setChildren(childItems);
}

Item *ItemReader::setupItemFromFile(const QString &filePath, const CodeLocation &referencingLocation)
{
    Item *item = readFile(filePath, referencingLocation);

    // This is technically not needed, because files are only set up once and then served
    // from a cache. But it simplifies the checks in item.cpp if we require the locking invariant
    // to always hold.
    std::unique_ptr<ModuleItemLocker> locker;
    if (item->type() == ItemType::Module)
        locker = std::make_unique<ModuleItemLocker>(*item);

    handleAllPropertyOptionsItems(item);
    return item;
}

Item *ItemReader::wrapInProjectIfNecessary(Item *item)
{
    if (item->type() == ItemType::Project)
        return item;
    Item *prj = Item::create(&m_loaderState.itemPool(), ItemType::Project);
    Item::addChild(prj, item);
    prj->setFile(item->file());
    prj->setLocation(item->location());
    prj->setupForBuiltinType(m_loaderState.parameters().deprecationWarningMode(),
                             m_visitorState->logger());
    return prj;
}

QStringList ItemReader::readExtraSearchPaths(Item *item, bool *wasSet)
{
    QStringList result;
    const QStringList paths = m_loaderState.evaluator().stringListValue(
        item, StringConstants::qbsSearchPathsProperty(), wasSet);
    const JSSourceValueConstPtr prop = item->sourceProperty(
        StringConstants::qbsSearchPathsProperty());

    // Value can come from within a project file or as an overridden value from the user
    // (e.g command line).
    const QString basePath = FileInfo::path(prop ? prop->file()->filePath()
                                                 : m_projectFilePath);
    for (const QString &path : paths)
        result += FileInfo::resolvePath(basePath, path);
    return result;
}

SearchPathsManager::SearchPathsManager(ItemReader &itemReader, const QStringList &extraSearchPaths)
    : m_itemReader(itemReader),
      m_oldSize(itemReader.extraSearchPathsStack().size())
{
    if (!extraSearchPaths.isEmpty())
        m_itemReader.pushExtraSearchPaths(extraSearchPaths);
}

SearchPathsManager::~SearchPathsManager()
{
    while (m_itemReader.extraSearchPathsStack().size() > m_oldSize)
        m_itemReader.popExtraSearchPaths();
}

} // namespace Internal
} // namespace qbs
