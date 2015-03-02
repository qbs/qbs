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

#ifndef QBS_ASTTOOLS_H
#define QBS_ASTTOOLS_H

#include <parser/qmljsastfwd_p.h>
#include <tools/codelocation.h>
#include <QStringList>

namespace qbs {
namespace Internal {

QStringList toStringList(QbsQmlJS::AST::UiQualifiedId *qid);
CodeLocation toCodeLocation(const QString &filePath, const QbsQmlJS::AST::SourceLocation &location);
QString textOf(const QString &source, QbsQmlJS::AST::Node *node);
QStringRef textRefOf(const QString &source, QbsQmlJS::AST::Node *node);

} // namespace Internal
} // namespace qbs

#endif // QBS_ASTTOOLS_H
