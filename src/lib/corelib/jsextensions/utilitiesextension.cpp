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

#include "jsextensions_p.h"

#include <api/languageinfo.h>
#include <jsextensions/jsextensions.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/hostosinfo.h>
#include <tools/toolchains.h>
#include <tools/version.h>

#if defined(Q_OS_MACOS) || defined(Q_OS_OSX)
#include <tools/applecodesignutils.h>
#endif

#ifdef Q_OS_WIN
#include <tools/msvcinfo.h>
#include <tools/vsenvironmentdetector.h>
#endif

#include <QtCore/qcryptographichash.h>
#include <QtCore/qdir.h>

#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>

namespace qbs {
namespace Internal {

class UtilitiesExtension : public QObject, QScriptable
{
    Q_OBJECT
public:
    static QScriptValue js_ctor(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_canonicalArchitecture(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_canonicalTargetArchitecture(QScriptContext *context,
                                                       QScriptEngine *engine);
    static QScriptValue js_canonicalToolchain(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_cStringQuote(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getHash(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_getNativeSetting(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_kernelVersion(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_nativeSettingGroups(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_rfc1034identifier(QScriptContext *context, QScriptEngine *engine);

    static QScriptValue js_smimeMessageContent(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_certificateInfo(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_signingIdentities(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_msvcCompilerInfo(QScriptContext *context, QScriptEngine *engine);

    static QScriptValue js_versionCompare(QScriptContext *context, QScriptEngine *engine);

    static QScriptValue js_qmlTypeInfo(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue js_builtinExtensionNames(QScriptContext *context, QScriptEngine *engine);
};

static void initializeJsExtensionUtilities(QScriptValue extensionObject)
{
    QScriptEngine *engine = extensionObject.engine();
    QScriptValue environmentObj = engine->newQMetaObject(&UtilitiesExtension::staticMetaObject,
                                             engine->newFunction(&UtilitiesExtension::js_ctor));
    environmentObj.setProperty(QStringLiteral("canonicalArchitecture"),
                               engine->newFunction(UtilitiesExtension::js_canonicalArchitecture, 1));
    environmentObj.setProperty(QStringLiteral("canonicalTargetArchitecture"),
                               engine->newFunction(
                                   UtilitiesExtension::js_canonicalTargetArchitecture, 4));
    environmentObj.setProperty(QStringLiteral("canonicalToolchain"),
                               engine->newFunction(UtilitiesExtension::js_canonicalToolchain));
    environmentObj.setProperty(QStringLiteral("cStringQuote"),
                               engine->newFunction(UtilitiesExtension::js_cStringQuote, 1));
    environmentObj.setProperty(QStringLiteral("getHash"),
                               engine->newFunction(UtilitiesExtension::js_getHash, 1));
    environmentObj.setProperty(QStringLiteral("getNativeSetting"),
                               engine->newFunction(UtilitiesExtension::js_getNativeSetting, 3));
    environmentObj.setProperty(QStringLiteral("kernelVersion"),
                               engine->newFunction(UtilitiesExtension::js_kernelVersion, 0));
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
    environmentObj.setProperty(QStringLiteral("msvcCompilerInfo"),
                               engine->newFunction(UtilitiesExtension::js_msvcCompilerInfo, 1));
    environmentObj.setProperty(QStringLiteral("versionCompare"),
                               engine->newFunction(UtilitiesExtension::js_versionCompare, 2));
    environmentObj.setProperty(QStringLiteral("qmlTypeInfo"),
                               engine->newFunction(UtilitiesExtension::js_qmlTypeInfo, 0));
    environmentObj.setProperty(QStringLiteral("builtinExtensionNames"),
                               engine->newFunction(UtilitiesExtension::js_builtinExtensionNames, 0));
    extensionObject.setProperty(QStringLiteral("Utilities"), environmentObj);
}

QBS_JSEXTENSION_REGISTER(Utilities, &initializeJsExtensionUtilities)

QScriptValue UtilitiesExtension::js_ctor(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(engine);
    return context->throwError(Tr::tr("'Utilities' cannot be instantiated."));
}

QScriptValue UtilitiesExtension::js_canonicalTargetArchitecture(QScriptContext *context,
                                                                QScriptEngine *engine)
{
    const QScriptValue arch = context->argument(0);
    if (arch.isUndefined() || arch.isNull())
        return arch;

    QScriptValue endianness = context->argument(1);
    if (endianness.isUndefined() || endianness.isNull())
        endianness = QString();
    const QScriptValue vendor = context->argument(2);
    const QScriptValue system = context->argument(3);
    const QScriptValue abi = context->argument(4);

    if (!arch.isString() || !endianness.isString()
            || !vendor.isString() || !system.isString() || !abi.isString())
        return context->throwError(QScriptContext::SyntaxError,
            QStringLiteral("canonicalTargetArchitecture expects 1 to 5 arguments of type string"));

    return engine->toScriptValue(canonicalTargetArchitecture(arch.toString(), endianness.toString(),
                                                             vendor.toString(),
                                                             system.toString(), abi.toString()));
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

// copied from src/corelib/tools/qtools_p.h
Q_DECL_CONSTEXPR inline char toHexUpper(uint value) Q_DECL_NOTHROW
{
    return "0123456789ABCDEF"[value & 0xF];
}

Q_DECL_CONSTEXPR inline int fromHex(uint c) Q_DECL_NOTHROW
{
    return ((c >= '0') && (c <= '9')) ? int(c - '0') :
           ((c >= 'A') && (c <= 'F')) ? int(c - 'A' + 10) :
           ((c >= 'a') && (c <= 'f')) ? int(c - 'a' + 10) :
           /* otherwise */              -1;
}

// copied from src/corelib/io/qdebug.cpp
static inline bool isPrintable(uchar c)
{ return c >= ' ' && c < 0x7f; }

// modified
template <typename Char>
static inline QString escapedString(const Char *begin, int length, bool isUnicode = true)
{
    QChar quote(QLatin1Char('"'));
    QString out = quote;

    bool lastWasHexEscape = false;
    const Char *end = begin + length;
    for (const Char *p = begin; p != end; ++p) {
        // check if we need to insert "" to break an hex escape sequence
        if (Q_UNLIKELY(lastWasHexEscape)) {
            if (fromHex(*p) != -1) {
                // yes, insert it
                out += QLatin1Char('"');
                out += QLatin1Char('"');
            }
            lastWasHexEscape = false;
        }

        if (sizeof(Char) == sizeof(QChar)) {
            // Surrogate characters are category Cs (Other_Surrogate), so isPrintable = false for them
            int runLength = 0;
            while (p + runLength != end &&
                   isPrintable(p[runLength]) && p[runLength] != '\\' && p[runLength] != '"')
                ++runLength;
            if (runLength) {
                out += QString(reinterpret_cast<const QChar *>(p), runLength);
                p += runLength - 1;
                continue;
            }
        } else if (isPrintable(*p) && *p != '\\' && *p != '"') {
            QChar c = QLatin1Char(*p);
            out += c;
            continue;
        }

        // print as an escape sequence (maybe, see below for surrogate pairs)
        int buflen = 2;
        ushort buf[sizeof "\\U12345678" - 1];
        buf[0] = '\\';

        switch (*p) {
        case '"':
        case '\\':
            buf[1] = *p;
            break;
        case '\b':
            buf[1] = 'b';
            break;
        case '\f':
            buf[1] = 'f';
            break;
        case '\n':
            buf[1] = 'n';
            break;
        case '\r':
            buf[1] = 'r';
            break;
        case '\t':
            buf[1] = 't';
            break;
        default:
            if (!isUnicode) {
                // print as hex escape
                buf[1] = 'x';
                buf[2] = toHexUpper(uchar(*p) >> 4);
                buf[3] = toHexUpper(uchar(*p));
                buflen = 4;
                lastWasHexEscape = true;
                break;
            }
            if (QChar::isHighSurrogate(*p)) {
                if ((p + 1) != end && QChar::isLowSurrogate(p[1])) {
                    // properly-paired surrogates
                    uint ucs4 = QChar::surrogateToUcs4(*p, p[1]);
                    if (isPrintable(ucs4)) {
                        buf[0] = *p;
                        buf[1] = p[1];
                        buflen = 2;
                    } else {
                        buf[1] = 'U';
                        buf[2] = '0'; // toHexUpper(ucs4 >> 32);
                        buf[3] = '0'; // toHexUpper(ucs4 >> 28);
                        buf[4] = toHexUpper(ucs4 >> 20);
                        buf[5] = toHexUpper(ucs4 >> 16);
                        buf[6] = toHexUpper(ucs4 >> 12);
                        buf[7] = toHexUpper(ucs4 >> 8);
                        buf[8] = toHexUpper(ucs4 >> 4);
                        buf[9] = toHexUpper(ucs4);
                        buflen = 10;
                    }
                    ++p;
                    break;
                }
                // improperly-paired surrogates, fall through
            }
            buf[1] = 'u';
            buf[2] = toHexUpper(ushort(*p) >> 12);
            buf[3] = toHexUpper(ushort(*p) >> 8);
            buf[4] = toHexUpper(*p >> 4);
            buf[5] = toHexUpper(*p);
            buflen = 6;
        }
        out += QString(reinterpret_cast<QChar *>(buf), buflen);
    }

    out += quote;
    return out;
}

QScriptValue UtilitiesExtension::js_cStringQuote(QScriptContext *context, QScriptEngine *engine)
{
    if (Q_UNLIKELY(context->argumentCount() < 1)) {
        return context->throwError(QScriptContext::SyntaxError,
                                   QStringLiteral("cStringQuote expects 1 argument"));
    }
    QString value = context->argument(0).toString();
    return engine->toScriptValue(escapedString(reinterpret_cast<const ushort *>(value.constData()), value.size()));
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

QScriptValue UtilitiesExtension::js_kernelVersion(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    return engine->toScriptValue(QSysInfo::kernelVersion());
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
#if !defined(Q_OS_MACOS) && !defined(Q_OS_OSX)
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
#if !defined(Q_OS_MACOS) && !defined(Q_OS_OSX)
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
#if !defined(Q_OS_MACOS) && !defined(Q_OS_OSX)
    Q_UNUSED(engine);
    return context->throwError(QScriptContext::UnknownError,
        QLatin1String("signingIdentities is not available on this platform"));
#else
    Q_UNUSED(context);
    return engine->toScriptValue(identitiesProperties());
#endif
}

QScriptValue UtilitiesExtension::js_msvcCompilerInfo(QScriptContext *context, QScriptEngine *engine)
{
#ifndef Q_OS_WIN
    Q_UNUSED(engine);
    return context->throwError(QScriptContext::UnknownError,
        QLatin1String("msvcCompilerInfo is not available on this platform"));
#else
    if (Q_UNLIKELY(context->argumentCount() != 1))
        return context->throwError(QScriptContext::SyntaxError,
                                   QLatin1String("msvcCompilerInfo expects 1 argument"));

    const QString compilerFilePath = context->argument(0).toString();
    MSVC msvc(compilerFilePath);
    VsEnvironmentDetector envdetector;
    if (!envdetector.start(&msvc))
        return context->throwError(QScriptContext::UnknownError,
                                   QStringLiteral("Detecting the MSVC build environment failed: ")
                                   + envdetector.errorString());

    try {
        QVariantMap envMap;
        for (const QString &key : msvc.environment.keys())
            envMap.insert(key, msvc.environment.value(key));

        return engine->toScriptValue(QVariantMap {
            {QStringLiteral("buildEnvironment"), envMap},
            {QStringLiteral("macros"), msvc.compilerDefines(compilerFilePath)},
        });
    } catch (const qbs::ErrorInfo &info) {
        return context->throwError(QScriptContext::UnknownError,
                                   info.toString());
    }
#endif
}

QScriptValue UtilitiesExtension::js_versionCompare(QScriptContext *context, QScriptEngine *engine)
{
    if (context->argumentCount() == 2) {
        const QScriptValue value1 = context->argument(0);
        const QScriptValue value2 = context->argument(1);
        if (value1.isString() && value2.isString()) {
            const auto a = Internal::Version::fromString(value1.toString());
            const auto b = Internal::Version::fromString(value2.toString());
            return engine->toScriptValue(Internal::compare(a, b));
        }
    }

    return context->throwError(QScriptContext::SyntaxError,
        QStringLiteral("versionCompare expects two arguments of type string"));
}

QScriptValue UtilitiesExtension::js_qmlTypeInfo(QScriptContext *context, QScriptEngine *engine)
{
    Q_UNUSED(context);
    return engine->toScriptValue(QString::fromUtf8(qbs::LanguageInfo().qmlTypeInfo()));
}

QScriptValue UtilitiesExtension::js_builtinExtensionNames(QScriptContext *context,
                                                          QScriptEngine *engine)
{
    Q_UNUSED(context);
    return engine->toScriptValue(JsExtensions::extensionNames());
}

} // namespace Internal
} // namespace qbs

Q_DECLARE_METATYPE(qbs::Internal::UtilitiesExtension *)

#include "utilitiesextension.moc"
