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

#include "resolvedfilecontext.h"

#include "jsimports.h"
#include <tools/persistence.h>

namespace qbs {
namespace Internal {

ResolvedFileContext::ResolvedFileContext(const FileContextBase &ctx)
    : FileContextBase(ctx)
{
}

void ResolvedFileContext::load(PersistentPool &pool)
{
    m_filePath = pool.idLoadString();
    m_jsExtensions = pool.idLoadStringList();
    m_searchPaths = pool.idLoadStringList();
    int count;
    pool.stream() >> count;
    for (int i = 0; i < count; ++i) {
        JsImport jsi;
        jsi.scopeName = pool.idLoadString();
        jsi.filePaths = pool.idLoadStringList();
        jsi.location.load(pool);
        m_jsImports << jsi;
    }
}

void ResolvedFileContext::store(PersistentPool &pool) const
{
    pool.storeString(m_filePath);
    pool.storeStringList(m_jsExtensions);
    pool.storeStringList(m_searchPaths);
    pool.stream() << m_jsImports.count();
    foreach (const JsImport &jsi, m_jsImports) {
        pool.storeString(jsi.scopeName);
        pool.storeStringList(jsi.filePaths);
        jsi.location.store(pool);
    }
}

bool operator==(const ResolvedFileContext &a, const ResolvedFileContext &b)
{
    return a.filePath() == b.filePath()
            && a.jsExtensions().toSet() == b.jsExtensions().toSet()
            && a.jsImports().toSet() == b.jsImports().toSet();
}

} // namespace Internal
} // namespace qbs
