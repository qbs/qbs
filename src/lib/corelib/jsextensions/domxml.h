/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 BogDan Vatra <bogdan@kde.org>
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

#ifndef DOMXML_H
#define DOMXML_H

#include <QDomDocument>
#include <QDomNode>
#include <QObject>
#include <QScriptValue>
#include <QScriptable>
#include <QVariant>

namespace qbs {
namespace Internal {

class XmlDomDocument;

void initializeJsExtensionXml(QScriptValue extensionObject);

class XmlDomNode: public QObject, public QScriptable
{
    Q_OBJECT
public:
    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);

    Q_INVOKABLE bool isElement() const;
    Q_INVOKABLE bool isCDATASection() const;
    Q_INVOKABLE bool isText() const;

    Q_INVOKABLE QString attribute(const QString & name, const QString & defValue = QString());
    Q_INVOKABLE void setAttribute(const QString & name, const QString & value);
    Q_INVOKABLE bool hasAttribute(const QString & name) const;
    Q_INVOKABLE QString tagName() const;
    Q_INVOKABLE void setTagName(const QString & name);

    Q_INVOKABLE QString text() const;

    Q_INVOKABLE QString data() const;
    Q_INVOKABLE void setData(const QString &v) const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE bool hasAttributes() const;
    Q_INVOKABLE bool hasChildNodes() const;
    Q_INVOKABLE QScriptValue parentNode() const;
    Q_INVOKABLE QScriptValue firstChild(const QString & tagName = QString());
    Q_INVOKABLE QScriptValue lastChild(const QString & tagName = QString()) const;
    Q_INVOKABLE QScriptValue previousSibling(const QString & tagName = QString()) const;
    Q_INVOKABLE QScriptValue nextSibling(const QString & tagName = QString()) const;

    Q_INVOKABLE QScriptValue appendChild(QScriptValue newChild);
    Q_INVOKABLE QScriptValue insertBefore(const QScriptValue& newChild, const QScriptValue& refChild);
    Q_INVOKABLE QScriptValue insertAfter(const QScriptValue& newChild, const QScriptValue& refChild);
    Q_INVOKABLE QScriptValue replaceChild(const QScriptValue& newChild, const QScriptValue& oldChild);
    Q_INVOKABLE QScriptValue removeChild(const QScriptValue& oldChild);

protected:
    friend class XmlDomDocument;
    XmlDomNode(const QDomNode &other = QDomNode());
    QDomNode m_domNode;
};

class XmlDomDocument: public XmlDomNode
{
    Q_OBJECT
public:
    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);
    Q_INVOKABLE QScriptValue documentElement();
    Q_INVOKABLE QScriptValue createElement(const QString & tagName);
    Q_INVOKABLE QScriptValue createCDATASection(const QString & value);
    Q_INVOKABLE QScriptValue createTextNode(const QString & value);

    Q_INVOKABLE bool setContent(const QString & content);
    Q_INVOKABLE QString toString(int indent = 1);

    Q_INVOKABLE void save(const QString & filePath, int indent = 1);
    Q_INVOKABLE void load(const QString & filePath);

protected:
    XmlDomDocument(QScriptContext *context, const QString &name = QString());

private:
    QDomDocument m_domDocument;
};

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::XmlDomDocument *)
Q_DECLARE_METATYPE(qbs::Internal::XmlDomNode *)

#endif // DOMXML_H
