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

#ifndef QBS_FILECONTEXTBASE_H
#define QBS_FILECONTEXTBASE_H

#include "jsimports.h"

namespace qbs {
namespace Internal {

class FileContextBase
{
    friend class ItemReaderASTVisitor;

public:
    void setFilePath(const QString &filePath);
    QString filePath() const;

    void setContent(const QString &content);
    const QString &content() const;

    void setJsImports(const JsImports &jsImports);
    JsImports jsImports() const;

    void setJsExtensions(const QStringList &extensions);
    QStringList jsExtensions() const;

    void setSearchPaths(const QStringList &paths);
    QStringList searchPaths() const;

    QString dirPath() const;

protected:
    FileContextBase() {}
    FileContextBase(const FileContextBase &other);

    QString m_filePath;
    QString m_content;
    JsImports m_jsImports;
    QStringList m_jsExtensions;
    QStringList m_searchPaths;
};

inline void FileContextBase::setFilePath(const QString &filePath)
{
    m_filePath = filePath;
}

inline QString FileContextBase::filePath() const
{
    return m_filePath;
}

inline void FileContextBase::setContent(const QString &content)
{
    m_content = content;
}

inline const QString &FileContextBase::content() const
{
    return m_content;
}

inline void FileContextBase::setJsImports(const JsImports &jsImports)
{
    m_jsImports = jsImports;
}

inline JsImports FileContextBase::jsImports() const
{
    return m_jsImports;
}

inline void FileContextBase::setJsExtensions(const QStringList &extensions)
{
    m_jsExtensions = extensions;
}

inline QStringList FileContextBase::jsExtensions() const
{
    return m_jsExtensions;
}

inline void FileContextBase::setSearchPaths(const QStringList &paths)
{
    m_searchPaths = paths;
}

inline QStringList FileContextBase::searchPaths() const
{
    return m_searchPaths;
}

} // namespace Internal
} // namespace qbs

#endif // QBS_FILECONTEXTBASE_H
