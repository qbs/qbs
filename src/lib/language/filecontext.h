/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QBS_FILECONTEXT_H
#define QBS_FILECONTEXT_H

#include "item.h"
#include "jsimports.h"

#include <QStringList>

namespace qbs {
namespace Internal {

class FileContext
{
    friend class ItemReaderASTVisitor;

    FileContext();

public:
    static FileContextPtr create();

    QString filePath() const;
    QString dirPath() const;
    JsImports jsImports() const;
    QStringList jsExtensions() const;

    Item *idScope() const;

private:
    QString m_filePath;
    JsImports m_jsImports;
    QStringList m_jsExtensions;
    Item *m_idScope;
};

inline QString FileContext::filePath() const
{
    return m_filePath;
}

inline JsImports FileContext::jsImports() const
{
    return m_jsImports;
}

inline QStringList FileContext::jsExtensions() const
{
    return m_jsExtensions;
}

inline Item *FileContext::idScope() const
{
    return m_idScope;
}

} // namespace Internal
} // namespace qbs

#endif // QBS_FILECONTEXT_H
