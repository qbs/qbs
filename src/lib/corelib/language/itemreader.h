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

#ifndef QBS_ITEMREADER_H
#define QBS_ITEMREADER_H

#include "forward_decls.h"
#include <logging/logger.h>

#include <QSet>
#include <QStack>
#include <QStringList>

namespace qbs {
namespace Internal {

class Item;
class ItemPool;
class ItemReaderVisitorState;

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
    ItemReader(const Logger &logger);
    ~ItemReader();

    void setPool(ItemPool *pool) { m_pool = pool; }
    void setSearchPaths(const QStringList &searchPaths);
    void pushExtraSearchPaths(const QStringList &extraSearchPaths);
    void popExtraSearchPaths();
    QStack<QStringList> extraSearchPathsStack() const;
    void setExtraSearchPathsStack(const QStack<QStringList> &s) { m_extraSearchPaths = s; }
    void clearExtraSearchPathsStack() { m_extraSearchPaths.clear(); }
    QStringList searchPaths() const;

    Item *readFile(const QString &filePath);

    QSet<QString> filesRead() const;

private:
    ItemPool *m_pool = nullptr;
    QStringList m_searchPaths;
    QStack<QStringList> m_extraSearchPaths;
    ItemReaderVisitorState * const m_visitorState;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEMREADER_H
