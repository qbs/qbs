/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Petroules Corporation.
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

#ifndef QBS_PROPERTYLIST_H
#define QBS_PROPERTYLIST_H

#include <qglobal.h>

#ifndef Q_OS_MAC

#include <QScriptEngine>

namespace qbs {
namespace Internal {

// provide a fake initializer for other platforms
void initializeJsExtensionPropertyList(QScriptValue extensionObject)
{
    // provide a fake object
    QScriptEngine *engine = extensionObject.engine();
    extensionObject.setProperty(QLatin1String("PropertyList"), engine->newObject());
}

} // namespace Internal
} // namespace qbs

#else // Q_OS_MAC

#include <QObject>
#include <QScriptable>
#include <QScriptValue>
#include <QString>
#include <QVariant>

namespace qbs {
namespace Internal {

void initializeJsExtensionPropertyList(QScriptValue extensionObject);

class PropertyListPrivate;

class PropertyList : public QObject, public QScriptable
{
    Q_OBJECT
public:
    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);
    PropertyList(QScriptContext *context);
    ~PropertyList();
    Q_INVOKABLE bool isEmpty() const;
    Q_INVOKABLE void clear();
    Q_INVOKABLE void readFromObject(const QScriptValue &value);
    Q_INVOKABLE void readFromString(const QString &input);
    Q_INVOKABLE void readFromFile(const QString &filePath);
    Q_INVOKABLE void writeToFile(const QString &filePath, const QString &plistFormat);
    Q_INVOKABLE QScriptValue format() const;
    Q_INVOKABLE QScriptValue toObject() const;
    Q_INVOKABLE QString toString(const QString &plistFormat) const;
    Q_INVOKABLE QString toXMLString() const;
    Q_INVOKABLE QString toJSON(const QString &style = QString()) const;
private:
    PropertyListPrivate *d;
};

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::PropertyList *)

#endif // Q_OS_MAC

#endif // QBS_PROPERTYLIST_H
