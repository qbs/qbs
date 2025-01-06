/****************************************************************************
**
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

#include "scripttools.h"

#include <language/scriptengine.h>
#include <tools/error.h>

#include <QtCore/qdatastream.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

namespace qbs {
namespace Internal {

void setConfigProperty(QVariantMap &cfg, const QStringList &name, const QVariant &value)
{
    if (name.length() == 1) {
        cfg.insert(name.front(), value);
    } else {
        QVariant &subCfg = cfg[name.front()];
        QVariantMap subCfgMap = subCfg.toMap();
        setConfigProperty(subCfgMap, name.mid(1), value);
        subCfg = subCfgMap;
    }
}

QVariant getConfigProperty(const QVariantMap &cfg, const QStringList &name)
{
    if (name.length() == 1)
        return cfg.value(name.front());
    return getConfigProperty(cfg.value(name.front()).toMap(), name.mid(1));
}

TemporaryGlobalObjectSetter::TemporaryGlobalObjectSetter(
        ScriptEngine *engine, const JSValue &object)
    : m_engine(engine), m_oldGlobalObject(engine->globalObject())
{
    engine->setGlobalObject(object);
}

TemporaryGlobalObjectSetter::~TemporaryGlobalObjectSetter()
{
    m_engine->setGlobalObject(m_oldGlobalObject);
}

JsException::JsException(JsException &&other) noexcept
    : m_ctx(other.m_ctx)
    , m_exception(other.m_exception)
    , m_backtrace(other.m_exception)
    , m_fallbackLocation(std::move(other.m_fallbackLocation))
{
    other.m_exception = JS_NULL;
    other.m_backtrace = JS_NULL;
}

JsException::~JsException()
{
    JS_FreeValue(m_ctx, m_exception);
    JS_FreeValue(m_ctx, m_backtrace);
}

QString JsException::message() const
{
    if (JS_IsError(m_ctx, m_exception))
        return getJsStringProperty(m_ctx, m_exception, QStringLiteral("message"));
    const QVariant v = getJsVariant(m_ctx, m_exception);
    switch (static_cast<QMetaType::Type>(v.userType())) {
    case QMetaType::QVariantMap:
        return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(v.toMap()))
                                 .toJson(QJsonDocument::Indented));
    case QMetaType::QStringList:
        return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(v.toStringList()))
                                 .toJson(QJsonDocument::Indented));
    case QMetaType::QVariantList:
        return QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(v.toList()))
                                 .toJson(QJsonDocument::Indented));
    default:
        return v.toString();
    }
}

const QStringList JsException::stackTrace() const
{
    return getJsString(m_ctx, m_backtrace).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

ErrorInfo JsException::toErrorInfo() const
{
    const QString msg = message();
    ErrorInfo e(msg, stackTrace());
    if (e.hasLocation() || !m_fallbackLocation.isValid())
        return e;
    return {msg, m_fallbackLocation};
}

void defineJsProperty(JSContext *ctx, JSValueConst obj, const QString &prop, JSValue val)
{
    JS_DefinePropertyValueStr(ctx, obj, prop.toUtf8().constData(), val, 0);
}

JSValue getJsProperty(JSContext *ctx, JSValue obj, const QString &prop)
{
    return JS_GetPropertyStr(ctx, obj, prop.toUtf8().constData());
}

void setJsProperty(JSContext *ctx, JSValueConst obj, std::string_view prop, JSValue val)
{
    JS_SetPropertyStr(ctx, obj, prop.data(), val);
}

void setJsProperty(JSContext *ctx, JSValue obj, const QString &prop, JSValue val)
{
    const auto propUtf8 = prop.toUtf8();
    setJsProperty(ctx, obj, std::string_view{propUtf8.data(), size_t(propUtf8.size())}, val);
}

void setJsProperty(JSContext *ctx, JSValue obj, const QString &prop, const QString &val)
{
    setJsProperty(ctx, obj, prop, makeJsString(ctx, val));
}

void handleJsProperties(JSContext *ctx, JSValue obj,
                        const std::function<void (const JSAtom &,
                                                  const JSPropertyDescriptor &)> &handler)
{
    struct PropsHolder {
        PropsHolder(JSContext *ctx) : ctx(ctx) {}
        ~PropsHolder() {
            for (uint32_t i = 0; i < count; ++i)
                JS_FreeAtom(ctx, props[i].atom);
            js_free(ctx, props);
        }
        JSContext * const ctx;
        JSPropertyEnum *props = nullptr;
        uint32_t count = 0;
    } propsHolder(ctx);
    JS_GetOwnPropertyNames(ctx, &propsHolder.props, &propsHolder.count, obj, JS_GPN_STRING_MASK);
    for (uint32_t i = 0; i < propsHolder.count; ++i) {
        JSPropertyDescriptor desc{0, JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED};
        JS_GetOwnProperty(ctx, &desc, obj, propsHolder.props[i].atom);
        const ScopedJsValueList valueHolder(ctx, {desc.value, desc.getter, desc.setter});
        handler(propsHolder.props[i].atom, desc);
    }
}

QString getJsString(JSContext *ctx, JSValueConst val)
{
    size_t strLen;
    const char * const str = JS_ToCStringLen(ctx, &strLen, val);
    QString s = QString::fromUtf8(str, strLen);
    JS_FreeCString(ctx, str);
    return s;
}

QString getJsStringProperty(JSContext *ctx, JSValue obj, const QString &prop)
{
    const ScopedJsValue sv(ctx, getJsProperty(ctx, obj, prop));
    return getJsString(ctx, sv);
}

int getJsIntProperty(JSContext *ctx, JSValue obj, const QString &prop)
{
    return JS_VALUE_GET_INT(getJsProperty(ctx, obj, prop));
}

bool getJsBoolProperty(JSContext *ctx, JSValue obj, const QString &prop)
{
    return JS_VALUE_GET_BOOL(getJsProperty(ctx, obj, prop));
}

JSValue makeJsArrayBuffer(JSContext *ctx, const QByteArray &s)
{
    return ScriptEngine::engineForContext(ctx)->asJsValue(s);
}

JSValue makeJsString(JSContext *ctx, const QString &s)
{
    return ScriptEngine::engineForContext(ctx)->asJsValue(s);
}

JSValue makeJsStringList(JSContext *ctx, const QStringList &l)
{
    return ScriptEngine::engineForContext(ctx)->asJsValue(l);
}

JSValue throwError(JSContext *ctx, const QString &message)
{
    return JS_Throw(ctx, makeJsString(ctx, message));
}

QStringList getJsStringList(JSContext *ctx, JSValue val)
{
    if (JS_IsString(val))
        return {getJsString(ctx, val)};
    if (!JS_IsArray(ctx, val))
        return {};
    const int size = getJsIntProperty(ctx, val, QLatin1String("length"));
    QStringList l;
    for (int i = 0; i < size; ++i) {
        const ScopedJsValue elem(ctx, JS_GetPropertyUint32(ctx, val, i));
        l << getJsString(ctx, elem);
    }
    return l;
}

JSValue makeJsVariant(JSContext *ctx, const QVariant &v, quintptr id)
{
    return ScriptEngine::engineForContext(ctx)->asJsValue(v, id);
}

JSValue makeJsVariantList(JSContext *ctx, const QVariantList &l, quintptr id)
{
    return ScriptEngine::engineForContext(ctx)->asJsValue(l, id);
}

JSValue makeJsVariantMap(JSContext *ctx, const QVariantMap &m, quintptr id)
{
    return ScriptEngine::engineForContext(ctx)->asJsValue(m, id);
}

static QVariant getJsVariantImpl(JSContext *ctx, JSValue val, QList<JSValue> path)
{
    if (JS_IsString(val))
        return getJsString(ctx, val);
    if (JS_IsBool(val))
        return bool(JS_VALUE_GET_BOOL(val));
    if (JS_IsArrayBuffer(val)) {
        size_t size = 0;
        const auto data = JS_GetArrayBuffer(ctx, &size, val);
        if (!data || !size)
            return QByteArray();
        return QByteArray(reinterpret_cast<const char *>(data), size);
    }
    if (JS_IsArray(ctx, val)) {
        if (path.contains(val))
            return {};
        path << val;
        QVariantList l;
        const int size = getJsIntProperty(ctx, val, QLatin1String("length"));
        for (int i = 0; i < size; ++i) {
            const ScopedJsValue sv(ctx, JS_GetPropertyUint32(ctx, val, i));
            l << getJsVariantImpl(ctx, sv, path);
        }
        return l;
    }
    if (JS_IsDate(val)) {
        ScopedJsValue toString(ctx, getJsProperty(ctx, val, QLatin1String("toISOString")));
        if (!JS_IsFunction(ctx, toString))
            return {};
        ScopedJsValue dateString(ctx, JS_Call(ctx, toString, val, 0, nullptr));
        if (!JS_IsString(dateString))
            return {};
        return QDateTime::fromString(getJsString(ctx, dateString), Qt::ISODateWithMs);
    }
    if (JS_IsObject(val)) {
        if (path.contains(val))
            return {};
        path << val;
        QVariantMap map;
        handleJsProperties(ctx, val, [ctx, &map, path](const JSAtom &prop, const JSPropertyDescriptor &desc) {
            map.insert(getJsString(ctx, prop), getJsVariantImpl(ctx, desc.value, path));
        });
        return map;
    }
    const auto tag = JS_VALUE_GET_TAG(val);
    if (tag == JS_TAG_INT)
        return JS_VALUE_GET_INT(val);
    else if (JS_TAG_IS_FLOAT64(tag))
        return JS_VALUE_GET_FLOAT64(val);
    return {};
}

QVariant getJsVariant(JSContext *ctx, JSValue val)
{
    return getJsVariantImpl(ctx, val, {});
}

QStringList getJsStringListProperty(JSContext *ctx, JSValue obj, const QString &prop)
{
    JSValue array = getJsProperty(ctx, obj, prop);
    QStringList list = getJsStringList(ctx, array);
    JS_FreeValue(ctx, array);
    return list;
}

QVariant getJsVariantProperty(JSContext *ctx, JSValue obj, const QString &prop)
{
    const JSValue vv = getJsProperty(ctx, obj, prop);
    QVariant v = getJsVariant(ctx, vv);
    JS_FreeValue(ctx, vv);
    return v;
}

QString getJsString(JSContext *ctx, JSAtom atom)
{
    const ScopedJsValue atomString(ctx, JS_AtomToString(ctx, atom));
    return getJsString(ctx, atomString);
}

} // namespace Internal
} // namespace qbs
