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

#include "jsextension.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>

namespace qbs {
namespace Internal {

class EnvironmentExtension : public JsExtension<EnvironmentExtension>
{
public:
    static const char *name() { return "Environment"; }
    static void setupStaticMethods(JSContext *ctx, JSValue classObj);
    static JSValue jsGetEnv(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsPutEnv(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsUnsetEnv(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue jsCurrentEnv(JSContext *ctx, JSValueConst, int, JSValueConst *);
};

void EnvironmentExtension::setupStaticMethods(JSContext *ctx, JSValue classObj)
{
    setupMethod(ctx, classObj, "getEnv", &EnvironmentExtension::jsGetEnv, 1);
    setupMethod(ctx, classObj, "putEnv", &EnvironmentExtension::jsPutEnv, 2);
    setupMethod(ctx, classObj, "unsetEnv", &EnvironmentExtension::jsUnsetEnv, 1);
    setupMethod(ctx, classObj, "currentEnv", &EnvironmentExtension::jsCurrentEnv, 0);
}

static QProcessEnvironment *getProcessEnvironment(ScriptEngine *engine, const QString &func,
                                                  bool doThrow = true)
{
    QVariant v = engine->property(StringConstants::qbsProcEnvVarInternal());
    auto procenv = reinterpret_cast<QProcessEnvironment *>(v.value<void *>());
    if (!procenv && doThrow) {
        throw QStringLiteral("%1 can only be called from "
                             "Module.setupBuildEnvironment and "
                             "Module.setupRunEnvironment").arg(func);
    }
    return procenv;
}

JSValue EnvironmentExtension::jsGetEnv(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    try {
        const auto name = getArgument<QString>(ctx, "Environment.getEnv", argc, argv);
        ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
        const QProcessEnvironment env = engine->environment();
        const QProcessEnvironment *procenv = getProcessEnvironment(engine, QStringLiteral("getEnv"),
                                                                   false);
        if (!procenv)
            procenv = &env;
        const QString value = procenv->value(name);
        return value.isNull() ? engine->undefinedValue() : makeJsString(ctx, value);
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue EnvironmentExtension::jsPutEnv(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto args = getArguments<QString, QString>(ctx, "Environment.putEnv", argc, argv);
        getProcessEnvironment(ScriptEngine::engineForContext(ctx), QStringLiteral("putEnv"))
                ->insert(std::get<0>(args), std::get<1>(args));
        return JS_UNDEFINED;
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue EnvironmentExtension::jsUnsetEnv(JSContext *ctx, JSValue, int argc, JSValue *argv)
{
    try {
        const auto name = getArgument<QString>(ctx, "Environment.unsetEnv", argc, argv);
        getProcessEnvironment(ScriptEngine::engineForContext(ctx), QStringLiteral("unsetEnv"))
                ->remove(name);
        return JS_UNDEFINED;
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue EnvironmentExtension::jsCurrentEnv(JSContext *ctx, JSValue, int, JSValue *)
{
    ScriptEngine * const engine = ScriptEngine::engineForContext(ctx);
    const QProcessEnvironment env = engine->environment();
    const QProcessEnvironment *procenv = getProcessEnvironment(engine, QStringLiteral("currentEnv"),
                                                               false);
    if (!procenv)
        procenv = &env;
    JSValue envObject = engine->newObject();
    const auto keys = procenv->keys();
    for (const QString &key : keys) {
        const QString keyName = HostOsInfo::isWindowsHost() ? key.toUpper() : key;
        setJsProperty(ctx, envObject, keyName, procenv->value(key));
    }
    return envObject;
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionEnvironment(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::EnvironmentExtension::registerClass(engine, extensionObject);
}
