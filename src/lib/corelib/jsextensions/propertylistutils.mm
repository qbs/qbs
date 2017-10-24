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

#import <Foundation/Foundation.h>
#include "propertylistutils.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

static QVariant fromObject(id obj);
static QVariantMap fromDictionary(NSDictionary *dict);
static QVariantList fromArray(NSArray *array);

static QVariant fromObject(id obj)
{
    QVariant value;
    if (!obj) {
        return value;
    } else if ([obj isKindOfClass:[NSDictionary class]]) {
        value = fromDictionary(obj);
    } else if ([obj isKindOfClass:[NSArray class]]) {
        value = fromArray(obj);
    } else if ([obj isKindOfClass:[NSString class]]) {
        value = QString::fromNSString(obj);
    } else if ([obj isKindOfClass:[NSData class]]) {
        value = QByteArray::fromNSData(obj);
    } else if ([obj isKindOfClass:[NSDate class]]) {
        value = QDateTime::fromNSDate(obj);
    } else if ([obj isKindOfClass:[NSNumber class]]) {
        if (strcmp([(NSNumber *)obj objCType], @encode(BOOL)) == 0) {
            value = static_cast<bool>([obj boolValue]);
        } else if (strcmp([(NSNumber *)obj objCType], @encode(signed char)) == 0) {
            value = [obj charValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(unsigned char)) == 0) {
            value = [obj unsignedCharValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(signed short)) == 0) {
            value = [obj shortValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(unsigned short)) == 0) {
            value = [obj unsignedShortValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(signed int)) == 0) {
            value = [obj intValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(unsigned int)) == 0) {
            value = [obj unsignedIntValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(signed long long)) == 0) {
            value = [obj longLongValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(unsigned long long)) == 0) {
            value = [obj unsignedLongLongValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(float)) == 0) {
            value = [obj floatValue];
        } else if (strcmp([(NSNumber *)obj objCType], @encode(double)) == 0) {
            value = [obj doubleValue];
        } else {
            // NSDecimal or unknown
            value = [obj doubleValue];
        }
    } else if ([obj isKindOfClass:[NSNull class]]) {
        // A null variant, close enough...
    } else {
        // unknown
    }

    return value;
}

static QVariantMap fromDictionary(NSDictionary *dict)
{
    QVariantMap map;
    for (NSString *key in dict)
        map[QString::fromNSString(key)] = fromObject([dict objectForKey:key]);
    return map;
}

static QVariantList fromArray(NSArray *array)
{
    QVariantList list;
    for (id obj in array)
        list.push_back(fromObject(obj));
    return list;
}

QVariant QPropertyListUtils::fromPropertyList(id plist)
{
    return fromObject(plist);
}

static id toObject(const QVariant &variant);
static NSDictionary *toDictionary(const QVariantMap &map);
static NSArray *toArray(const QVariantList &list);

static id toObject(const QVariant &variant)
{
    if (variant.type() == QVariant::Hash) {
        return toDictionary(qHashToMap(variant.toHash()));
    } else if (variant.type() == QVariant::Map) {
        return toDictionary(variant.toMap());
    } else if (variant.type() == QVariant::List) {
        return toArray(variant.toList());
    } else if (variant.type() == QVariant::String) {
        return variant.toString().toNSString();
    } else if (variant.type() == QVariant::ByteArray) {
        return variant.toByteArray().toNSData();
    } else if (variant.type() == QVariant::Date ||
               variant.type() == QVariant::DateTime) {
        return variant.toDateTime().toNSDate();
    } else if (variant.type() == QVariant::Bool) {
        return variant.toBool()
                ? [NSNumber numberWithBool:YES]
                : [NSNumber numberWithBool:NO];
    } else if (variant.type() == QVariant::Char ||
               variant.type() == QVariant::Int) {
        return [NSNumber numberWithInt:variant.toInt()];
    } else if (variant.type() == QVariant::UInt) {
        return [NSNumber numberWithUnsignedInt:variant.toUInt()];
    } else if (variant.type() == QVariant::LongLong) {
        return [NSNumber numberWithLongLong:variant.toLongLong()];
    } else if (variant.type() == QVariant::ULongLong) {
        return [NSNumber numberWithUnsignedLongLong:variant.toULongLong()];
    } else if (variant.type() == QVariant::Double) {
        return [NSNumber numberWithDouble:variant.toDouble()];
    } else {
        return [NSNull null];
    }
}

static NSDictionary *toDictionary(const QVariantMap &map)
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    QMapIterator<QString, QVariant> i(map);
    while (i.hasNext()) {
        i.next();
        [dict setObject:toObject(i.value()) forKey:i.key().toNSString()];
    }
    return [NSDictionary dictionaryWithDictionary:dict];
}

static NSArray *toArray(const QVariantList &list)
{
    NSMutableArray *array = [NSMutableArray array];
    for (const QVariant &variant : list)
        [array addObject:toObject(variant)];
    return [NSArray arrayWithArray:array];
}

id QPropertyListUtils::toPropertyList(const QVariant &variant)
{
    return toObject(variant);
}
