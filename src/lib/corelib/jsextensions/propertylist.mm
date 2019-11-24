/****************************************************************************
**
** Copyright (C) 2015 Petroules Corporation.
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

#include <language/scriptengine.h>
#include <tools/hostosinfo.h>

#include <QtCore/qfile.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qvariant.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>

// Same values as CoreFoundation and Foundation APIs
enum {
    QPropertyListOpenStepFormat = 1,
    QPropertyListXMLFormat_v1_0 = 100,
    QPropertyListBinaryFormat_v1_0 = 200,
    QPropertyListJSONFormat = 1000 // If this conflicts someday, just change it :)
};

namespace qbs {
namespace Internal {

class PropertyListPrivate;

class PropertyList : public QObject, public QScriptable
{
    Q_OBJECT
public:
    static QScriptValue ctor(QScriptContext *context, QScriptEngine *engine);
    PropertyList(QScriptContext *context);
    ~PropertyList() override;
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

class PropertyListPrivate
{
public:
    PropertyListPrivate();

    QVariant propertyListObject;
    int propertyListFormat;

    void readFromData(QScriptContext *context, const QByteArray &data);
    QByteArray writeToData(QScriptContext *context, const QString &format);
};

QScriptValue PropertyList::ctor(QScriptContext *context, QScriptEngine *engine)
{
    auto const se = static_cast<ScriptEngine *>(engine);
    const DubiousContextList dubiousContexts({
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
    });
    se->checkContext(QStringLiteral("qbs.PropertyList"), dubiousContexts);

    auto p = new PropertyList(context);
    QScriptValue obj = engine->newQObject(p, QScriptEngine::ScriptOwnership);
    return obj;
}

PropertyListPrivate::PropertyListPrivate()
    : propertyListObject(), propertyListFormat(0)
{
}

PropertyList::~PropertyList()
{
    delete d;
}

PropertyList::PropertyList(QScriptContext *context)
: d(new PropertyListPrivate)
{
    Q_UNUSED(context);
    Q_ASSERT(thisObject().engine() == engine());
}

bool PropertyList::isEmpty() const
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());
    return p->d->propertyListObject.isNull();
}

void PropertyList::clear()
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());
    p->d->propertyListObject = QVariant();
    p->d->propertyListFormat = 0;
}

void PropertyList::readFromObject(const QScriptValue &value)
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());
    p->d->propertyListObject = value.toVariant();
    p->d->propertyListFormat = 0; // wasn't deserialized from any external format
}

void PropertyList::readFromString(const QString &input)
{
    readFromData(input.toUtf8());
}

void PropertyList::readFromFile(const QString &filePath)
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray data = file.readAll();
        if (file.error() == QFile::NoError) {
            p->d->readFromData(p->context(), data);
            return;
        }
    }

    p->context()->throwError(QStringLiteral("%1: %2").arg(filePath).arg(file.errorString()));
}

void PropertyList::readFromData(const QByteArray &data)
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());
    p->d->readFromData(p->context(), data);
}

void PropertyList::writeToFile(const QString &filePath, const QString &plistFormat)
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());

    QFile file(filePath);
    QByteArray data = p->d->writeToData(p->context(), plistFormat);
    if (Q_LIKELY(!data.isEmpty())) {
        if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size()) {
            return;
        }
    }

    p->context()->throwError(QStringLiteral("%1: %2").arg(filePath).arg(file.errorString()));
}

QScriptValue PropertyList::format() const
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());
    switch (p->d->propertyListFormat)
    {
    case QPropertyListOpenStepFormat:
        return QStringLiteral("openstep");
    case QPropertyListXMLFormat_v1_0:
        return QStringLiteral("xml1");
    case QPropertyListBinaryFormat_v1_0:
        return QStringLiteral("binary1");
    case QPropertyListJSONFormat:
        return QStringLiteral("json");
    default:
        return p->engine()->undefinedValue();
    }
}

QScriptValue PropertyList::toObject() const
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());
    return p->engine()->toScriptValue(p->d->propertyListObject);
}

QString PropertyList::toString(const QString &plistFormat) const
{
    Q_ASSERT(thisObject().engine() == engine());
    auto p = qscriptvalue_cast<PropertyList*>(thisObject());

    if (plistFormat == QLatin1String("binary1")) {
        p->context()->throwError(QStringLiteral("Property list object cannot be converted to a "
                                               "string in the binary1 format; this format can only "
                                               "be written directly to a file"));
        return {};
    }

    if (!isEmpty())
        return QString::fromUtf8(p->d->writeToData(p->context(), plistFormat));

    return {};
}

QString PropertyList::toXMLString() const
{
    return toString(QStringLiteral("xml1"));
}

QString PropertyList::toJSON(const QString &style) const
{
    QString format = QLatin1String("json");
    if (!style.isEmpty())
        format += QLatin1String("-") + style;

    return toString(format);
}

} // namespace Internal
} // namespace qbs

#include "propertylistutils.h"

namespace qbs {
namespace Internal {

void PropertyListPrivate::readFromData(QScriptContext *context, const QByteArray &data)
{
    @autoreleasepool {
        NSPropertyListFormat format;
        int internalFormat = 0;
        NSString *errorString = nil;
        id plist = [NSPropertyListSerialization propertyListWithData:data.toNSData()
                                                             options:0
                                                              format:&format error:nil];
        if (plist) {
            internalFormat = format;
        } else {
            NSError *error = nil;
            plist = [NSJSONSerialization JSONObjectWithData:data.toNSData()
                                                    options:0
                                                      error:&error];
            if (Q_UNLIKELY(!plist)) {
                errorString = [error localizedDescription];
            } else {
                internalFormat = QPropertyListJSONFormat;
            }
        }

        if (Q_UNLIKELY(!plist)) {
            context->throwError(QString::fromNSString(errorString));
        } else {
            QVariant obj = QPropertyListUtils::fromPropertyList(plist);
            if (!obj.isNull()) {
                propertyListObject = obj;
                propertyListFormat = internalFormat;
            } else {
                context->throwError(QStringLiteral("error converting property list"));
            }
        }
    }
}

QByteArray PropertyListPrivate::writeToData(QScriptContext *context, const QString &format)
{
    @autoreleasepool {
        NSError *error = nil;
        NSString *errorString = nil;
        NSData *data = nil;

        id obj = QPropertyListUtils::toPropertyList(propertyListObject);
        if (!obj) {
            context->throwError(QStringLiteral("error converting property list"));
            return QByteArray();
        }

        if (format == QLatin1String("json") || format == QLatin1String("json-pretty") ||
            format == QLatin1String("json-compact")) {
            if ([NSJSONSerialization isValidJSONObject:obj]) {
                error = nil;
                errorString = nil;
                const NSJSONWritingOptions options = format == QLatin1String("json-pretty")
                        ? NSJSONWritingPrettyPrinted : 0;
                data = [NSJSONSerialization dataWithJSONObject:obj
                                                       options:options
                                                         error:&error];
                if (Q_UNLIKELY(!data)) {
                    errorString = [error localizedDescription];
                }
            } else {
                errorString = @"Property list object cannot be converted to JSON data";
            }
        } else if (format == QLatin1String("xml1") || format == QLatin1String("binary1")) {
            const NSPropertyListFormat plistFormat = format == QLatin1String("xml1")
                                                          ? NSPropertyListXMLFormat_v1_0
                                                          : NSPropertyListBinaryFormat_v1_0;

            error = nil;
            errorString = nil;
            data = [NSPropertyListSerialization dataWithPropertyList:obj
                                                              format:plistFormat
                                                             options:0
                                                               error:&error];
            if (Q_UNLIKELY(!data)) {
                errorString = [error localizedDescription];
            }
        } else {
            errorString = [NSString stringWithFormat:@"Property lists cannot be written in the '%s' "
                                                     @"format", format.toUtf8().constData()];
        }

        if (Q_UNLIKELY(!data)) {
            context->throwError(QString::fromNSString(errorString));
        }

        return QByteArray::fromNSData(data);
    }
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionPropertyList(QScriptValue extensionObject)
{
    using namespace qbs::Internal;
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue obj = engine->newQMetaObject(&PropertyList::staticMetaObject,
                                              engine->newFunction(&PropertyList::ctor));
    extensionObject.setProperty(QStringLiteral("PropertyList"), obj);
}

Q_DECLARE_METATYPE(qbs::Internal::PropertyList *)

#include "propertylist.moc"
