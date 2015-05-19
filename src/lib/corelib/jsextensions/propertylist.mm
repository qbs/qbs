/****************************************************************************
**
** Copyright (C) 2015 Petroules Corporation.
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

#include "propertylist.h"

#include <tools/hostosinfo.h>

#include <QFile>
#include <QScriptEngine>
#include <QScriptValue>
#include <QTextStream>

// Same values as CoreFoundation and Foundation APIs
enum {
    QPropertyListOpenStepFormat = 1,
    QPropertyListXMLFormat_v1_0 = 100,
    QPropertyListBinaryFormat_v1_0 = 200,
    QPropertyListJSONFormat = 1000 // If this conflicts someday, just change it :)
};

namespace qbs {
namespace Internal {

class PropertyListPrivate
{
public:
    PropertyListPrivate();

    QVariant propertyListObject;
    int propertyListFormat;

    void readFromData(QScriptContext *context, QByteArray data);
    QByteArray writeToData(QScriptContext *context, const QString &format);
};

void initializeJsExtensionPropertyList(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue obj = engine->newQMetaObject(&PropertyList::staticMetaObject,
                                              engine->newFunction(&PropertyList::ctor));
    extensionObject.setProperty(QLatin1String("PropertyList"), obj);
}

QScriptValue PropertyList::ctor(QScriptContext *context, QScriptEngine *engine)
{
    PropertyList *p = new PropertyList(context);
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
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());
    return p->d->propertyListObject.isNull();
}

void PropertyList::clear()
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());
    p->d->propertyListObject = QVariant();
    p->d->propertyListFormat = 0;
}

void PropertyList::readFromObject(const QScriptValue &value)
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());
    p->d->propertyListObject = value.toVariant();
    p->d->propertyListFormat = 0; // wasn't deserialized from any external format
}

void PropertyList::readFromString(const QString &input)
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());
    p->d->readFromData(p->context(), input.toUtf8());
}

void PropertyList::readFromFile(const QString &filePath)
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        if (!data.isEmpty()) {
            p->d->readFromData(p->context(), data);
        } else {
            p->context()->throwError(file.errorString());
        }
    } else {
        p->context()->throwError(file.errorString());
    }
}

void PropertyList::writeToFile(const QString &filePath, const QString &plistFormat)
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());

    QByteArray data = p->d->writeToData(p->context(), plistFormat);
    if (Q_LIKELY(!data.isEmpty())) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            if (Q_UNLIKELY(file.write(data) != data.size())) {
                p->context()->throwError(file.errorString());
            }
        } else {
            p->context()->throwError(file.errorString());
        }
    }
}

QScriptValue PropertyList::format() const
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());
    switch (p->d->propertyListFormat)
    {
    case QPropertyListOpenStepFormat:
        return QLatin1String("openstep");
    case QPropertyListXMLFormat_v1_0:
        return QLatin1String("xml1");
    case QPropertyListBinaryFormat_v1_0:
        return QLatin1String("binary1");
    case QPropertyListJSONFormat:
        return QLatin1String("json");
    default:
        return p->engine()->undefinedValue();
    }
}

QScriptValue PropertyList::toObject() const
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());
    return p->engine()->toScriptValue(p->d->propertyListObject);
}

QString PropertyList::toString(const QString &plistFormat) const
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());

    if (plistFormat == QLatin1String("binary1")) {
        p->context()->throwError(QLatin1String("Property list object cannot be converted to a "
                                               "string in the binary1 format; this format can only "
                                               "be written directly to a file"));
        return QString();
    }

    if (!isEmpty())
        return QString::fromUtf8(p->d->writeToData(p->context(), plistFormat));

    return QString();
}

QString PropertyList::toXMLString() const
{
    return toString(QLatin1String("xml1"));
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

void PropertyListPrivate::readFromData(QScriptContext *context, QByteArray data)
{
    @autoreleasepool {
        NSPropertyListFormat format;
        int internalFormat = 0;
        NSError *error = nil;
        NSString *errorString = nil;
        id plist = nil;

        if ([NSPropertyListSerialization
                respondsToSelector:@selector(propertyListWithData:options:format:error:)]) {
            error = nil;
            errorString = nil;
            plist = [NSPropertyListSerialization propertyListWithData:QByteArray_toNSData(data)
                                                              options:0
                                                               format:&format error:&error];
            if (Q_UNLIKELY(!plist)) {
                errorString = [error localizedDescription];
            }
        }
        else
        {
            error = nil;
            errorString = nil;
            plist = [NSPropertyListSerialization propertyListFromData:QByteArray_toNSData(data)
                                                     mutabilityOption:NSPropertyListImmutable
                                                               format:&format
                                                     errorDescription:&errorString];
        }
        if (plist)
            internalFormat = format;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_10_7
        if (!plist && NSClassFromString(@"NSJSONSerialization")) {
            error = nil;
            errorString = nil;
            plist = [NSJSONSerialization JSONObjectWithData:QByteArray_toNSData(data)
                                                    options:0
                                                      error:&error];
            if (Q_UNLIKELY(!plist)) {
                errorString = [error localizedDescription];
            } else {
                internalFormat = QPropertyListJSONFormat;
            }
        }
#endif

        if (Q_UNLIKELY(!plist)) {
            context->throwError(QString_fromNSString(errorString));
        } else {
            QVariant obj = QPropertyListUtils::fromPropertyList(plist);
            if (!obj.isNull()) {
                propertyListObject = obj;
                propertyListFormat = internalFormat;
            } else {
                context->throwError(QLatin1String("error converting property list"));
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
            context->throwError(QLatin1String("error converting property list"));
            return 0;
        }

        if (format == QLatin1String("json") || format == QLatin1String("json-pretty") ||
            format == QLatin1String("json-compact")) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_10_7
            if (NSClassFromString(@"NSJSONSerialization")) {
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
            }
#endif
            else {
                errorString = @"JSON serialization of property lists is not "
                              @"supported on this version of OS X";
            }
        } else if (format == QLatin1String("xml1") || format == QLatin1String("binary1")) {
            const NSPropertyListFormat plistFormat = format == QLatin1String("xml1")
                                                          ? NSPropertyListXMLFormat_v1_0
                                                          : NSPropertyListBinaryFormat_v1_0;

            error = nil;
            errorString = nil;
            if ([NSPropertyListSerialization
                    respondsToSelector:@selector(dataWithPropertyList:format:options:error:)]) {
                data = [NSPropertyListSerialization dataWithPropertyList:obj
                                                                  format:plistFormat
                                                                 options:0
                                                                   error:&error];
                if (Q_UNLIKELY(!data)) {
                    errorString = [error localizedDescription];
                }
            } else {
                data = [NSPropertyListSerialization dataFromPropertyList:obj
                                                                  format:plistFormat
                                                        errorDescription:&errorString];
            }
        } else {
            errorString = [NSString stringWithFormat:@"Property lists cannot be written in the '%s' "
                                                     @"format", format.toUtf8().constData()];
        }

        if (Q_UNLIKELY(!data)) {
            context->throwError(QString_fromNSString(errorString));
        }

        return QByteArray_fromNSData(data);
    }
}

} // namespace Internal
} // namespace qbs
