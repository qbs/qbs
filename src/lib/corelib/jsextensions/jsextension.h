/****************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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

#pragma once

#include <language/scriptengine.h>
#include <logging/translator.h>

#include <optional>
#include <tuple>
#include <utility>

namespace qbs::Internal {

template<typename ...T> struct PackHelper {};
template<typename T> struct FromArgHelper;
template <typename T> struct FunctionTrait;

#define DECLARE_ENUM(ctx, obj, enumVal) \
    JS_SetPropertyStr((ctx), (obj), #enumVal, JS_NewInt32((ctx), (enumVal)))

template<class Derived> class JsExtension
{
#define DEFINE_JS_FORWARDER(name, func, text)                                                \
    static JSValue name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) \
    {                                                                                        \
        return jsForward(func, text, ctx, this_val, argc, argv);                             \
    }

#define DEFINE_JS_FORWARDER_QUAL(klass, name, func, text)                                    \
    static JSValue name(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) \
    {                                                                                        \
        return klass::jsForward(func, text, ctx, this_val, argc, argv);                      \
    }

public:
    virtual ~JsExtension() = default;

    template<typename ...Args > static JSValue createObject(JSContext *ctx, Args... args)
    {
        ScopedJsValue obj(ctx, JS_NewObjectClass(ctx, classId(ctx)));
        JS_SetOpaque(obj, new Derived(ctx, args...));
        Derived::setupMethods(ctx, obj);
        return obj.release();
    }

    static JSValue createObjectDirect(JSContext *ctx, void *obj)
    {
        JSValue jsObj = JS_NewObjectClass(ctx, classId(ctx));
        attachPointerTo(jsObj, obj);
        Derived::setupMethods(ctx, jsObj);
        return jsObj;
    }

    static Derived *fromJsObject(JSContext *ctx, JSValue obj)
    {
        return attachedPointer<Derived>(obj, classId(ctx));
    }

    static void registerClass(ScriptEngine *engine, JSValue extensionObject)
    {
        if (const JSValue cachedValue = engine->getInternalExtension(Derived::name());
                !JS_IsUndefined(cachedValue)) {
            JS_SetPropertyStr(engine->context(), extensionObject, Derived::name(), cachedValue);
            return;
        }
        engine->registerClass(Derived::name(), &Derived::ctor, &finalizer, extensionObject);
        const ScopedJsValue classObj(
                    engine->context(),
                    JS_GetPropertyStr(engine->context(), extensionObject, Derived::name()));
        Derived::setupStaticMethods(engine->context(), classObj);
        Derived::declareEnums(engine->context(), classObj);
    }

    // obj is either class (for "static" methods) or class instance
    template<typename Func> static void setupMethod(JSContext *ctx, JSValue obj, const char *name,
                                                    Func func, int argc)
    {
        JS_DefinePropertyValueStr(ctx, obj, name, JS_NewCFunction(ctx, func, name, argc), 0);
    }

    template<typename Func> static void setupMethod(JSContext *ctx, JSValue obj,
                                                    const QString &name, Func func, int argc)
    {
        setupMethod(ctx, obj, name.toUtf8().constData(), func, argc);
    }

    template<typename T> static T fromArg(JSContext *ctx, const char *funcName, int pos, JSValue v)
    {
        return FromArgHelper<T>::fromArg(ctx, funcName, pos, v);
    }

    template <typename Tuple, std::size_t... I>
    static void assignToTuple(Tuple &tuple, std::index_sequence<I...>, JSContext *ctx, const char *funcName, JSValueConst *argv) {
        Q_UNUSED(ctx)
        Q_UNUSED(funcName)
        Q_UNUSED(argv)
        ((std::get<I>(tuple) = fromArg<std::tuple_element_t<I, Tuple>>(ctx, funcName, I + 1, argv[I])), ...);
    }

    template<typename ...Args> static std::tuple<Args...> getArguments(JSContext *ctx,
            const char *funcName, int argc, JSValueConst *argv)
    {
        constexpr int expectedArgc = sizeof ...(Args);
        if (argc < expectedArgc) {
            throw Tr::tr("%1 requires %d arguments.")
                    .arg(QLatin1String(funcName)).arg(expectedArgc);
        }
        std::tuple<std::remove_const_t<std::remove_reference_t<Args>>...> values;
        assignToTuple(values, std::make_index_sequence<std::tuple_size<decltype(values)>::value>(),
                      ctx, funcName, argv);
        return values;
    }
    template<typename ...Args> static std::tuple<Args...> getArgumentsHelper(PackHelper<Args...>, JSContext *ctx,
            const char *funcName, int argc, JSValueConst *argv)
    {
        return getArguments<Args...>(ctx, funcName, argc, argv);
    }

    template<typename T> static T getArgument(JSContext *ctx, const char *funcName,
                                              int argc, JSValueConst *argv)
    {
        return std::get<0>(getArguments<T>(ctx, funcName, argc, argv));
    }

    template <typename Func, typename Tuple, std::size_t... I>
    static void jsForwardHelperVoid(Derived *obj, Func func, const Tuple &tuple, std::index_sequence<I...>) {
        (obj->*func)(std::get<I>(tuple)...);
    }
    template <typename Func, typename Ret, typename Tuple, std::size_t... I>
    static Ret jsForwardHelperRet(Derived *obj, Func &func, Tuple const& tuple, std::index_sequence<I...>) {
        return (obj->*func)(std::get<I>(tuple)...);
    }

    static JSValue toJsValue(JSContext *ctx, const QString &s) { return makeJsString(ctx, s); }
    static JSValue toJsValue(JSContext *ctx, bool v) { return JS_NewBool(ctx, v); }
    static JSValue toJsValue(JSContext *ctx, qint32 v) { return JS_NewInt32(ctx, v); }
    static JSValue toJsValue(JSContext *ctx, qint64 v) { return JS_NewInt64(ctx, v); }
    static JSValue toJsValue(JSContext *ctx, const QVariant &v) { return makeJsVariant(ctx, v); }
    static JSValue toJsValue(JSContext *ctx, const QByteArray &data)
    {
        const JSValue array = JS_NewArray(ctx);
        for (int i = 0; i < data.size(); ++i) {
            JS_SetPropertyUint32(ctx, array, i,
                                 JS_NewUint32(ctx, static_cast<unsigned char>(data.at(i))));
        }
        return array;
    }
    static JSValue toJsValue(JSContext *ctx, const QVariantMap &m)
    {
        return makeJsVariantMap(ctx, m);
    }
    template<typename T> static JSValue toJsValue(JSContext *ctx, const std::optional<T> &v)
    {
        if (!v)
            return JS_UNDEFINED;
        return makeJsString(ctx, *v);
    }

    template<typename Func>
    static JSValue jsForward(Func func, const char *name, JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv)
    {
        try {
            using Ret = typename FunctionTrait<Func>::Ret;
            using Args = typename FunctionTrait<Func>::Args;
            const auto args = getArgumentsHelper(Args(), ctx, name, argc, argv);
            const auto obj = fromJsObject(ctx, this_val);
            if constexpr (std::is_same_v<Ret, void>) {
                jsForwardHelperVoid(obj, func, args, std::make_index_sequence<std::tuple_size<decltype(args)>::value>());
                return JS_UNDEFINED;
            } else {
                return toJsValue(ctx, jsForwardHelperRet<Func, Ret>(obj, func, args, std::make_index_sequence<std::tuple_size_v<decltype(args)>>()));
            }
        } catch (const QString &error) {
            return throwError(ctx, error);
        }
    }

    static void setupMethods(JSContext *, JSValue) {}
    static void setupStaticMethods(JSContext *, JSValue) {}
    static void declareEnums(JSContext *, JSValue) {}

private:
    static void finalizer(JSRuntime *rt, JSValue val)
    {
        delete attachedPointer<Derived>(val, classId(rt));
    }

    static JSClassID classId(JSContext *ctx)
    {
        return ScriptEngine::engineForContext(ctx)->getClassId(Derived::name());
    }

    static JSClassID classId(JSRuntime *rt)
    {
        return ScriptEngine::engineForRuntime(rt)->getClassId(Derived::name());
    }

    static JSValue ctor(JSContext *ctx, JSValueConst, JSValueConst, int, JSValueConst *, int)
    {
        return throwError(ctx, Tr::tr("'%1' cannot be instantiated.")
                          .arg(QLatin1String(Derived::name())));
    }
};

template<> struct FromArgHelper<bool> {
    static bool fromArg(JSContext *ctx, const char *, int, JSValue v) {
        return JS_ToBool(ctx, v);
    }
};
template<> struct FromArgHelper<qint32> {
    static qint32 fromArg(JSContext *ctx, const char *funcName, int pos, JSValue v) {
        int32_t val;
        if (JS_ToInt32(ctx, &val, v) < 0) {
            throw Tr::tr("%1 requires an integer as argument %2")
                    .arg(QLatin1String(funcName)).arg(pos);
        }
        return val;
    }
};
template<> struct FromArgHelper<qint64> {
    static qint64 fromArg(JSContext *ctx, const char *funcName, int pos, JSValue v) {
        int64_t val;
        if (JS_ToInt64(ctx, &val, v) < 0) {
            throw Tr::tr("%1 requires an integer as argument %2")
                    .arg(QLatin1String(funcName)).arg(pos);
        }
        return val;
    }
};
template<> struct FromArgHelper<QByteArray> {
    static QByteArray fromArg(JSContext *ctx, const char *funcName, int pos, JSValue v) {
        const auto throwError = [&] {
            throw Tr::tr("%1 requires an array of bytes as argument %2")
                    .arg(QLatin1String(funcName)).arg(pos);
        };
        if (!JS_IsArray(ctx, v))
            throwError();
        QByteArray data;
        data.resize(getJsIntProperty(ctx, v, QLatin1String("length")));
        for (int i = 0; i < data.size(); ++i) {
            const JSValue jsNumber = JS_GetPropertyUint32(ctx, v, i);
            if (JS_VALUE_GET_TAG(jsNumber) != JS_TAG_INT)
                throwError();
            uint32_t n;
            JS_ToUint32(ctx, &n, jsNumber);
            if (n > 0xff)
                throwError();
            data[i] = n;
        }
        return data;
    }
};
template<> struct FromArgHelper<QString> {
    static QString fromArg(JSContext *ctx, const char *funcName, int pos, JSValue v) {
        if (JS_IsUndefined(v))
            return {};
        const ScopedJsValue stringified(ctx, JS_ToString(ctx, v));
        if (!JS_IsString(stringified)) {
            throw Tr::tr("%1 requires a string as argument %2")
                    .arg(QLatin1String(funcName)).arg(pos);
        }
        return getJsString(ctx, stringified);
    }
};
template<> struct FromArgHelper<QStringList> {
    static QStringList fromArg(JSContext *ctx, const char *funcName, int pos, JSValue v) {
        if (!JS_IsArray(ctx, v)) {
            throw Tr::tr("%1 requires an array of strings as argument %2")
                    .arg(QLatin1String(funcName)).arg(pos);
        }
        return getJsStringList(ctx, v);
    }
};
template<> struct FromArgHelper<QVariant> {
    static QVariant fromArg(JSContext *ctx, const char *, int, JSValue v) {
        return getJsVariant(ctx, v);
    }
};
template<class C> struct FromArgHelper<C *> {
    static C *fromArg(JSContext *ctx, const char *funcName, int pos, JSValue v) {
        C * const obj = JsExtension<C>::fromJsObject(ctx, v);
        if (!obj) {
            throw Tr::tr("Argument %2 has wrong type in call to %1")
                    .arg(QLatin1String(funcName)).arg(pos);
        }
        return obj;
    }
};

template <typename R, typename C, typename... A>
struct FunctionTrait<R(C::*)(A... )> {
    using Ret = R;
    using Args = PackHelper<std::remove_reference_t<A>...>;
};
template <typename R, typename C, typename... A>
struct FunctionTrait<R(C::*)(A... ) const> {
    using Ret = R;
    using Args = PackHelper<std::remove_reference_t<A>...>;
};

} // namespace qbs::Internal
