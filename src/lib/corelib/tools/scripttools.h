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

#ifndef QBS_SCRIPTTOOLS_H
#define QBS_SCRIPTTOOLS_H

#include "codelocation.h"
#include "porting.h"
#include "qbs_export.h"

#include <quickjs.h>

#include <QtCore/qhash.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <string_view>
#include <utility>
#include <vector>

namespace qbs {
class ErrorInfo;
namespace Internal {
class ScriptEngine;

using JSValueList = std::vector<JSValue>;

void defineJsProperty(JSContext *ctx, JSValueConst obj, const QString &prop, JSValue val);
JSValue getJsProperty(JSContext *ctx, JSValueConst obj, const QString &prop);
void setJsProperty(JSContext *ctx, JSValueConst obj, std::string_view prop, JSValue val);
void setJsProperty(JSContext *ctx, JSValueConst obj, const QString &prop, JSValue val);
void setJsProperty(JSContext *ctx, JSValueConst obj, const QString &prop, const QString &val);
QString getJsStringProperty(JSContext *ctx, JSValueConst obj, const QString &prop);
QStringList getJsStringListProperty(JSContext *ctx, JSValueConst obj, const QString &prop);
int getJsIntProperty(JSContext *ctx, JSValueConst obj, const QString &prop);
bool getJsBoolProperty(JSContext *ctx, JSValueConst obj, const QString &prop);
QVariant getJsVariantProperty(JSContext *ctx, JSValueConst obj, const QString &prop);
QString getJsString(JSContext *ctx, JSValueConst val);
QString getJsString(JSContext *ctx, JSAtom atom);
QBS_AUTOTEST_EXPORT QVariant getJsVariant(JSContext *ctx, JSValueConst val);
JSValue makeJsArrayBuffer(JSContext *ctx, const QByteArray &s);
JSValue makeJsString(JSContext *ctx, const QString &s);
JSValue makeJsStringList(JSContext *ctx, const QStringList &l);
JSValue makeJsVariant(JSContext *ctx, const QVariant &v, quintptr id = 0);
JSValue makeJsVariantList(JSContext *ctx, const QVariantList &l, quintptr id = 0);
JSValue makeJsVariantMap(JSContext *ctx, const QVariantMap &m, quintptr id = 0);
QStringList getJsStringList(JSContext *ctx, JSValueConst val);
JSValue throwError(JSContext *ctx, const QString &message);
using PropertyHandler = std::function<void(const JSAtom &, const JSPropertyDescriptor &)>;
void handleJsProperties(JSContext *ctx, JSValueConst obj, const  PropertyHandler &handler);
inline quintptr jsObjectId(const JSValue &val) { return quintptr(JS_VALUE_GET_OBJ(val)); }

template <class T>
void attachPointerTo(JSValue &scriptValue, T *ptr)
{
    JS_SetOpaque(scriptValue, const_cast<std::remove_const_t<T> *>(ptr));
}

template <class T>
T *attachedPointer(const JSValue &scriptValue, JSClassID id)
{
    return reinterpret_cast<T *>(JS_GetOpaque(scriptValue, id));
}

class TemporaryGlobalObjectSetter
{
public:
    TemporaryGlobalObjectSetter(ScriptEngine *engine, const JSValue &object);
    ~TemporaryGlobalObjectSetter();

private:
    ScriptEngine * const m_engine;
    const JSValue m_oldGlobalObject;
};

class ScopedJsValue
{
public:
    ScopedJsValue(JSContext *ctx, JSValue v) : m_context(ctx), m_value(v) {}
    void setValue(JSValue v) { JS_FreeValue(m_context, m_value); m_value = v; }
    ~ScopedJsValue() { JS_FreeValue(m_context, m_value); }
    operator JSValue() const { return m_value; }
    JSValue release() { const JSValue v = m_value; m_value = JS_UNDEFINED; return v; }
    void reset(JSValue v) { JS_FreeValue(m_context, m_value); m_value = v; }

