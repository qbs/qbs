/**************************************************************************
**
** This file is part of the Qt Build Suite
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file.
** Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**************************************************************************/

#include <QtTest/QtTest>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptProgram>

using namespace QmlJS::AST;

class Function
{
public:
    QString name;
    QString code;
};

class Property
{
public:
    QString name;
    QString code;
    QList<QStringList> dependencies;
    QList<QStringList> sideEffects;
};

class Object
{
public:
    QString prototype;
    QList<Object> objects;
    QHash<QString, Property> properties;
    QHash<QString, Function> functions;
};

template <typename T>
static QString textOf(const QString &source, T *node)
{
    return source.mid(node->firstSourceLocation().begin(), node->lastSourceLocation().end() - node->firstSourceLocation().begin());
}

static QList<QStringList> stripLocals(const QList<QStringList> &deps, const QStringList &locals)
{
    QList<QStringList> result;
    foreach (const QStringList &dep, deps) {
        if (!locals.contains(dep.first()))
            result.append(dep);
    }
    return result;
}

// run on a statement/expression to find its deps and side effect targets
class DependencyFinder : protected QmlJS::AST::Visitor
{
public:
    DependencyFinder();

    void operator()(Node *node)
    {
        m_deps.clear();
        m_sideEffects.clear();
        m_locals.clear();
        Node::accept(node, this);
        m_deps = stripLocals(m_deps, m_locals);
        m_sideEffects = stripLocals(m_sideEffects, m_locals);
    }

    QList<QStringList> dependencies() const
    {
        return m_deps;
    }

    QList<QStringList> sideEffects() const
    {
        return m_sideEffects;
    }

protected:
    using Visitor::visit;

    // local vars, not external deps
    bool visit(VariableDeclaration *ast);
    bool visit(FunctionDeclaration *ast);
    bool visit(FunctionExpression *ast);

    // deps
    bool visit(IdentifierExpression *ast);
    bool visit(FieldMemberExpression *ast);
    bool visit(ArrayMemberExpression *ast);

    // side effects
    bool visit(BinaryExpression *ast);
    bool visit(PreIncrementExpression *ast);
    bool visit(PostIncrementExpression *ast);
    bool visit(PreDecrementExpression *ast);
    bool visit(PostDecrementExpression *ast);

    void sideEffectOn(ExpressionNode *ast);

private:
    QList<QSOperator::Op> m_assigningOps;

    QStringList m_locals;
    QList<QStringList> m_deps;
    QList<QStringList> m_sideEffects;
};

DependencyFinder::DependencyFinder()
{
    m_assigningOps << QSOperator::Assign
                   << QSOperator::InplaceAnd
                   << QSOperator::InplaceSub
                   << QSOperator::InplaceDiv
                   << QSOperator::InplaceAdd
                   << QSOperator::InplaceLeftShift
                   << QSOperator::InplaceMod
                   << QSOperator::InplaceMul
                   << QSOperator::InplaceOr
                   << QSOperator::InplaceRightShift
                   << QSOperator::InplaceURightShift
                   << QSOperator::InplaceXor;
}

bool DependencyFinder::visit(VariableDeclaration *ast)
{
    if (ast->name)
        m_locals.append(ast->name->asString());
    return true;
}

bool DependencyFinder::visit(FunctionDeclaration *ast)
{
    if (ast->name)
        m_locals.append(ast->name->asString());
    return visit(static_cast<FunctionExpression *>(ast));
}

bool DependencyFinder::visit(FunctionExpression *ast)
{
    const QStringList outerLocals = m_locals;
    m_locals.clear();
    m_locals += QLatin1String("arguments");
    if (ast->name)
        m_locals += ast->name->asString();
    for (FormalParameterList *it = ast->formals; it; it = it->next) {
        if (it->name)
            m_locals += it->name->asString();
    }

    const QList<QStringList> outerDeps = m_deps;
    const QList<QStringList> outerSideEffects = m_sideEffects;
    m_deps.clear();
    m_sideEffects.clear();

    Node::accept(ast->body, this);

    m_deps = stripLocals(m_deps, m_locals);
    m_deps += outerDeps;

    m_sideEffects = stripLocals(m_sideEffects, m_locals);
    m_sideEffects += outerSideEffects;

    m_locals = outerLocals;
    return false;
}

