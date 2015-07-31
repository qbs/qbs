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
#ifndef QBS_ITEMREADERVISITORSTATE_H
#define QBS_ITEMREADERVISITORSTATE_H

#include <logging/logger.h>

#include <QSet>
#include <QStack>
#include <QStringList>

namespace qbs {
namespace Internal {
class Item;
class ItemPool;

class ItemReaderVisitorState
{
public:
    ItemReaderVisitorState(Logger logger);
    ~ItemReaderVisitorState();

    QSet<QString> filesRead() const { return m_filesRead; }

    Item *readFile(const QString &filePath, const QStringList &searchPaths, ItemPool *itemPool);

    void cacheDirectoryEntries(const QString &dirPath, const QStringList &entries);
    bool findDirectoryEntries(const QString &dirPath, QStringList *entries) const;

private:
    Logger m_logger;
    QSet<QString> m_filesRead;
    QHash<QString, QStringList> m_directoryEntries;

    class ASTCache;
    ASTCache * const m_astCache;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
