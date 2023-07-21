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

#ifndef QBS_ITEMREADER_H
#define QBS_ITEMREADER_H

#include <logging/logger.h>
#include <tools/deprecationwarningmode.h>
#include <tools/set.h>

#include <QtCore/qstringlist.h>

#include <memory>

namespace qbs {
class SetupProjectParameters;
namespace Internal {
class Evaluator;
class Item;
class ItemPool;
class ItemReaderVisitorState;
class LoaderState;

/*
 * Reads a qbs file and creates a tree of Item objects.
 *
 * In this stage the following steps are performed:
 *    - The QML/JS parser creates the AST.
 *    - The AST is converted to a tree of Item objects.
 *
 * This class is also responsible for the QMLish inheritance semantics.
 */
class ItemReader
{
public:
    ItemReader(LoaderState &loaderState);
    ~ItemReader();
    void init();

    void pushExtraSearchPaths(const QStringList &extraSearchPaths);
    void popExtraSearchPaths();
    const std::vector<QStringList> &extraSearchPathsStack() const;
    void setExtraSearchPathsStack(const std::vector<QStringList> &s);
    void clearExtraSearchPathsStack();
    const QStringList &allSearchPaths() const;

    // Parses a file, creates an item for it, generates PropertyDeclarations from
    // PropertyOptions items and removes said items from the item tree.
    Item *setupItemFromFile(const QString &filePath, const CodeLocation &referencingLocation);

    Item *wrapInProjectIfNecessary(Item *item);
    QStringList readExtraSearchPaths(Item *item, bool *wasSet = nullptr);

    qint64 elapsedTime() const { return m_elapsedTime; }

private:
    void setSearchPaths(const QStringList &searchPaths);
    Item *readFile(const QString &filePath);
    Item *readFile(const QString &filePath, const CodeLocation &referencingLocation);
    void handlePropertyOptions(Item *optionsItem);
    void handleAllPropertyOptionsItems(Item *item);

    LoaderState &m_loaderState;
    QStringList m_searchPaths;
    std::vector<QStringList> m_extraSearchPaths;
    mutable QStringList m_allSearchPaths;
    std::unique_ptr<ItemReaderVisitorState> m_visitorState;
    QString m_projectFilePath;
    qint64 m_elapsedTime = -1;
};

class SearchPathsManager {
public:
    SearchPathsManager(ItemReader &itemReader, const QStringList &extraSearchPaths = {});
    ~SearchPathsManager();

private:
    ItemReader &m_itemReader;
    size_t m_oldSize{0};
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEMREADER_H
