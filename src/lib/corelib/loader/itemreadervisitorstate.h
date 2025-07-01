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
#ifndef QBS_ITEMREADERVISITORSTATE_H
#define QBS_ITEMREADERVISITORSTATE_H

#include <tools/codelocation.h>
#include <tools/deprecationwarningmode.h>

#include <QtCore/qstringlist.h>

namespace qbs {
namespace Internal {
class Item;
class ItemPool;
class ItemReaderCache;
class LoaderState;
class Logger;

class ItemReaderVisitorState
{
public:
    ItemReaderVisitorState(LoaderState &loaderState);

    Logger &logger() const { return m_logger; }

    Item *readFile(const QString &filePath, const QStringList &searchPaths, ItemPool *itemPool);

    void findDirectoryEntries(const QString &dirPath, QStringList *entries) const;

    Item *mostDerivingItem() const;
    void setMostDerivingItem(Item *item);

    DeprecationWarningMode deprecationWarningMode() const;

    void addCodeLink(
        const QString &sourceFile, const CodeRange &sourceRange, const CodeLocation &targetLoc);

private:
    LoaderState &m_loaderState;
    ItemReaderCache &m_cache;
    Logger &m_logger;
    Item *m_mostDerivingItem = nullptr;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard.
