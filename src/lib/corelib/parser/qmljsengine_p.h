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

#ifndef QMLJSENGINE_P_H
#define QMLJSENGINE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qmljsglobal_p.h"
#include "qmljsastfwd_p.h"
#include "qmljsmemorypool_p.h"
#include <tools/qbs_export.h>

#include <QtCore/qstring.h>

#include <set>
#include <utility>

namespace QbsQmlJS {

class Lexer;
class Directives;
class MemoryPool;

class QML_PARSER_EXPORT DiagnosticMessage
{
public:
    enum Kind { Warning, Error };

    DiagnosticMessage()
        : kind(Error) {}

    DiagnosticMessage(Kind kind, const AST::SourceLocation &loc, QString message)
        : kind(kind), loc(loc), message(std::move(message)) {}

    bool isWarning() const
    { return kind == Warning; }

    bool isError() const
    { return kind == Error; }

    Kind kind;
    AST::SourceLocation loc;
    QString message;
};

class QBS_AUTOTEST_EXPORT Engine
{
    Lexer *_lexer{nullptr};
    Directives *_directives{nullptr};
    MemoryPool _pool;
    QList<AST::SourceLocation> _comments;
    QString _extraCode;
    QString _code;

public:
    Engine();
    ~Engine();

    void setCode(const QString &code);

    void addComment(int pos, int len, int line, int col);
    QList<AST::SourceLocation> comments() const;

    Lexer *lexer() const;
    void setLexer(Lexer *lexer);

    void setDirectives(Directives *directives);
    Directives *directives() const;

    MemoryPool *pool();

    inline QStringRef midRef(int position, int size) { return _code.midRef(position, size); }

    QStringRef newStringRef(const QString &s);
    QStringRef newStringRef(const QChar *chars, int size);
};

double integerFromString(const char *buf, int size, int radix);

} // end of namespace QbsQmlJS

#endif // QMLJSENGINE_P_H
