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

#include "jsextension.h"
#include "propertylistutils.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>

#include <QtCore/qfile.h>
#include <QtCore/qstring.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qvariant.h>

namespace qbs {
namespace Internal {

class PropertyList : public JsExtension<PropertyList>
{
public:
    static const char *name() { return "PropertyList"; }
    static JSValue ctor(JSContext *ctx, JSValueConst, JSValueConst, int, JSValueConst *, int);
    PropertyList(JSContext *) {}
    static void setupMethods(JSContext *ctx, JSValue obj);

    DEFINE_JS_FORWARDER(jsIsEmpty, &PropertyList::isEmpty, "PropertyList.isEmpty");
    DEFINE_JS_FORWARDER(jsClear, &PropertyList::clear, "PropertyList.clear");
    DEFINE_JS_FORWARDER(jsReadFromObject, &PropertyList::readFromObject,
                        "PropertyList.readFromObject");
    DEFINE_JS_FORWARDER(jsReadFromString, &PropertyList::readFromString,
                        "PropertyList.readFromString");
    DEFINE_JS_FORWARDER(jsReadFromFile, &PropertyList::readFromFile, "PropertyList.readFromFile");
    DEFINE_JS_FORWARDER(jsReadFromData, &PropertyList::readFromData, "PropertyList.readFromData");
    DEFINE_JS_FORWARDER(jsWriteToFile, &PropertyList::writeToFile, "PropertyList.writeToFile");
    DEFINE_JS_FORWARDER(jsFormat, &PropertyList::format, "PropertyList.format");
    DEFINE_JS_FORWARDER(jsToObject, &PropertyList::toObject, "PropertyList.toObject");
    DEFINE_JS_FORWARDER(jsToString, &PropertyList::toString, "PropertyList.toString");
    DEFINE_JS_FORWARDER(jsToXmlString, &PropertyList::toXMLString, "PropertyList.toXMLString");

    static JSValue jsToJson(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
    {
        try {
            QString style;
            if (argc > 0)
                style = fromArg<QString>(ctx, "Process.exec", 1, argv[0]);
            return toJsValue(ctx, fromJsObject(ctx, this_val)->toJSON(style));
        } catch (const QString &error) { return throwError(ctx, error); }
    }

private:
    bool isEmpty() const { return m_propertyListObject.isNull(); }
    void clear();
    void readFromObject(const QVariant &value);
    void readFromString(const QString &input);
    void readFromFile(const QString &filePath);
    void readFromData(const QByteArray &data);
    void writeToFile(const QString &filePath, const QString &plistFormat);
    std::optional<QString> format() const;
    QVariant toObject() const {
        return m_propertyListObject.isNull() ? QVariantMap() : m_propertyListObject;
    }
    QString toString(const QString &plistFormat) const;
    QString toXMLString() const;
    QString toJSON(const QString &style = QString()) const;

    QByteArray writeToData(const QString &format) const;

    QVariant m_propertyListObject;
    int m_propertyListFormat = 0;
};

// Same values as CoreFoundation and Foundation APIs
enum {
    QPropertyListOpenStepFormat = 1,
    QPropertyListXMLFormat_v1_0 = 100,
    QPropertyListBinaryFormat_v1_0 = 200,
    QPropertyListJSONFormat = 1000 // If this conflicts someday, just change it :)
};

JSValue PropertyList::ctor(JSContext *ctx, JSValueConst, JSValueConst, int, JSValueConst *, int)
{
    try {
        JSValue obj = createObject(ctx);
        const auto se = ScriptEngine::engineForContext(ctx);
        const DubiousContextList dubiousContexts{
            DubiousContext(EvalContext::PropertyEvaluation, DubiousContext::SuggestMoving)
        };
        se->checkContext(QStringLiteral("qbs.PropertyList"), dubiousContexts);
        return obj;
    } catch (const QString &error) { return throwError(ctx, error); }
}

void PropertyList::setupMethods(JSContext *ctx, JSValue obj)
{
    setupMethod(ctx, obj, "isEmpty", &jsIsEmpty, 0);
    setupMethod(ctx, obj, "clear", &jsClear, 0);
    setupMethod(ctx, obj, "readFromObject", &jsReadFromObject, 1);
    setupMethod(ctx, obj, "readFromString", &jsReadFromString, 1);
    setupMethod(ctx, obj, "readFromFile", &jsReadFromFile, 1);
    setupMethod(ctx, obj, "readFromData", &jsReadFromData, 1);
    setupMethod(ctx, obj, "writeToFile", &jsWriteToFile, 1);
    setupMethod(ctx, obj, "format", &jsFormat, 0);
    setupMethod(ctx, obj, "toObject", &jsToObject, 0);
    setupMethod(ctx, obj, "toString", &jsToString, 1);
    setupMethod(ctx, obj, "toXMLString", &jsToXmlString, 1);
    setupMethod(ctx, obj, "toJSON", &jsToJson, 1);
}

void PropertyList::clear()
{
    m_propertyListObject = QVariant();
    m_propertyListFormat = 0;
}

void PropertyList::readFromObject(const QVariant &value)
{
    m_propertyListObject = value;
    m_propertyListFormat = 0; // wasn't deserialized from any external format
}

void PropertyList::readFromString(const QString &input)
{
    readFromData(input.toUtf8());
}

void PropertyList::readFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray data = file.readAll();
        if (file.error() == QFile::NoError) {
            readFromData(data);
            return;
        }
    }
    throw QStringLiteral("%1: %2").arg(filePath).arg(file.errorString());
}

void PropertyList::readFromData(const QByteArray &data)
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

        if (Q_UNLIKELY(!plist))
            throw QString::fromNSString(errorString);
        QVariant obj = QPropertyListUtils::fromPropertyList(plist);
        if (!obj.isNull()) {
            m_propertyListObject = obj;
            m_propertyListFormat = internalFormat;
        } else {
            throw Tr::tr("error converting property list");
        }
    }
}

void PropertyList::writeToFile(const QString &filePath, const QString &plistFormat)
{
    QFile file(filePath);
    QByteArray data = writeToData(plistFormat);
    if (Q_LIKELY(!data.isEmpty())) {
        if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size()) {
            return;
        }
    }

    throw QStringLiteral("%1: %2").arg(filePath).arg(file.errorString());
}

std::optional<QString> PropertyList::format() const
{
    switch (m_propertyListFormat)
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
        return {};
    }
}

QString PropertyList::toString(const QString &plistFormat) const
{
    if (plistFormat == QLatin1String("binary1")) {
        throw Tr::tr("Property list object cannot be converted to a "
                     "string in the binary1 format; this format can only "
                     "be written directly to a file");
    }

    if (!isEmpty())
        return QString::fromUtf8(writeToData(plistFormat));
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

QByteArray PropertyList::writeToData(const QString &format) const
{
    @autoreleasepool {
        NSError *error = nil;
        NSString *errorString = nil;
        NSData *data = nil;

        id obj = QPropertyListUtils::toPropertyList(m_propertyListObject);
        if (!obj)
            throw Tr::tr("error converting property list");

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

        if (Q_UNLIKELY(!data))
            throw QString::fromNSString(errorString);

        return QByteArray::fromNSData(data);
    }
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionPropertyList(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::PropertyList::registerClass(engine, extensionObject);
}
