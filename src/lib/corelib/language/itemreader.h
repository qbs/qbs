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

#include "forward_decls.h"
#include <logging/logger.h>
#include <tools/set.h>

#include <QtCore/qstringlist.h>

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
    ItemReader(Logger &logger);
    ~ItemReader();

    void setPool(ItemPool *pool) { m_pool = pool; }
    void setSearchPaths(const QStringList &searchPaths);
    void pushExtraSearchPaths(const QStringList &extraSearchPaths);
    void popExtraSearchPaths();
    std::vector<QStringList> extraSearchPathsStack() const;
    void setExtraSearchPathsStack(const std::vector<QStringList> &s);
    void clearExtraSearchPathsStack();
    const QStringList &allSearchPaths() const;

    Item *readFile(const QString &filePath);

    Set<QString> filesRead() const;

    void setEnableTiming(bool on);
    qint64 elapsedTime() const { return m_elapsedTime; }

private:
    ItemPool *m_pool = nullptr;
    QStringList m_searchPaths;
    std::vector<QStringList> m_extraSearchPaths;
    mutable QStringList m_allSearchPaths;
    ItemReaderVisitorState * const m_visitorState;
    qint64 m_elapsedTime = -1;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_ITEMREADER_H
