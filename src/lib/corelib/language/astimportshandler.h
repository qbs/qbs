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
#ifndef QBS_ASTIMPORTSHANDLER_H
#define QBS_ASTIMPORTSHANDLER_H

#include "forward_decls.h"

#include <parser/qmljsastfwd_p.h>
#include <tools/set.h>

#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>

namespace qbs {
class CodeLocation;
class Version;

namespace Internal {
class ItemReaderVisitorState;
class JsImport;
class Logger;

class ASTImportsHandler
{
public:
    ASTImportsHandler(ItemReaderVisitorState &visitorState, Logger &logger,
                      const FileContextPtr &file);

    void handleImports(const QbsQmlJS::AST::UiImportList *uiImportList);

    QHash<QStringList, QString> typeNameFileMap() const { return m_typeNameToFile; }

private:
    static Version readImportVersion(const QString &str, const CodeLocation &location);

    bool addPrototype(const QString &fileName, const QString &filePath, const QString &as,
                      bool needsCheck);
    void checkImportVersion(const QbsQmlJS::AST::SourceLocation &versionToken) const;
    void collectPrototypes(const QString &path, const QString &as);
    void collectPrototypesAndJsCollections(const QString &path, const QString &as,
                                           const CodeLocation &location);
    void handleImport(const QbsQmlJS::AST::UiImport *import, bool *baseImported);

    ItemReaderVisitorState &m_visitorState;
    Logger &m_logger;
    const FileContextPtr &m_file;
    const QString m_directory;
    QHash<QStringList, QString> m_typeNameToFile;
    Set<QString> m_importAsNames;

    using JsImportsHash = QHash<QString, JsImport>;
    JsImportsHash m_jsImports;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard
