/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qbs.
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
#ifndef QBS_ASTIMPORTSHANDLER_H
#define QBS_ASTIMPORTSHANDLER_H

#include "forward_decls.h"

#include <parser/qmljsastfwd_p.h>

#include <QHash>
#include <QSet>
#include <QStringList>

namespace qbs {
class CodeLocation;

namespace Internal {
class ItemReaderVisitorState;
class JsImport;
class Logger;
class Version;

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
    void handleImport(const QbsQmlJS::AST::UiImport *import);

    ItemReaderVisitorState &m_visitorState;
    Logger &m_logger;
    const FileContextPtr &m_file;
    const QString m_directory;
    QHash<QStringList, QString> m_typeNameToFile;
    QSet<QString> m_importAsNames;

    using JsImportsHash = QHash<QString, JsImport>;
    JsImportsHash m_jsImports;
};

} // namespace Internal
} // namespace qbs

#endif // Include guard