bool DependencyFinder::visit(IdentifierExpression *ast)
{
    if (ast->name)
        m_deps += QStringList() << ast->name->asString();
    return true;
}

class GetMemberAccess : protected Visitor
{
    QStringList m_result;

public:
    // gets the longest static member access, i.e.
    // foo.bar["abc"] -> foo,bar,abc
    // foo.bar[aaa].def -> foo.bar
    QStringList operator()(ExpressionNode *ast)
    {
        m_result.clear();
        Node::accept(ast, this);
        return m_result;
    }

protected:
    bool preVisit(Node *ast)
    {
        if (cast<FieldMemberExpression *>(ast)
                || cast<ArrayMemberExpression *>(ast)
                || cast<IdentifierExpression *>(ast))
            return true;
        m_result.clear();
        return false;
    }

    bool visit(FieldMemberExpression *ast)
    {
        if (ast->name) {
            m_result.prepend(ast->name->asString());
            return true;
        }
        m_result.clear();
        return true;
    }

    bool visit(ArrayMemberExpression *ast)
    {
        if (StringLiteral *string = cast<StringLiteral *>(ast->expression)) {
            m_result.prepend(string->value->asString());
            return true;
        }
        m_result.clear();
        return true;
    }

    bool visit(IdentifierExpression *ast)
    {
        if (ast->name) {
            m_result.prepend(ast->name->asString());
            return true;
        }
        m_result.clear();
        return true;
    }
};

bool DependencyFinder::visit(FieldMemberExpression *ast)
{
    const QStringList dep = GetMemberAccess()(ast);
    if (!dep.isEmpty())
        m_deps += dep;
    return false;
}

bool DependencyFinder::visit(ArrayMemberExpression *ast)
{
    const QStringList dep = GetMemberAccess()(ast);
    if (!dep.isEmpty())
        m_deps += dep;
    return false;
}

void DependencyFinder::sideEffectOn(ExpressionNode *ast)
{
    QStringList assignee = GetMemberAccess()(ast);
    if (!assignee.isEmpty())
        m_sideEffects += assignee;
    // ### does this catch everything?
}

bool DependencyFinder::visit(BinaryExpression *ast)
{
    if (!m_assigningOps.contains(static_cast<QSOperator::Op>(ast->op)))
        return true;

    sideEffectOn(ast->left);

    // need to collect dependencies regardless, consider a = b = c = d
    return true;
}

bool DependencyFinder::visit(PreDecrementExpression *ast)
{
    sideEffectOn(ast->expression);
    return true;
}

bool DependencyFinder::visit(PostDecrementExpression *ast)
{
    sideEffectOn(ast->base);
    return true;
}

bool DependencyFinder::visit(PreIncrementExpression *ast)
{
    sideEffectOn(ast->expression);
    return true;
}

bool DependencyFinder::visit(PostIncrementExpression *ast)
{
    sideEffectOn(ast->base);
    return true;
}

static QHash<QString, Function> bindFunctions(const QString &source, UiObjectInitializer *ast)
{
    QHash<QString, Function> result;
    for (UiObjectMemberList *it = ast->members; it; it = it->next) {
        UiSourceElement *sourceElement = cast<UiSourceElement *>(it->member);
        if (!sourceElement)
            continue;
        FunctionDeclaration *fdecl = cast<FunctionDeclaration *>(sourceElement->sourceElement);
        if (!fdecl)
            continue;

        Function f;
        if (!fdecl->name)
            throw Exception("function decl without name");
        f.name = fdecl->name->asString();
        f.code = textOf(source, fdecl);
        result.insert(f.name, f);
    }
    return result;
}