    ScopedJsValue(const ScopedJsValue &) = delete;
    ScopedJsValue &operator=(const ScopedJsValue &) = delete;

    ScopedJsValue(ScopedJsValue && other) : m_context(other.m_context), m_value(other.m_value)
    {
        other.m_value = JS_UNDEFINED;
    }

private:
    JSContext * const m_context;
    JSValue m_value;
};

class ScopedJsValueList
{
public:
    ScopedJsValueList(JSContext *ctx, const JSValueList &l) : m_context(ctx), m_list(l) {}
    ~ScopedJsValueList() { for (const JSValue v : m_list) JS_FreeValue(m_context, v); }
    operator JSValueList() const { return m_list; }

    ScopedJsValueList(const ScopedJsValueList &) = delete;
    ScopedJsValueList &operator=(const ScopedJsValueList &) = delete;

    ScopedJsValueList(ScopedJsValueList && other) noexcept
        : m_context(other.m_context), m_list(std::move(other.m_list))
    {
        other.m_list.clear();
    }

private:
    JSContext * const m_context;
    JSValueList m_list;
};

class ScopedJsAtom
{
public:
    ScopedJsAtom(JSContext *ctx, JSAtom a) : m_context(ctx), m_atom(a) {}
    ScopedJsAtom(JSContext *ctx, const QByteArray &s)
        : ScopedJsAtom(ctx, JS_NewAtom(ctx, s.constData())) {}
    ScopedJsAtom(JSContext *ctx, const QString &s) : ScopedJsAtom(ctx, s.toUtf8()) {}
    ~ScopedJsAtom() { JS_FreeAtom(m_context, m_atom); }
    operator JSAtom() const { return m_atom; }

    ScopedJsAtom(const ScopedJsAtom &) = delete;
    ScopedJsAtom&operator=(const ScopedJsAtom &) = delete;

    ScopedJsAtom(ScopedJsAtom &&other) : m_context(other.m_context), m_atom(other.m_atom)
    {
        other.m_atom = 0;
    }

private:
    JSContext * const m_context;
    JSAtom m_atom;
};

class QBS_AUTOTEST_EXPORT JsException
{
public:
    JsException(JSContext *ctx, JSValue ex, JSValue bt, const CodeLocation &fallbackLocation)
        : m_ctx(ctx)
        , m_exception(ex)
        , m_backtrace(bt)
        , m_fallbackLocation(fallbackLocation)
    {}
    JsException(JsException && other) noexcept;
    ~JsException();
    JsException(const JsException &) = delete;
    JsException &operator=(const JsException &) = delete;

    operator bool() const { return m_exception.tag != JS_TAG_UNINITIALIZED; }
    QString message() const;
    const QStringList stackTrace() const;
    ErrorInfo toErrorInfo() const;
private:
    JSContext *m_ctx;
    JSValue m_exception;
    JSValue m_backtrace;
    CodeLocation m_fallbackLocation;
};

void setConfigProperty(QVariantMap &cfg, const QStringList &name, const QVariant &value);
QVariant QBS_AUTOTEST_EXPORT getConfigProperty(const QVariantMap &cfg, const QStringList &name);

} // namespace Internal
} // namespace qbs

// Only to be used for objects!
#ifndef JS_NAN_BOXING
inline bool operator==(JSValue v1, JSValue v2) { return v1.u.ptr == v2.u.ptr; }
inline bool operator!=(JSValue v1, JSValue v2) { return !(v1 == v2); }
inline bool operator<(JSValue v1, JSValue v2) { return v1.u.ptr < v2.u.ptr; }
inline qbs::QHashValueType qHash(const JSValue &v) { return QT_PREPEND_NAMESPACE(qHash)(v.u.ptr); }
#endif

#endif // QBS_SCRIPTTOOLS_H
