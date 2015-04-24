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

#ifndef QBS_SCANRESULTCACHE_H
#define QBS_SCANRESULTCACHE_H

#include <language/filetags.h>

#include <QHash>
#include <QString>
#include <QVector>

namespace qbs {
namespace Internal {

class ScanResultCache
{
public:
    class Dependency
    {
    public:
        Dependency() : m_isClean(true) {}
        Dependency(const QString &filePath);

        QString filePath() const { return m_dirPath.isEmpty() ? m_fileName : m_dirPath + QLatin1Char('/') + m_fileName; }
        const QString &dirPath() const { return m_dirPath; }
        const QString &fileName() const { return m_fileName; }
        bool isClean() const { return m_isClean; }

    private:
        QString m_dirPath;
        QString m_fileName;
        bool m_isClean;
    };

    class Result
    {
    public:
        Result()
            : valid(false)
        {}

        QVector<Dependency> deps;
        FileTags additionalFileTags;
        bool valid;
    };

    Result value(const void* scanner, const QString &fileName) const;
    void insert(const void* scanner, const QString &fileName, const Result &value);
    void remove(const QString &fileName);

private:
    typedef QHash<const void*, QHash<QString, Result> > ScanResultCacheData;
    ScanResultCacheData m_data;
};

} // namespace qbs
} // namespace qbs

#endif // QBS_SCANRESULTCACHE_H