static QHash<QString, Property> bindProperties(const QString &source, UiObjectInitializer *ast)
{
    QHash<QString, Property> result;
    for (UiObjectMemberList *it = ast->members; it; it = it->next) {
        if (UiScriptBinding *scriptBinding = cast<UiScriptBinding *>(it->member)) {
            Property p;
            if (!scriptBinding->qualifiedId || !scriptBinding->qualifiedId->name || scriptBinding->qualifiedId->next)
                throw Exception("script binding without name or name with dots");
            p.name = scriptBinding->qualifiedId->name->asString();
            p.code = textOf(source, scriptBinding->statement);
            DependencyFinder finder;
            finder(scriptBinding->statement);
            p.dependencies = finder.dependencies();
            p.sideEffects = finder.sideEffects();
            result.insert(p.name, p);
        }
    }
    return result;
}

static Object bindObject(const QString &source, UiObjectDefinition *ast);

static QList<Object> bindObjects(const QString &source, UiObjectInitializer *ast)
{
    QList<Object> result;
    for (UiObjectMemberList *it = ast->members; it; it = it->next) {
        if (UiObjectDefinition *objDef = cast<UiObjectDefinition *>(it->member)) {
            result += bindObject(source, objDef);
        }
    }
    return result;
}

static Object bindObject(const QString &source, UiObjectDefinition *ast)
{
    Object result;

    if (!ast->qualifiedTypeNameId || !ast->qualifiedTypeNameId->name || ast->qualifiedTypeNameId->next)
        throw Exception("no prototype or prototype with dots");
    result.prototype = ast->qualifiedTypeNameId->name->asString();

    result.functions = bindFunctions(source, ast->initializer);
    result.properties = bindProperties(source, ast->initializer);
    result.objects = bindObjects(source, ast->initializer);

    return result;
}


class Loader
{
public:
    Object readFile(const QString &fileName)
    {
        Object emptyReturn;

        QFile file(fileName);
        if (!file.open(QFile::ReadOnly)) {
            qWarning() << "Couldn't open" << fileName;
            return emptyReturn;
        }

        const QString code = QTextStream(&file).readAll();
        QScopedPointer<QmlJS::Engine> engine(new QmlJS::Engine);
        QScopedPointer<QmlJS::NodePool> nodePool(new QmlJS::NodePool(fileName, engine.data()));
        QmlJS::Lexer lexer(engine.data());
        lexer.setCode(code, 1);
        QmlJS::Parser parser(engine.data());
        parser.parse();
        QList<QmlJS::DiagnosticMessage> parserMessages = parser.diagnosticMessages();
        if (!parserMessages.isEmpty()) {
            foreach (const QmlJS::DiagnosticMessage &msg, parserMessages)
                qWarning() << fileName << ":" << msg.loc.startLine << ": " << msg.message;
            return emptyReturn;
        }

        // extract dependencies without resolving them
        UiObjectDefinition *rootDef = cast<UiObjectDefinition *>(parser.ast()->members->member);
        if (!rootDef)
            return emptyReturn;
        return bindObject(code, rootDef);
    }
};

class TestDepFinder : public QObject
{
    Q_OBJECT
private slots:
    void test1();
    void evaluationOrder_data();
    void evaluationOrder();
};

void TestDepFinder::test1()
{
    Loader loader;
    Object object = loader.readFile(SRCDIR "test1.qbs");
    QCOMPARE(object.properties.size(), 3);

    Property includePaths = object.properties.value("includePaths");
    QCOMPARE(includePaths.dependencies.size(), 2);
    QCOMPARE(includePaths.dependencies.at(0), QStringList() << "foo");
    QCOMPARE(includePaths.dependencies.at(1), QStringList() << "abc" << "def");
    QVERIFY(includePaths.sideEffects.isEmpty());

    Property foo = object.properties.value("foo");
    QCOMPARE(foo.dependencies.size(), 1);
    QCOMPARE(foo.dependencies.at(0), QStringList() << "bar");
    QVERIFY(foo.sideEffects.isEmpty());

    Property car = object.properties.value("car");
    QCOMPARE(car.dependencies.size(), 1);
    QCOMPARE(car.dependencies.at(0), QStringList() << "includePaths");
    QVERIFY(car.sideEffects.isEmpty());
}

