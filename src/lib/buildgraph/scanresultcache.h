/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef QBS_SCANRESULTCACHE_H
#define QBS_SCANRESULTCACHE_H

#include <QHash>
#include <QSet>
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
        Dependency() : m_isLocal(false), m_isClean(true) {}
        Dependency(const QString &filePath, bool m_isLocal);

        QString filePath() const { return m_dirPath.isEmpty() ? m_fileName : m_dirPath + QLatin1Char('/') + m_fileName; }
        const QString &dirPath() const { return m_dirPath; }
        const QString &fileName() const { return m_fileName; }
        bool isLocal() const { return m_isLocal; }
        bool isClean() const { return m_isClean; }

    private:
        QString m_dirPath;
        QString m_fileName;
        bool m_isLocal;
        bool m_isClean;
    };

    class Result
    {
    public:
        Result()
            : valid(false)
        {}

        QVector<Dependency> deps;
        bool valid;
    };

    Result value(const QString &fileName) const;
    void insert(const QString &fileName, const Result &value);
    void remove(const QString &filePath);

private:
    QHash<QString, Result> m_data;
};

} // namespace qbs
} // namespace qbs

#endif // QBS_SCANRESULTCACHE_H
