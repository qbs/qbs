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

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/hostosinfo.h>
#include <tools/stringconstants.h>

#include <QtCore/qdir.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>

namespace qbs {
namespace Internal {

class EnvironmentExtension : public QObject, QScriptable
{
    Q_OBJECT
public:
    void initializeJsExtensionEnvironment(QScriptValue extensionObject);
    static QScriptValue js_ctor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getEnv(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_putEnv(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_unsetEnv(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_currentEnv(QScriptContext *context, QScriptEngine *engine);
};

QScriptValue EnvironmentExtension::js_ctor(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    return context->throwError(Tr::tr("'Environment' cannot be instantiated."));
}

static QProcessEnvironment *getProcessEnvironment(QScriptContext *context, QScriptEngine *engine,
                                                  const QString &func, bool doThrow = true)
{
    QVariant v = engine->property(StringConstants::qbsProcEnvVarInternal());
    auto procenv = reinterpret_cast<QProcessEnvironment *>(v.value<void *>());
    if (!procenv && doThrow)
        throw context->throwError(QScriptContext::UnknownError,
                                  QStringLiteral("%1 can only be called from ").arg(func) +
                                  QStringLiteral("Module.setupBuildEnvironment and ") +
                                  QStringLiteral("Module.setupRunEnvironment"));
    return procenv;
}

QScriptValue EnvironmentExtension::js_getEnv(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("getEnv expects 1 argument"));
    const QProcessEnvironment env = static_cast<ScriptEngine *>(engine)->environment();
    const QProcessEnvironment *procenv = getProcessEnvironment(context, engine,
                                                               QStringLiteral("getEnv"), false);
    if (!procenv)
        procenv = &env;

    const QString name = context->argument(0).toString();
    const QString value = procenv->value(name);
    return value.isNull() ? engine->undefinedValue() : value;
}

QScriptValue EnvironmentExtension::js_putEnv(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() != 2))
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("putEnv expects 2 arguments"));
    getProcessEnvironment(context, engine, QStringLiteral("putEnv"))->insert(
                context->argument(0).toString(),
                context->argument(1).toString());
    return engine->undefinedValue();
}

QScriptValue EnvironmentExtension::js_unsetEnv(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("unsetEnv expects 1 argument"));
    getProcessEnvironment(context, engine, QStringLiteral("unsetEnv"))->remove(
                context->argument(0).toString());
    return engine->undefinedValue();
}

QScriptValue EnvironmentExtension::js_currentEnv(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    const QProcessEnvironment env = static_cast<ScriptEngine *>(engine)->environment();
    const QProcessEnvironment *procenv = getProcessEnvironment(context, engine,
                                                               QStringLiteral("currentEnv"), false);
    if (!procenv)
        procenv = &env;
    QScriptValue envObject = engine->newObject();
    const auto keys = procenv->keys();
    for (const QString &key : keys) {
        const QString keyName = HostOsInfo::isWindowsHost() ? key.toUpper() : key;
        envObject.setProperty(keyName, QScriptValue(procenv->value(key)));
    }
    return envObject;
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionEnvironment(QScriptValue extensionObject)
{
    using namespace qbs::Internal;
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue environmentObj = engine->newQMetaObject(&EnvironmentExtension::staticMetaObject,
                                             engine->newFunction(&EnvironmentExtension::js_ctor));
    environmentObj.setProperty(QStringLiteral("getEnv"),
                               engine->newFunction(EnvironmentExtension::js_getEnv, 1));
    environmentObj.setProperty(QStringLiteral("putEnv"),
                               engine->newFunction(EnvironmentExtension::js_putEnv, 2));
    environmentObj.setProperty(QStringLiteral("unsetEnv"),
                               engine->newFunction(EnvironmentExtension::js_unsetEnv, 1));
    environmentObj.setProperty(QStringLiteral("currentEnv"),
                               engine->newFunction(EnvironmentExtension::js_currentEnv, 0));
    extensionObject.setProperty(QStringLiteral("Environment"), environmentObj);
}

Q_DECLARE_METATYPE(qbs::Internal::EnvironmentExtension *)

#include "environmentextension.moc"