class ResolvedProperty
{
public:
    typedef QSharedPointer<ResolvedProperty> Ptr;
    const Object *object;
    const Property *property;
    QList<ResolvedProperty::Ptr> dependencies;
};

static QList<ResolvedProperty::Ptr> resolveProperties(const Object &object)
{
    QHash<QString, ResolvedProperty::Ptr> result;

    // build initial resolved property list
    foreach (const Property &property, object.properties) {
        ResolvedProperty::Ptr resolvedProperty(new ResolvedProperty);
        resolvedProperty->object = &object;
        resolvedProperty->property = &property;
        result.insert(property.name, resolvedProperty);
    }

    // add dependency links
    QHashIterator<QString, ResolvedProperty::Ptr> it(result);
    while (it.hasNext()) {
        it.next();
        ResolvedProperty::Ptr resolvedProperty = it.value();
        foreach (const QStringList &dependency, resolvedProperty->property->dependencies) {
            ResolvedProperty::Ptr rDepProp = result.value(dependency.first());
            // ### TODO This needs serious extension:
            // ### check globals (QScriptEngine), object.functions, parent objects?
            if (!rDepProp) {
                throw Exception(QString("property %1 has dependency on nonexistent property %2").arg(
                                    it.key(), dependency.first()));
            }
            resolvedProperty->dependencies += rDepProp;
        }
    }

    return result.values();
}

static void resolveDependencies(const ResolvedProperty::Ptr &property,
                                QList<ResolvedProperty::Ptr> *target,
                                QSet<ResolvedProperty::Ptr> *done,
                                QSet<ResolvedProperty::Ptr> *parents)
{
    if (done->contains(property))
        return;
    parents->insert(property);
    foreach (const ResolvedProperty::Ptr &dep, property->dependencies) {
        if (parents->contains(dep))
            throw Exception("dependency cycle");
        resolveDependencies(dep, target, done, parents);
    }
    target->append(property);
    done->insert(property);
    parents->remove(property);
}

static QList<ResolvedProperty::Ptr> sortForEvaluation(const QList<ResolvedProperty::Ptr> &input)
{
    QList<ResolvedProperty::Ptr> result;
    QSet<ResolvedProperty::Ptr> done;
    QSet<ResolvedProperty::Ptr> parents;

    foreach (const ResolvedProperty::Ptr &property, input) {
        resolveDependencies(property, &result, &done, &parents);
    }

    return result;
}

void TestDepFinder::evaluationOrder_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QStringList>("expectedOrder");
    QTest::newRow("no dependencies")
            << QString(SRCDIR "order_nodeps.qbs")
            << (QStringList() << "foo" << "bar");
    QTest::newRow("basic1")
            << QString(SRCDIR "order_basic1.qbs")
            << (QStringList() << "bar" << "foo");
    QTest::newRow("basic2")
            << QString(SRCDIR "order_basic2.qbs")
            << (QStringList() << "car" << "bar" << "foo");
    QTest::newRow("complex1")
            << QString(SRCDIR "order_complex1.qbs")
            << (QStringList() << "blub" << "def" << "foo" << "abc" << "car" << "bar");
}

void TestDepFinder::evaluationOrder()
{
    QFETCH(QString, fileName);
    QFETCH(QStringList, expectedOrder);

    Loader loader;
    Object object = loader.readFile(fileName);
    QCOMPARE(object.properties.size(), expectedOrder.size());

    QList<ResolvedProperty::Ptr> resolvedProperties = resolveProperties(object);
    QList<ResolvedProperty::Ptr> evaluationOrder = sortForEvaluation(resolvedProperties);

    for (int i = 0; i < evaluationOrder.size(); ++i) {
        QCOMPARE(evaluationOrder[i]->property->name, expectedOrder[i]);
    }
}

QTEST_MAIN(TestDepFinder)

#include "tst_dependencyFinder.moc"
