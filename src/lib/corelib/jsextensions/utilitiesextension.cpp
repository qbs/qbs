/****************************************************************************
**
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

#include "utilitiesextension.h"

#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/fileinfo.h>
#include <tools/toolchains.h>

#ifdef Q_OS_OSX
#include <tools/applecodesignutils.h>
#endif

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScriptable>
#include <QScriptEngine>

namespace qbs {
namespace Internal {

class UtilitiesExtension : public QObject, QScriptable
{
    Q_OBJECT
public:
    static QScriptValue js_ctor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_canonicalArchitecture(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_canonicalToolchain(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getHash(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getNativeSetting(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_nativeSettingGroups(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_rfc1034identifier(QScriptContext *context, QScriptEngine *engine);

    static QScriptValue js_smimeMessageContent(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_certificateInfo(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_signingIdentities(QScriptContext *context, QScriptEngine *engine);
};

void initializeJsExtensionUtilities(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue environmentObj = engine->newQMetaObject(&UtilitiesExtension::staticMetaObject,
                                             engine->newFunction(&UtilitiesExtension::js_ctor));
    environmentObj.setProperty(QStringLiteral("canonicalArchitecture"),
                               engine->newFunction(UtilitiesExtension::js_canonicalArchitecture, 1));
    environmentObj.setProperty(QStringLiteral("canonicalToolchain"),
                               engine->newFunction(UtilitiesExtension::js_canonicalToolchain));
    environmentObj.setProperty(QStringLiteral("getHash"),
                               engine->newFunction(UtilitiesExtension::js_getHash, 1));
    environmentObj.setProperty(QStringLiteral("getNativeSetting"),
                               engine->newFunction(UtilitiesExtension::js_getNativeSetting, 3));
    environmentObj.setProperty(QStringLiteral("nativeSettingGroups"),
                               engine->newFunction(UtilitiesExtension::js_nativeSettingGroups, 1));
    environmentObj.setProperty(QStringLiteral("rfc1034Identifier"),
                               engine->newFunction(UtilitiesExtension::js_rfc1034identifier, 1));
    environmentObj.setProperty(QStringLiteral("smimeMessageContent"),
                               engine->newFunction(UtilitiesExtension::js_smimeMessageContent, 1));
    environmentObj.setProperty(QStringLiteral("certificateInfo"),
                               engine->newFunction(UtilitiesExtension::js_certificateInfo, 1));
    environmentObj.setProperty(QStringLiteral("signingIdentities"),
                               engine->newFunction(UtilitiesExtension::js_signingIdentities, 0));
    extensionObject.setProperty(QStringLiteral("Utilities"), environmentObj);
}

QScriptValue UtilitiesExtension::js_ctor(QScriptContext *context, QScriptEngine *engine)
{
    // Add instance variables here etc., if needed.
    Q_UNUSED(context);
    Q_UNUSED(engine);
    return true;
}

QScriptValue UtilitiesExtension::js_canonicalArchitecture(QScriptContext *context,
                                                          QScriptEngine *engine)
{
    const QScriptValue value = context->argument(0);
    if (value.isUndefined() || value.isNull())
        return value;

    if (context->argumentCount() == 1 && value.isString())
        return engine->toScriptValue(canonicalArchitecture(value.toString()));

    return context->throwError(QScriptContext::SyntaxError,
        QStringLiteral("canonicalArchitecture expects one argument of type string"));
}

QScriptValue UtilitiesExtension::js_canonicalToolchain(QScriptContext *context,
                                                       QScriptEngine *engine)
{
    QStringList toolchain;
    for (int i = 0; i < context->argumentCount(); ++i)
        toolchain << context->argument(i).toString();
    return engine->toScriptValue(canonicalToolchain(toolchain));
}

QScriptValue UtilitiesExtension::js_getHash(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("getHash expects 1 argument"));
    }
    const QByteArray input = context->argument(0).toString().toLatin1();
    const QByteArray hash
            = QCryptographicHash::hash(input, QCryptographicHash::Sha1).toHex().left(16);
    return engine->toScriptValue(QString::fromLatin1(hash));
}

QScriptValue UtilitiesExtension::js_getNativeSetting(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1 || context->argumentCount() > 3)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("getNativeSetting expects between 1 and 3 arguments"));
    }

    QString key = context->argumentCount() > 1 ? context->argument(1).toString() : QString();

    // We'll let empty string represent the default registry value
    if (HostOsInfo::isWindowsHost() && key.isEmpty())
        key = QLatin1String(".");

    QVariant defaultValue = context->argumentCount() > 2 ? context->argument(2).toVariant() : QVariant();

    QSettings settings(context->argument(0).toString(), QSettings::NativeFormat);
    QVariant value = settings.value(key, defaultValue);
    return value.isNull() ? engine->undefinedValue() : engine->toScriptValue(value);
}

QScriptValue UtilitiesExtension::js_nativeSettingGroups(QScriptContext *context,
                                                        QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() != 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("nativeSettingGroups expects 1 argument"));
    }

    QSettings settings(context->argument(0).toString(), QSettings::NativeFormat);
    return engine->toScriptValue(settings.childGroups());
}

QScriptValue UtilitiesExtension::js_rfc1034identifier(QScriptContext *context,
                                                      QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("rfc1034Identifier expects 1 argument"));
    const QString identifier = context->argument(0).toString();
    return engine->toScriptValue(HostOsInfo::rfc1034Identifier(identifier));
}

/**
 * Reads the contents of the S/MIME message located at \p filePath.
 * An equivalent command line would be:
 * \code security cms -D -i <infile> -o <outfile> \endcode
 * or:
 * \code openssl smime -verify -noverify -inform DER -in <infile> -out <outfile> \endcode
 *
 * \note A provisioning profile is an S/MIME message whose contents are an XML property list,
 * so this method can be used to read such files.
 */
QScriptValue UtilitiesExtension::js_smimeMessageContent(QScriptContext *context,
                                                        QScriptEngine *engine)
{
#ifndef Q_OS_OSX
    Q_UNUSED(engine);
    return context->throwError(QScriptContext::UnknownError,
        QLatin1String("smimeMessageContent is not available on this platform"));
#else
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("smimeMessageContent expects 1 argument"));

    const QString filePath = context->argument(0).toString();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return engine->undefinedValue();

    QByteArray content = smimeMessageContent(file.readAll());
    if (content.isEmpty())
        return engine->undefinedValue();
    return engine->toScriptValue(content);
#endif
}

QScriptValue UtilitiesExtension::js_certificateInfo(QScriptContext *context,
                                                    QScriptEngine *engine)
{
#ifndef Q_OS_OSX
    Q_UNUSED(engine);
    return context->throwError(QScriptContext::UnknownError,
        QLatin1String("certificateInfo is not available on this platform"));
#else
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("certificateInfo expects 1 argument"));
    return engine->toScriptValue(certificateInfo(context->argument(0).toVariant().toByteArray()));
#endif
}

// Rough command line equivalent: security find-identity -p codesigning -v
QScriptValue UtilitiesExtension::js_signingIdentities(QScriptContext *context,
                                                      QScriptEngine *engine)
{
#ifndef Q_OS_OSX
    Q_UNUSED(engine);
    return context->throwError(QScriptContext::UnknownError,
        QLatin1String("signingIdentities is not available on this platform"));
#else
    Q_UNUSED(context);
    return engine->toScriptValue(identitiesProperties());
#endif
}

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::UtilitiesExtension *)

#include "utilitiesextension.moc"
