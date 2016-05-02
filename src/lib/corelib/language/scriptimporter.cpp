/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "scriptimporter.h"

#include <parser/qmljsastfwd_p.h>
#include <parser/qmljsastvisitor_p.h>
#include <parser/qmljslexer_p.h>
#include <parser/qmljsparser_p.h>
#include <tools/error.h>

#include <QScriptValueIterator>

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

    bool m_first;
    bool m_barrier;
    QString m_suffix;
};


ScriptImporter::ScriptImporter(QScriptEngine *scriptEngine)
    : m_engine(scriptEngine)
{
}

// ### merge with Evaluator::handleEvaluationError
static ErrorInfo errorInfoFromScriptValue(const QScriptValue &value, const QString &filePath)
{
    if (!value.isError())
        return ErrorInfo(value.toString(), CodeLocation(filePath));

    return ErrorInfo(value.property(QStringLiteral("message")).toString(),
                     CodeLocation(value.property(QStringLiteral("fileName")).toString(),
                                  value.property(QStringLiteral("lineNumber")).toInt32(),
                                  false));
}

void ScriptImporter::importSourceCode(const QString &sourceCode, const QString &filePath,
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
    if (m_engine->hasUncaughtException())
        throw errorInfoFromScriptValue(result, filePath);
    copyProperties(result, targetObject);
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
