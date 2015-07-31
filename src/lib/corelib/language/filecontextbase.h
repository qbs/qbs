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
public:
    void setFilePath(const QString &filePath) { m_filePath = filePath; }
    QString filePath() const { return m_filePath; }

    void setContent(const QString &content) { m_content = content; }
    const QString &content() const { return m_content; }

    void addJsImport(const JsImport &jsImport) { m_jsImports << jsImport; }
    JsImports jsImports() const { return m_jsImports; }

    void addJsExtension(const QString &extension) { m_jsExtensions << extension; }
    QStringList jsExtensions() const { return m_jsExtensions; }

    void setSearchPaths(const QStringList &paths) { m_searchPaths = paths; }
    QStringList searchPaths() const { return m_searchPaths; }

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

} // namespace Internal
} // namespace qbs

#endif // QBS_FILECONTEXTBASE_H
