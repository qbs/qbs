/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2015 Petroules Corporation.
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

#ifndef QBS_PROPERTYLIST_H
#define QBS_PROPERTYLIST_H

#include <QtCore/qglobal.h>

// ### remove when qbs requires qbs 1.6 to build itself
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0) && defined(__APPLE__) && !defined(Q_OS_MAC)
#define Q_OS_MAC
#endif

#ifndef Q_OS_MAC

#include <QtScript/qscriptengine.h>

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

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptvalue.h>

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
    Q_INVOKABLE void readFromData(const QByteArray &data);
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
