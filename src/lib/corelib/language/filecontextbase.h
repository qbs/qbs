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

    void addJsImport(const JsImport &jsImport) { m_jsImports.push_back(jsImport); }
    const JsImports &jsImports() const { return m_jsImports; }

    void addJsExtension(const QString &extension) { m_jsExtensions << extension; }
    QStringList jsExtensions() const { return m_jsExtensions; }

    void setSearchPaths(const QStringList &paths) { m_searchPaths = paths; }
    QStringList searchPaths() const { return m_searchPaths; }

    QString dirPath() const;

protected:
    FileContextBase() = default;
    FileContextBase(const FileContextBase &other) = default;
    FileContextBase(FileContextBase &&other) = default;

    QString m_filePath;
    JsImports m_jsImports;
    QStringList m_jsExtensions;
    QStringList m_searchPaths;
};

} // namespace Internal
} // namespace qbs

#endif // QBS_FILECONTEXTBASE_H
