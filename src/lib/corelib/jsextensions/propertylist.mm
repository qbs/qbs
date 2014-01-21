/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2014 Petroules Corporation.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "propertylist.h"

#include <tools/hostosinfo.h>

#include <QFile>
#include <QScriptEngine>
#include <QScriptValue>
#include <QTextStream>

#import <Foundation/Foundation.h>

namespace qbs {
namespace Internal {

QString toQString(NSString *str)
{
    QString qstring;
    if ([str length] > 0) {
        qstring.resize([str length]);
        [str getCharacters:(unichar *)qstring.data() range:NSMakeRange(0, qstring.size())];
    }
    return qstring;
}

NSString *fromQString(const QString &str)
{
    return [NSString stringWithCharacters:(const unichar *)str.unicode() length:str.size()];
}

class PropertyListPrivate
{
public:
    PropertyListPrivate();

    id propertyListObject;

    void read(QScriptContext *context, NSData *data);
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
    : propertyListObject()
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

void PropertyList::read(const QString &input)
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());

    NSString *inputString = fromQString(input);
    NSData *data = [NSData dataWithBytes:[inputString UTF8String]
                            length:[inputString lengthOfBytesUsingEncoding:NSUTF8StringEncoding]];
    p->d->read(p->context(), data);
}

void PropertyList::readFile(const QString &filePath)
{
    Q_ASSERT(thisObject().engine() == engine());
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());

    NSError *error;
    NSData *data = [NSData dataWithContentsOfFile:fromQString(filePath) options:0 error:&error];
    if (!data) {
        p->context()->throwError(toQString([error description]));
    }

    p->d->read(p->context(), data);
}

void PropertyListPrivate::read(QScriptContext *context, NSData *data)
{
    NSError *error = nil;
    id plist = nil;
    if ([NSPropertyListSerialization
            respondsToSelector:@selector(propertyListWithData:options:format:error:)]) {
        error = nil;
        plist = [NSPropertyListSerialization propertyListWithData:data
                                                          options:0
                                                           format:NULL error:&error];
        if (Q_UNLIKELY(!plist)) {
            context->throwError(toQString([error description]));
        }
    }
    else
    {
        NSString *errorString = nil;
        plist = [NSPropertyListSerialization propertyListFromData:data
                                                 mutabilityOption:NSPropertyListImmutable
                                                           format:NULL
                                                 errorDescription:&errorString];
        if (Q_UNLIKELY(!plist)) {
            context->throwError(toQString(errorString));
        }
    }

    propertyListObject = plist;
}

QString PropertyList::toXML() const
{
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());
    if (!p->d->propertyListObject)
        return QString();

    NSData *data = nil;
    if ([NSPropertyListSerialization
            respondsToSelector:@selector(dataWithPropertyList:format:options:error:)]) {
        NSError *error = nil;
        data = [NSPropertyListSerialization dataWithPropertyList:p->d->propertyListObject
                                                          format:NSPropertyListXMLFormat_v1_0
                                                         options:0 error:&error];
        if (!data) {
            p->context()->throwError(toQString([error description]));
        }
    } else {
        NSString *errorString = nil;
        data = [NSPropertyListSerialization dataFromPropertyList:p->d->propertyListObject
                                                          format:NSPropertyListXMLFormat_v1_0
                                                errorDescription:&errorString];
        if (!data) {
            p->context()->throwError(toQString(errorString));
        }
    }

    return toQString([[[NSString alloc] initWithData:data
                                            encoding:NSUTF8StringEncoding] autorelease]);
}

QString PropertyList::toJSON() const
{
    PropertyList *p = qscriptvalue_cast<PropertyList*>(thisObject());
    if (!p->d->propertyListObject)
        return QString();

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_10_7
    if (NSClassFromString(@"NSJSONSerialization")) {
        NSError *error = nil;
        NSData *data = [NSJSONSerialization dataWithJSONObject:p->d->propertyListObject
                                                       options:0
                                                         error:&error];
        if (!data) {
            p->context()->throwError(toQString([error description]));
        }

        return toQString([[[NSString alloc] initWithData:data
                                                encoding:NSUTF8StringEncoding] autorelease]);
    }
    else
#endif
    {
        p->context()->throwError(QLatin1String("JSON serialization of property lists is not "
                                               "supported on this version of OS X"));
        return QString();
    }
}

} // namespace Internal
} // namespace qbs
