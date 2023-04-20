/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 BogDan Vatra <bogdan@kde.org>
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

#include "jsextension.h"

#include <language/scriptengine.h>

#include <QtCore/qfile.h>
#include <QtCore/qvariant.h>

#include <QtXml/qdom.h>

namespace qbs {
namespace Internal {

template<class C> class XmlDomNode : public JsExtension<XmlDomNode<C>>
{
public:
    static const char *name();
    XmlDomNode(const C &value) : m_value(value) {}
    XmlDomNode(JSContext *, const QString &name);
    XmlDomNode(JSContext *) {}

    static JSValue ctor(JSContext *ctx, JSValueConst, JSValueConst,
                        int argc, JSValueConst *argv, int);
    static void setupMethods(JSContext *ctx, JSValue obj);

    C value() const { return m_value; }

private:
#define XML_JS_FWD(name, func, text) DEFINE_JS_FORWARDER_QUAL(XmlDomNode, name, func, text)

    XML_JS_FWD(jsIsElement, &XmlDomNode::isElement, "DomNode.isElement");
    XML_JS_FWD(jsIsCDATASection, &XmlDomNode::isCDATASection, "DomNode.isCDATASection");
    XML_JS_FWD(jsIsText, &XmlDomNode::isText, "DomNode.isText");
    XML_JS_FWD(jsHasAttribute, &XmlDomNode::hasAttribute, "DomNode.hasAttribute");
    XML_JS_FWD(jsTagName, &XmlDomNode::tagName, "DomNode.tagName");
    XML_JS_FWD(jsSetTagName, &XmlDomNode::setTagName, "DomNode.setTagName");
    XML_JS_FWD(jsText, &XmlDomNode::text, "DomNode.text");
    XML_JS_FWD(jsData, &XmlDomNode::data, "DomNode.data");
    XML_JS_FWD(jsSetData, &XmlDomNode::setData, "DomNode.setData");
    XML_JS_FWD(jsClear, &XmlDomNode::clear, "DomNode.clear");
    XML_JS_FWD(jsHasAttributes, &XmlDomNode::hasAttributes, "DomNode.hasAttributes");
    XML_JS_FWD(jsHasChildNodes, &XmlDomNode::hasChildNodes, "DomNode.hasChildNodes");
    XML_JS_FWD(jsSetAttribute, &XmlDomNode::setAttribute, "DomNode.setAttribute");
    XML_JS_FWD(jsAppendChild, &XmlDomNode::appendChild, "DomNode.appendChild");
    XML_JS_FWD(jsInsertBefore, &XmlDomNode::insertBefore, "DomNode.insertBefore");
    XML_JS_FWD(jsInsertAfter, &XmlDomNode::insertAfter, "DomNode.insertAfter");
    XML_JS_FWD(jsReplaceChild, &XmlDomNode::replaceChild, "DomNode.replaceChild");
    XML_JS_FWD(jsRemoveChild, &XmlDomNode::removeChild, "DomNode.removeChild");

    static JSValue jsAttribute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
    {
        try {
            const auto name = JsExtension<XmlDomNode>::template getArgument<QString>
                    (ctx, "DomNode.attribute", argc, argv);
            QString defaultValue;
            if (argc > 1)
                defaultValue = JsExtension<XmlDomNode>::template fromArg<QString>
                        (ctx, "DomNode.attribute", 2, argv[1]);
            return makeJsString(ctx, JsExtension<XmlDomNode>::fromJsObject
                                (ctx, this_val)->attribute(name, defaultValue));
        } catch (const QString &error) {
            return throwError(ctx, error);
        }
    }
    static JSValue jsParentNode(JSContext *ctx, JSValueConst this_val, int, JSValueConst *)
    {
        return JsExtension<XmlDomNode<QDomNode>>::createObjectDirect(
                ctx, JsExtension<XmlDomNode>::fromJsObject(ctx, this_val)->parentNode());
    }
    static JSValue jsFirstChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
    {
        try {
            QString tagName;
            if (argc > 0)
                tagName = JsExtension<XmlDomNode>::template getArgument<QString>
                        (ctx, "DomNode.firstChild", argc, argv);
            return JsExtension<XmlDomNode<QDomNode>>::createObjectDirect
                    (ctx, JsExtension<XmlDomNode>::fromJsObject(ctx, this_val)->firstChild(tagName));
        } catch (const QString &error) {
            return throwError(ctx, error);
        }
    }
    static JSValue jsLastChild(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
    {
        try {
            QString tagName;
            if (argc > 0)
                tagName = JsExtension<XmlDomNode>::template getArgument<QString>
                        (ctx, "DomNode.lastChild", argc, argv);
            return JsExtension<XmlDomNode<QDomNode>>::createObjectDirect
                    (ctx, JsExtension<XmlDomNode>::fromJsObject(ctx, this_val)->lastChild(tagName));
        } catch (const QString &error) {
            return throwError(ctx, error);
        }
    }
    static JSValue jsPreviousSibling(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv)
    {
        try {
            QString tagName;
            if (argc > 0)
                tagName = JsExtension<XmlDomNode>::template getArgument<QString>
                        (ctx, "DomNode.previousSibling", argc, argv);
            return JsExtension<XmlDomNode<QDomNode>>::createObjectDirect(ctx, JsExtension<XmlDomNode>::fromJsObject
                                                            (ctx, this_val)->previousSibling(tagName));
        } catch (const QString &error) {
            return throwError(ctx, error);
        }
    }
    static JSValue jsNextSibling(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv)
    {
        try {
            QString tagName;
            if (argc > 0)
                tagName = JsExtension<C>::template getArgument<QString>
                        (ctx, "Xml.DomElement.nextSibling", argc, argv);
            return JsExtension<XmlDomNode<QDomNode>>::createObjectDirect
                    (ctx, JsExtension<XmlDomNode>::fromJsObject(ctx, this_val)->nextSibling(tagName));
        } catch (const QString &error) {
            return throwError(ctx, error);
        }
    }

    // Implemented for QDomDocument only.
    static JSValue jsDocumentElement(JSContext *ctx, JSValueConst this_val, int, JSValueConst *);
    static JSValue jsCreateElement(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv);
    static JSValue jsCreateCDATASection(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv);
    static JSValue jsCreateTextNode(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv);
    static JSValue jsSetContent(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue jsToString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue jsSave(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
    static JSValue jsLoad(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

    bool isElement() const { return m_value.isElement(); }
    bool isCDATASection() const { return m_value.isCDATASection(); }
    bool isText() const { return m_value.isText(); }
    QString attribute(const QString &name, const QString &defaultValue)
    {
        QDomElement el = m_value.toElement();
        if (el.isNull())
            throw QStringLiteral("Node '%1' is not an element node").arg(m_value.nodeName());
        return el.attribute(name, defaultValue);
    }
    void setAttribute(const QString &name, const QString &value)
    {
        QDomElement el = m_value.toElement();
        if (el.isNull())
            throw QStringLiteral("Node '%1' is not an element node").arg(m_value.nodeName());
        el.setAttribute(name, value);
    }
    bool hasAttribute(const QString &name) const
    {
        QDomElement el = m_value.toElement();
        if (el.isNull())
            throw QStringLiteral("Node '%1' is not an element node").arg(m_value.nodeName());
        return el.hasAttribute(name);
    }
    QString tagName() const
    {
        QDomElement el = m_value.toElement();
        if (el.isNull())
            throw QStringLiteral("Node '%1' is not an element node").arg(m_value.nodeName());
        return el.tagName();
    }
    void setTagName(const QString &name)
    {
        QDomElement el = m_value.toElement();
        if (el.isNull())
            throw QStringLiteral("Node '%1' is not an element node").arg(m_value.nodeName());
        el.setTagName(name);
    }

    QString text() const
    {
        QDomElement el = m_value.toElement();
        if (el.isNull())
            throw QStringLiteral("Node '%1' is not an element node").arg(m_value.nodeName());
        return el.text();
    }

    QString data() const
    {
        if (m_value.isText())
            return m_value.toText().data();
        if (m_value.isCDATASection())
            return m_value.toCDATASection().data();
        if (m_value.isCharacterData())
            return m_value.toCharacterData().data();
        throw QStringLiteral("Node '%1' is not a character data node").arg(m_value.nodeName());
    }
    void setData(const QString &v) const
    {
        if (m_value.isText())
            return m_value.toText().setData(v);
        if (m_value.isCDATASection())
            return m_value.toCDATASection().setData(v);
        if (m_value.isCharacterData())
            return m_value.toCharacterData().setData(v);
        throw QStringLiteral("Node '%1' is not a character data node").arg(m_value.nodeName());
    }
    void clear() { m_value.clear(); }
    bool hasAttributes() const { return m_value.hasAttributes(); }
    bool hasChildNodes() const { return m_value.hasChildNodes(); }

    XmlDomNode<QDomNode> *parentNode() const
    {
        return new XmlDomNode<QDomNode>(m_value.parentNode());
    }
    XmlDomNode<QDomNode> *firstChild(const QString &tagName)
    {
        if (tagName.isEmpty())
            return new XmlDomNode<QDomNode>(m_value.firstChild());
        return new XmlDomNode<QDomNode>(m_value.firstChildElement(tagName));
    }
    XmlDomNode<QDomNode> *lastChild(const QString &tagName) const
    {
        if (tagName.isEmpty())
            return new XmlDomNode<QDomNode>(m_value.lastChild());
        return new XmlDomNode<QDomNode>(m_value.lastChildElement(tagName));
    }
    XmlDomNode<QDomNode> *previousSibling(const QString &tagName) const
    {
        if (tagName.isEmpty())
            return new XmlDomNode<QDomNode>(m_value.previousSibling());
        return new XmlDomNode<QDomNode>(m_value.previousSiblingElement(tagName));
    }
    XmlDomNode<QDomNode> *nextSibling(const QString &tagName) const
    {
        if (tagName.isEmpty())
            return new XmlDomNode<QDomNode>(m_value.nextSibling());
        return new XmlDomNode<QDomNode>(m_value.nextSiblingElement(tagName));
    }
    void appendChild(const XmlDomNode<QDomNode> *newChild)
    {
        m_value.appendChild(newChild->value());
    }
    void insertBefore(const XmlDomNode<QDomNode> *newChild, const XmlDomNode<QDomNode> *refChild)
    {
        m_value.insertBefore(newChild->value(), refChild->value());
    }
    void insertAfter(const XmlDomNode<QDomNode> *newChild, const XmlDomNode<QDomNode> *refChild)
    {
        m_value.insertAfter(newChild->value(), refChild->value());
    }
    void replaceChild(const XmlDomNode<QDomNode> *newChild, const XmlDomNode<QDomNode> *oldChild)
    {
        m_value.replaceChild(newChild->value(), oldChild->value());
    }
    void removeChild(const XmlDomNode<QDomNode> *oldChild)
    {
        m_value.removeChild(oldChild->value());
    }

    // Implemented for QDomDocument only.
    XmlDomNode<QDomNode> *documentElement();
    XmlDomNode<QDomNode> *createElement(const QString &tagName);
    XmlDomNode<QDomNode> *createCDATASection(const QString &value);
    XmlDomNode<QDomNode> *createTextNode(const QString &value);

    bool setContent(const QString &content);
    QString toString(int indent = 1);

    void save(const QString &filePath, int indent = 1);
    void load(const QString &filePath);

    C m_value;
};

template<> const char *XmlDomNode<QDomNode>::name() { return "DomNode"; }
template<> const char *XmlDomNode<QDomDocument>::name() { return "DomDocument"; }

template<> JSValue XmlDomNode<QDomNode>::ctor(JSContext *ctx, JSValue, JSValue, int , JSValue *,
                                              int)
{
    const JSValue obj = createObject(ctx);
    const auto se = ScriptEngine::engineForContext(ctx);
    const DubiousContextList dubiousContexts{
        DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
    };
    se->checkContext(QStringLiteral("qbs.Xml.DomNode"), dubiousContexts);
    se->setUsesIo();
    return obj;
}

template<> JSValue XmlDomNode<QDomDocument>::ctor(JSContext *ctx, JSValue, JSValue,
                                                  int argc, JSValue *argv, int)
{
    try {
        JSValue obj;
        if (argc == 0) {
            obj = createObject(ctx);
        } else {
            const auto name = getArgument<QString>(ctx, "XmlDomDocument constructor",
                                                   argc, argv);
            obj = createObject(ctx, name);
        }
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts{
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
        };
        se->checkContext(QStringLiteral("qbs.Xml.DomDocument"), dubiousContexts);
        se->setUsesIo();
        return obj;
    } catch (const QString &error) { return throwError(ctx, error); }
}

template<> XmlDomNode<QDomNode> *XmlDomNode<QDomDocument>::documentElement()
{
    return new XmlDomNode<QDomNode>(m_value.documentElement());
}

template<> XmlDomNode<QDomNode> *XmlDomNode<QDomDocument>::createElement(const QString &tagName)
{
    return new XmlDomNode<QDomNode>(m_value.createElement(tagName));
}

template<> XmlDomNode<QDomNode> *XmlDomNode<QDomDocument>::createCDATASection(const QString &value)
{
    return new XmlDomNode<QDomNode>(m_value.createCDATASection(value));
}

template<> XmlDomNode<QDomNode> *XmlDomNode<QDomDocument>::createTextNode(const QString &value)
{
    return new XmlDomNode<QDomNode>(m_value.createTextNode(value));
}

template<> bool XmlDomNode<QDomDocument>::setContent(const QString &content)
{
    return static_cast<bool>(m_value.setContent(content));
}

template<> QString XmlDomNode<QDomDocument>::toString(int indent)
{
    return m_value.toString(indent);
}

template<> JSValue XmlDomNode<QDomDocument>::jsDocumentElement(JSContext *ctx, JSValue this_val,
                                                               int, JSValue *)
{
    return XmlDomNode<QDomNode>::createObjectDirect(
                ctx, fromJsObject(ctx, this_val)->documentElement());
}

template<> JSValue XmlDomNode<QDomDocument>::jsCreateElement(JSContext *ctx, JSValue this_val,
                                                             int argc, JSValue *argv)
{
    try {
        const auto tagName = getArgument<QString>(ctx, "DomDocument.createElement", argc, argv);
        const JSValue obj = XmlDomNode<QDomNode>::createObjectDirect(
                    ctx, fromJsObject(ctx, this_val)->createElement(tagName));
        return obj;
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

template<> JSValue XmlDomNode<QDomDocument>::jsCreateCDATASection(JSContext *ctx, JSValue this_val,
                                                                  int argc, JSValue *argv)
{
    try {
        const auto value = getArgument<QString>(ctx, "DomDocument.createCDATASection", argc, argv);
        return XmlDomNode<QDomNode>::createObjectDirect(
                    ctx, fromJsObject(ctx, this_val)->createCDATASection(value));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

template<> JSValue XmlDomNode<QDomDocument>::jsCreateTextNode(JSContext *ctx, JSValue this_val,
                                                              int argc, JSValue *argv)
{
    try {
        const auto value = getArgument<QString>(ctx, "DomDocument.createTextNode", argc, argv);
        return XmlDomNode<QDomNode>::createObjectDirect(
                    ctx, fromJsObject(ctx, this_val)->createTextNode(value));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

template<> JSValue XmlDomNode<QDomDocument>::jsSetContent(JSContext *ctx, JSValue this_val,
                                                          int argc, JSValue *argv)
{
    try {
        const auto content = getArgument<QString>(ctx, "DomDocument.setContent", argc, argv);
        return JS_NewBool(ctx, fromJsObject(ctx, this_val)->setContent(content));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

template<> JSValue XmlDomNode<QDomDocument>::jsToString(JSContext *ctx, JSValue this_val,
                                                        int argc, JSValue *argv)
{
    try {
        qint32 indent = 1;
        if (argc > 0)
            indent = getArgument<qint32>(ctx, "DomDocument.toString", argc, argv);
        return makeJsString(ctx, fromJsObject(ctx, this_val)->toString(indent));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

template<> void XmlDomNode<QDomDocument>::save(const QString &filePath, int indent)
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
        throw QStringLiteral("unable to open '%1'").arg(filePath);

    QByteArray buff(m_value.toByteArray(indent));
    if (buff.size() != f.write(buff))
        throw f.errorString();

    f.close();
    if (f.error() != QFile::NoError)
        throw f.errorString();
}

template<> JSValue XmlDomNode<QDomDocument>::jsSave(JSContext *ctx, JSValue this_val,
                                                    int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "DomDocument.save", argc, argv);
        qint32 indent = 1;
        if (argc > 1)
            indent = fromArg<qint32>(ctx, "toString", 2, argv[1]);
        fromJsObject(ctx, this_val)->save(filePath, indent);
        return JS_UNDEFINED;
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

template<> void XmlDomNode<QDomDocument>::load(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        throw QStringLiteral("unable to open '%1'").arg(filePath);

    QString errorMsg;
    if (!m_value.setContent(&f, &errorMsg))
        throw errorMsg;
}

template<> JSValue XmlDomNode<QDomDocument>::jsLoad(JSContext *ctx, JSValue this_val,
                                                    int argc, JSValue *argv)
{
    try {
        const auto filePath = getArgument<QString>(ctx, "DomDocument.load", argc, argv);
        fromJsObject(ctx, this_val)->load(filePath);
        return JS_UNDEFINED;
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

template<> XmlDomNode<QDomDocument>::XmlDomNode(JSContext *, const QString &name) : m_value(name) {}

template<class C>
void XmlDomNode<C>::setupMethods(JSContext *ctx, JSValue obj)
{
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "appendChild", &jsAppendChild, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "attribute", &jsAttribute, 2);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "clear", &jsClear, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "data", &jsData, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "firstChild", &jsFirstChild, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "hasAttribute", &jsHasAttribute, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "hasAttributes", &jsHasAttributes, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "hasChildNodes", &jsHasChildNodes, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "insertAfter", &jsInsertAfter, 2);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "insertBefore", &jsInsertBefore, 2);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "isCDATASection", &jsIsCDATASection, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "isElement", &jsIsElement, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "isText", &jsIsText, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "lastChild", &jsLastChild, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "nextSibling", &jsNextSibling, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "parentNode", &jsParentNode, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "previousSibling", &jsPreviousSibling, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "removeChild", &jsRemoveChild, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "replaceChild", &jsReplaceChild, 2);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "setAttribute", &jsSetAttribute, 2);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "setData", &jsSetData, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "setTagName", &jsSetTagName, 1);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "tagName", &jsTagName, 0);
    JsExtension<XmlDomNode>::setupMethod(ctx, obj, "text", &jsText, 0);
    if constexpr (std::is_same_v<C, QDomDocument>) {
        JsExtension<XmlDomNode>::setupMethod(ctx, obj, "documentElement", &jsDocumentElement, 0);
        JsExtension<XmlDomNode>::setupMethod(ctx, obj, "createElement", &jsCreateElement, 1);
        JsExtension<XmlDomNode>::setupMethod(ctx, obj, "createCDATASection",
                                             &jsCreateCDATASection, 1);
        JsExtension<XmlDomNode>::setupMethod(ctx, obj, "createTextNode", &jsCreateTextNode, 1);
        JsExtension<XmlDomNode>::setupMethod(ctx, obj, "setContent", &jsSetContent, 1);
        JsExtension<XmlDomNode>::setupMethod(ctx, obj, "toString", &jsToString, 1);
        JsExtension<XmlDomNode>::setupMethod(ctx, obj, "save", &jsSave, 2);
        JsExtension<XmlDomNode>::setupMethod(ctx, obj, "load", &jsLoad, 1);
    }
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionXml(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    using namespace qbs::Internal;
    JSValue contextObject = engine->newObject();
    XmlDomNode<QDomNode>::registerClass(engine, contextObject);
    XmlDomNode<QDomDocument>::registerClass(engine, contextObject);
    setJsProperty(engine->context(), extensionObject, QLatin1String("Xml"), contextObject);
}
