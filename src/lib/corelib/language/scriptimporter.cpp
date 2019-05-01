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

#include "scriptimporter.h"

#include "evaluator.h"
#include "scriptengine.h"

#include <parser/qmljsastfwd_p.h>
#include <parser/qmljsastvisitor_p.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/error.h>

#include <QtScript/qscriptvalueiterator.h>

namespace qbs {
namespace Internal {

class IdentifierExtractor : private QbsQmlJS::AST::Visitor
{
public:
    void start(QbsQmlJS::AST::Node *node)
    {
        m_first = true;
        m_barrier = false;
        m_suffix += QLatin1String("\nreturn {");
        if (node)
            node->accept(this);
        m_suffix += QLatin1String("}})()");
    }

    const QString &suffix() const { return m_suffix; }

private:
    bool visit(QbsQmlJS::AST::SourceElements *) override
    {
        // Only consider the top level of source elements.
        if (m_barrier)
            return false;
        m_barrier = true;
        return true;
    }

    void endVisit(QbsQmlJS::AST::SourceElements *) override
    {
        m_barrier = false;
    }

    bool visit(QbsQmlJS::AST::FunctionSourceElement *fse) override
    {
        add(fse->declaration->name);
        return false;
    }

    bool visit(QbsQmlJS::AST::VariableDeclaration *vd) override
    {
        add(vd->name);
        return false;
    }

    void add(const QStringRef &name)
    {
        if (m_first) {
            m_first = false;
            m_suffix.reserve(m_suffix.length() + name.length() * 2 + 1);
        } else {
            m_suffix.reserve(m_suffix.length() + name.length() * 2 + 2);
            m_suffix += QLatin1Char(',');
        }
        m_suffix += name;
        m_suffix += QLatin1Char(':');
        m_suffix += name;
    }

    bool m_first = false;
    bool m_barrier = false;
    QString m_suffix;
};


ScriptImporter::ScriptImporter(ScriptEngine *scriptEngine)
    : m_engine(scriptEngine)
{
}

QScriptValue ScriptImporter::importSourceCode(const QString &sourceCode, const QString &filePath,
        QScriptValue &targetObject)
{
    Q_ASSERT(targetObject.isObject());
    // The targetObject doesn't get overwritten but enhanced by the contents of the .js file.
    // This is necessary for library imports that consist of multiple js files.

    QString &code = m_sourceCodeCache[filePath];
    if (code.isEmpty()) {
        QbsQmlJS::Engine engine;
        QbsQmlJS::Lexer lexer(&engine);
        lexer.setCode(sourceCode, 1, false);
        QbsQmlJS::Parser parser(&engine);
        if (!parser.parseProgram()) {
            throw ErrorInfo(parser.errorMessage(), CodeLocation(filePath, parser.errorLineNumber(),
                                                                parser.errorColumnNumber()));
        }

        IdentifierExtractor extractor;
        extractor.start(parser.rootNode());
        code = QLatin1String("(function(){\n") + sourceCode + extractor.suffix();
    }

    QScriptValue result = m_engine->evaluate(code, filePath, 0);
    throwOnEvaluationError(m_engine, result, [&filePath] () { return CodeLocation(filePath, 0); });
    copyProperties(result, targetObject);
    return result;
}

void ScriptImporter::copyProperties(const QScriptValue &src, QScriptValue &dst)
{
    QScriptValueIterator it(src);
    while (it.hasNext()) {
        it.next();
        dst.setProperty(it.name(), it.value());
    }
}

} // namespace Internal
} // namespace qbs
