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
#include "jsextensions.h"

#include <api/languageinfo.h>
#include <language/scriptengine.h>
#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/hostosinfo.h>
#include <tools/stlutils.h>
#include <tools/stringconstants.h>
#include <tools/toolchains.h>
#include <tools/version.h>

#if defined(Q_OS_MACOS) || defined(Q_OS_OSX)
#include <tools/applecodesignutils.h>
#endif

#ifdef __APPLE__
#include <ar.h>
#include <mach/machine.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#ifndef FAT_MAGIC_64
#define FAT_MAGIC_64 0xcafebabf
#define FAT_CIGAM_64 0xbfbafeca
struct fat_arch_64 {
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    uint64_t offset;
    uint64_t size;
    uint32_t align;
    uint32_t reserved;
};
#endif
#endif


#ifdef Q_OS_WIN
#include <tools/clangclinfo.h>
#include <tools/msvcinfo.h>
#include <tools/vsenvironmentdetector.h>
#endif

#include <QtCore/qcryptographichash.h>
#include <QtCore/qdir.h>
#include <QtCore/qendian.h>
#include <QtCore/qfile.h>
#include <QtCore/qlibrary.h>

namespace qbs {
namespace Internal {

class DummyLogSink : public ILogSink {
    void doPrintMessage(LoggerLevel, const QString &, const QString &) override { }
};

class UtilitiesExtension : public JsExtension<UtilitiesExtension>
{
public:
    static const char *name() { return "Utilities"; }
    static void setupStaticMethods(JSContext *ctx, JSValue classObj);
    static JSValue js_canonicalArchitecture(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_canonicalPlatform(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue js_canonicalTargetArchitecture(JSContext *ctx, JSValueConst,
                                                  int, JSValueConst *);
    static JSValue js_canonicalToolchain(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_cStringQuote(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue js_getHash(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue js_getNativeSetting(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue js_kernelVersion(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_nativeSettingGroups(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_rfc1034identifier(JSContext *ctx, JSValueConst, int, JSValueConst *);

    static JSValue js_smimeMessageContent(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_certificateInfo(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_signingIdentities(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_msvcCompilerInfo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue js_clangClCompilerInfo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue js_installedMSVCs(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_installedClangCls(JSContext *ctx, JSValueConst, int, JSValueConst *);

    static JSValue js_versionCompare(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);

    static JSValue js_qmlTypeInfo(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_builtinExtensionNames(JSContext *ctx, JSValueConst, int, JSValueConst *);
    static JSValue js_isSharedLibrary(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);

    static JSValue js_getArchitecturesFromBinary(JSContext *ctx, JSValueConst, int, JSValueConst *);
};

JSValue UtilitiesExtension::js_canonicalPlatform(JSContext *ctx, JSValueConst,
                                                 int argc, JSValueConst *argv)
{
    try {
        const auto platform = getArgument<QString>(ctx, "Utilities.canonicalPlatform", argc, argv);
        if (platform.isEmpty())
            return makeJsStringList(ctx, {});
        std::vector<QString> platforms = HostOsInfo::canonicalOSIdentifiers(platform);
        return makeJsStringList(ctx, QStringList(std::move_iterator(platforms.begin()),
                                                 std::move_iterator(platforms.end())));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

JSValue UtilitiesExtension::js_canonicalTargetArchitecture(JSContext *ctx, JSValueConst,
                                                           int argc, JSValueConst *argv)
{
    try {
        const auto arch = getArgument<QString>(ctx, "Utilities.canonicalTargetArchitecture",
                                               argc, argv);
        QString endianness, vendor, system, abi;
        if (argc > 1)
            endianness = fromArg<QString>(ctx, "Utilities.canonicalTargetArchitecture", 2, argv[1]);
        if (argc > 2)
            vendor = fromArg<QString>(ctx, "Utilities.canonicalTargetArchitecture", 3, argv[2]);
        if (argc > 3)
            system = fromArg<QString>(ctx, "Utilities.canonicalTargetArchitecture", 4, argv[3]);
        if (argc > 4)
            abi = fromArg<QString>(ctx, "Utilities.canonicalTargetArchitecture", 5, argv[4]);
        return makeJsString(ctx,
                            canonicalTargetArchitecture(arch, endianness, vendor, system, abi));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

void UtilitiesExtension::setupStaticMethods(JSContext *ctx, JSValue classObj)
{
    setupMethod(ctx, classObj, "canonicalArchitecture",
                      &UtilitiesExtension::js_canonicalArchitecture, 1);
    setupMethod(ctx, classObj, "canonicalPlatform",
                      &UtilitiesExtension::js_canonicalPlatform, 1);
    setupMethod(ctx, classObj, "canonicalTargetArchitecture",
                      &UtilitiesExtension::js_canonicalTargetArchitecture, 1);
    setupMethod(ctx, classObj, "canonicalToolchain",
                      &UtilitiesExtension::js_canonicalToolchain, 1);
    setupMethod(ctx, classObj, "cStringQuote", &UtilitiesExtension::js_cStringQuote, 1);
    setupMethod(ctx, classObj, "getHash", &UtilitiesExtension::js_getHash, 1);
    setupMethod(ctx, classObj, "getNativeSetting",
                      &UtilitiesExtension::js_getNativeSetting, 3);
    setupMethod(ctx, classObj, "kernelVersion", &UtilitiesExtension::js_kernelVersion, 0);
    setupMethod(ctx, classObj, "nativeSettingGroups",
                      &UtilitiesExtension::js_nativeSettingGroups, 1);
    setupMethod(ctx, classObj, "rfc1034Identifier",
                      &UtilitiesExtension::js_rfc1034identifier, 1);
    setupMethod(ctx, classObj, "smimeMessageContent",
                      &UtilitiesExtension::js_smimeMessageContent, 1);
    setupMethod(ctx, classObj, "certificateInfo", &UtilitiesExtension::js_certificateInfo, 1);
    setupMethod(ctx, classObj, "signingIdentities",
                      &UtilitiesExtension::js_signingIdentities, 1);
    setupMethod(ctx, classObj, "msvcCompilerInfo",
                      &UtilitiesExtension::js_msvcCompilerInfo, 1);
    setupMethod(ctx, classObj, "clangClCompilerInfo",
                      &UtilitiesExtension::js_clangClCompilerInfo, 1);
    setupMethod(ctx, classObj, "installedMSVCs", &UtilitiesExtension::js_installedMSVCs, 1);
    setupMethod(ctx, classObj, "installedClangCls",
                      &UtilitiesExtension::js_installedClangCls, 1);
    setupMethod(ctx, classObj, "versionCompare", &UtilitiesExtension::js_versionCompare, 2);
    setupMethod(ctx, classObj, "qmlTypeInfo", &UtilitiesExtension::js_qmlTypeInfo, 0);
    setupMethod(ctx, classObj, "builtinExtensionNames",
                      &UtilitiesExtension::js_builtinExtensionNames, 0);
    setupMethod(ctx, classObj, "isSharedLibrary", &UtilitiesExtension::js_isSharedLibrary, 1);
    setupMethod(ctx, classObj, "getArchitecturesFromBinary",
                      &UtilitiesExtension::js_getArchitecturesFromBinary, 1);
}

JSValue UtilitiesExtension::js_canonicalArchitecture(JSContext *ctx, JSValueConst,
                                                     int argc, JSValueConst *argv)
{
    try {
        const auto arch = getArgument<QString>(ctx, "Utilities.canonicalArchitecture", argc, argv);
        if (arch.isEmpty())
            return makeJsString(ctx, {});
        return makeJsString(ctx, canonicalArchitecture(arch));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

JSValue UtilitiesExtension::js_canonicalToolchain(JSContext *ctx, JSValueConst,
                                                  int argc, JSValueConst *argv)
{
    try {
        QStringList toolchain;
        for (int i = 0; i < argc; ++i)
            toolchain << fromArg<QString>(ctx, "Utilities.canonicalToolchain", i + 1, argv[i]);
        return makeJsStringList(ctx, canonicalToolchain(toolchain));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

// copied from src/corelib/tools/qtools_p.h
Q_DECL_CONSTEXPR inline uchar toHexUpper(uint value) Q_DECL_NOTHROW
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
                   QChar::isPrint(p[runLength]) && p[runLength] != '\\' && p[runLength] != '"')
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
                    if (QChar::isPrint(ucs4)) {
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

JSValue UtilitiesExtension::js_cStringQuote(JSContext *ctx, JSValueConst,
                                            int argc, JSValueConst *argv)
{
    try {
        const auto str = getArgument<QString>(ctx, "Utilities.cStringQuote", argc, argv);
        return makeJsString(ctx, escapedString(
                                reinterpret_cast<const ushort *>(str.constData()), str.size()));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

JSValue UtilitiesExtension::js_getHash(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
    try {
        const QByteArray input = getArgument<QString>(ctx, "Utilities.getHash", argc, argv)
                .toLatin1();
        const QByteArray hash = QCryptographicHash::hash(input, QCryptographicHash::Sha1)
                .toHex().left(16);
        return makeJsString(ctx, QString::fromLatin1(hash));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

JSValue UtilitiesExtension::js_getNativeSetting(JSContext *ctx, JSValueConst,
                                                int argc, JSValueConst *argv)
{
    try {
        const auto path = getArgument<QString>(ctx, "Utilities.getNativeSetting", argc, argv);

        QString key;
        if (argc > 1)
            key = fromArg<QString>(ctx, "Utilities.getNativeSetting", 2, argv[1]);

        // QSettings will ignore the default value if an empty key is passed.
        if (key.isEmpty()) {
            key = HostOsInfo::isWindowsHost() ? StringConstants::dot() // default registry value
                                              : QLatin1String("__dummy");
        }

        QVariant defaultValue;
        if (argc > 2)
            defaultValue = fromArg<QVariant>(ctx, "Utilities.getNativeSetting", 3, argv[2]);

        const QSettings settings(path, QSettings::NativeFormat);
        const QVariant v = settings.value(key, defaultValue);
        return v.isNull() ? JS_UNDEFINED : makeJsVariant(ctx, v);
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

JSValue UtilitiesExtension::js_kernelVersion(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    return makeJsString(ctx, QSysInfo::kernelVersion());
}

JSValue UtilitiesExtension::js_nativeSettingGroups(JSContext *ctx, JSValueConst,
                                                   int argc, JSValueConst *argv)
{
    try {
        const auto path = getArgument<QString>(ctx, "Utilities.nativeSettingGroups", argc, argv);
        QSettings settings(path, QSettings::NativeFormat);
        return makeJsStringList(ctx, settings.childGroups());
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
}

JSValue UtilitiesExtension::js_rfc1034identifier(JSContext *ctx, JSValueConst,
                                                 int argc, JSValueConst *argv)
{
    try {
        const auto identifier = getArgument<QString>(ctx, "Utilities.rfc1034identifier", argc, argv);
        return makeJsString(ctx, HostOsInfo::rfc1034Identifier(identifier));
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
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
JSValue UtilitiesExtension::js_smimeMessageContent(JSContext *ctx, JSValueConst,
                                                   int argc, JSValueConst *argv)
{
#if !defined(Q_OS_MACOS) && !defined(Q_OS_OSX)
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    return throwError(ctx, QStringLiteral("smimeMessageContent is not available on this platform"));
#else
    try {
        const QString filePath = getArgument<QString>(ctx, "Utilities.smimeMessageContent",
                                                      argc, argv);
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return JS_UNDEFINED;

        QByteArray content = smimeMessageContent(file.readAll());
        if (content.isEmpty())
            return JS_UNDEFINED;
        return toJsValue(ctx, content);
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
#endif
}

JSValue UtilitiesExtension::js_certificateInfo(JSContext *ctx, JSValueConst,
                                               int argc, JSValueConst *argv)
{
#if !defined(Q_OS_MACOS) && !defined(Q_OS_OSX)
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    return throwError(ctx, QStringLiteral("certificateInfo is not available on this platform"));
#else
    try {
        const QVariant arg = getArgument<QVariant>(ctx, "Utilities.certificateInfo", argc, argv);
        return toJsValue(ctx, certificateInfo(arg.toByteArray()));
    }  catch (const QString &error) {
        return throwError(ctx, error);
    }
#endif
}

// Rough command line equivalent: security find-identity -p codesigning -v
JSValue UtilitiesExtension::js_signingIdentities(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
#if !defined(Q_OS_MACOS) && !defined(Q_OS_OSX)
    return throwError(ctx, QStringLiteral("signingIdentities is not available on this platform"));
#else
    return makeJsVariantMap(ctx, identitiesProperties());
#endif
}

#ifdef Q_OS_WIN
// Try to detect the cross-architecture from the compiler path; prepend the host architecture
// if it is differ than the target architecture (e.g. something like x64_x86 and so forth).
static QString detectArchitecture(const QString &compilerFilePath, const QString &targetArch)
{
    const auto startIndex = compilerFilePath.lastIndexOf(QLatin1String("Host"));
    if (startIndex == -1)
        return targetArch;
    const auto endIndex = compilerFilePath.indexOf(QLatin1Char('/'), startIndex);
    if (endIndex == -1)
        return targetArch;
    const auto hostArch = compilerFilePath.mid(startIndex + 4, endIndex - startIndex - 4).toLower();
    if (hostArch.isEmpty() || (hostArch == targetArch))
        return targetArch;
    return hostArch + QLatin1Char('_') + targetArch;
}

static std::pair<QVariantMap /*result*/, QString /*error*/> msvcCompilerInfoHelper(
        const QString &compilerFilePath,
        MSVC::CompilerLanguage language,
        const QString &vcvarsallPath,
        const QString &arch,
        const QString &sdkVersion)
{
    QString detailedArch = detectArchitecture(compilerFilePath, arch);
    MSVC msvc(compilerFilePath, std::move(detailedArch), sdkVersion);
    VsEnvironmentDetector envdetector(vcvarsallPath);
    if (!envdetector.start(&msvc))
        return { {}, QStringLiteral("Detecting the MSVC build environment failed: ")
                    + envdetector.errorString() };

    try {
        QVariantMap envMap;
        const auto keys = msvc.environment.keys();
        for (const QString &key : keys)
            envMap.insert(key, msvc.environment.value(key));

        return {
            QVariantMap {
                {QStringLiteral("buildEnvironment"), envMap},
                {QStringLiteral("macros"), msvc.compilerDefines(compilerFilePath, language)},
            },
            {}
        };
    } catch (const qbs::ErrorInfo &info) {
        return { {}, info.toString() };
    }
}
#endif

JSValue UtilitiesExtension::js_msvcCompilerInfo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
#ifndef Q_OS_WIN
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    return throwError(ctx, QStringLiteral("msvcCompilerInfo is not available on this platform"));
#else
    try {
        const auto args = getArguments<QString, QString, QString>(ctx, "Utilities.msvcCompilerInfo", argc, argv);
        const QString compilerFilePath = std::get<0>(args);
        const QString compilerLanguage = std::get<1>(args);
        const QString sdkVersion = std::get<2>(args);
        MSVC::CompilerLanguage language;
        if (compilerLanguage == QStringLiteral("c"))
            language = MSVC::CLanguage;
        else if (compilerLanguage == StringConstants::cppLang())
            language = MSVC::CPlusPlusLanguage;
        else
            throw Tr::tr("Utilities.msvcCompilerInfo expects \"c\" or \"cpp\" as its second argument");
        const auto result = msvcCompilerInfoHelper(
                    compilerFilePath,
                    language,
                    {},
                    MSVC::architectureFromClPath(compilerFilePath),
                    sdkVersion);
        if (result.first.isEmpty())
            throw result.second;
        return toJsValue(ctx, result.first);
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
#endif
}

JSValue UtilitiesExtension::js_clangClCompilerInfo(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
#ifndef Q_OS_WIN
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    return throwError(ctx, QStringLiteral("clangClCompilerInfo is not available on this platform"));
#else
    try {
        const auto args = getArguments<QString, QString, QString, QString, QString>(ctx, "Utilities.clangClCompilerInfo", argc, argv);
    const QString compilerFilePath = std::get<0>(args);

    // architecture cannot be empty as vcvarsall.bat requires at least 1 arg, so fallback
    // to host architecture if none is present
    QString arch = std::get<1>(args);
    if (arch.isEmpty())
        arch = HostOsInfo::hostOSArchitecture();

    const QString vcvarsallPath = std::get<2>(args);
    const QString compilerLanguage = std::get<3>(args);
    const QString sdkVersion = std::get<4>(args);
    MSVC::CompilerLanguage language;
    if (compilerLanguage == QStringLiteral("c"))
        language = MSVC::CLanguage;
    else if (compilerLanguage == StringConstants::cppLang())
        language = MSVC::CPlusPlusLanguage;
    else
        throw Tr::tr("Utilities.clangClCompilerInfo expects \"c\" or \"cpp\" as its fourth argument");

    const auto result = msvcCompilerInfoHelper(
            compilerFilePath, language, vcvarsallPath, arch, sdkVersion);
    if (result.first.isEmpty())
        throw result.second;
    return toJsValue(ctx, result.first);
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
#endif
}

JSValue UtilitiesExtension::js_installedMSVCs(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
#ifndef Q_OS_WIN
    return throwError(ctx, QStringLiteral("Utilities.installedMSVCs is not available on this platform"));
    Q_UNUSED(argc)
    Q_UNUSED(argv)
#else
    try {
        const QString hostArch = HostOsInfo::hostOSArchitecture();
        QString preferredArch = getArgument<QString>(ctx, "Utilities.installedMSVCs", argc, argv);
        if (preferredArch.isEmpty())
            preferredArch = hostArch;

        DummyLogSink dummySink;
        Logger dummyLogger(&dummySink);
        auto msvcs = MSVC::installedCompilers(dummyLogger);

        const auto predicate = [&preferredArch, &hostArch](const MSVC &msvc)
        {
            auto archPair = MSVC::getHostTargetArchPair(msvc.architecture);
            return archPair.first != hostArch || preferredArch != archPair.second;
        };
        Internal::removeIf(msvcs, predicate);
        QVariantList result;
        for (const auto &msvc: msvcs)
            result.append(msvc.toVariantMap());
        return makeJsVariantList(ctx, result);
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
#endif
}

JSValue UtilitiesExtension::js_installedClangCls(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
{
#ifndef Q_OS_WIN
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    return throwError(ctx, QStringLiteral("Utilities.installedClangCls is not available on this platform"));
#else
    try {
        const QString path = getArgument<QString>(ctx, "Utilities.installedClangCls", argc, argv);
        DummyLogSink dummySink;
        Logger dummyLogger(&dummySink);
        auto compilers = ClangClInfo::installedCompilers({path}, dummyLogger);
        QVariantList result;
        for (const auto &compiler: compilers)
            result.append(compiler.toVariantMap());
        return makeJsVariantList(ctx, result);
    } catch (const QString &error) {
        return throwError(ctx, error);
    }
#endif
}

JSValue UtilitiesExtension::js_versionCompare(JSContext *ctx, JSValueConst,
                                              int argc, JSValueConst *argv)
{
    try {
        const auto args = getArguments<QString, QString>(ctx, "Utilities.versionCompare",
                                                         argc, argv);
        const auto a = Version::fromString(std::get<0>(args), true);
        const auto b = Version::fromString(std::get<1>(args), true);
        return JS_NewInt32(ctx, compare(a, b));
    } catch (const QString &error) { return throwError(ctx, error); }
}

JSValue UtilitiesExtension::js_qmlTypeInfo(JSContext *ctx, JSValueConst, int, JSValueConst *)
{
    return makeJsString(ctx, QString::fromStdString(qbs::LanguageInfo::qmlTypeInfo()));
}

JSValue UtilitiesExtension::js_builtinExtensionNames(JSContext *ctx, JSValueConst,
                                                     int, JSValueConst *)
{
    return makeJsStringList(ctx, JsExtensions::extensionNames());
}

JSValue UtilitiesExtension::js_isSharedLibrary(JSContext *ctx, JSValueConst,
                                               int argc, JSValueConst *argv)
{
    try {
        const auto fileName = getArgument<QString>(ctx, "Utilities.isSharedLibrary", argc, argv);
        return JS_NewBool(ctx, QLibrary::isLibrary(fileName));
    } catch (const QString &error) { return throwError(ctx, error); }
}

#ifdef __APPLE__
template <typename T = uint32_t> T readInt(QIODevice *ioDevice, bool *ok,
                                           bool swap, bool peek = false) {
    const auto bytes = peek
            ? ioDevice->peek(sizeof(T))
            : ioDevice->read(sizeof(T));
    if (bytes.size() != sizeof(T)) {
        if (ok)
            *ok = false;
        return T();
    }
    if (ok)
        *ok = true;
    T n = *reinterpret_cast<const T *>(bytes.constData());
    return swap ? qbswap(n) : n;
}

static QString archName(cpu_type_t cputype, cpu_subtype_t cpusubtype)
{
    switch (cputype) {
    case CPU_TYPE_X86:
        switch (cpusubtype) {
        case CPU_SUBTYPE_X86_ALL:
            return QStringLiteral("i386");
        default:
            return {};
        }
    case CPU_TYPE_X86_64:
        switch (cpusubtype) {
        case CPU_SUBTYPE_X86_64_ALL:
            return QStringLiteral("x86_64");
        case CPU_SUBTYPE_X86_64_H:
            return QStringLiteral("x86_64h");
        default:
            return {};
        }
    case CPU_TYPE_ARM:
        switch (cpusubtype) {
        case CPU_SUBTYPE_ARM_V7:
            return QStringLiteral("armv7a");
        case CPU_SUBTYPE_ARM_V7S:
            return QStringLiteral("armv7s");
        case CPU_SUBTYPE_ARM_V7K:
            return QStringLiteral("armv7k");
        default:
            return {};
        }
    case CPU_TYPE_ARM64:
        switch (cpusubtype) {
        case CPU_SUBTYPE_ARM64_ALL:
            return QStringLiteral("arm64");
        default:
            return {};
        }
    default:
        return {};
    }
}

static QStringList detectMachOArchs(QIODevice *device)
{
    bool ok;
    bool foundMachO = false;
    qint64 pos = device->pos();

    char ar_header[SARMAG];
    if (device->read(ar_header, SARMAG) == SARMAG) {
        if (strncmp(ar_header, ARMAG, SARMAG) == 0) {
            while (!device->atEnd()) {
                static_assert(sizeof(ar_hdr) == 60, "sizeof(ar_hdr) != 60");
                ar_hdr header;
                if (device->read(reinterpret_cast<char *>(&header),
                                 sizeof(ar_hdr)) != sizeof(ar_hdr))
                    return {};

                // If the file name is stored in the "extended format" manner,
                // the real filename is prepended to the data section, so skip that many bytes
                int filenameLength = 0;
                if (strncmp(header.ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0) {
                    char arName[sizeof(header.ar_name)] = { 0 };
                    memcpy(arName, header.ar_name + sizeof(AR_EFMT1) - 1,
                           sizeof(header.ar_name) - (sizeof(AR_EFMT1) - 1) - 1);
                    filenameLength = strtoul(arName, nullptr, 10);
                    if (device->read(filenameLength).size() != filenameLength)
                        return {};
                }

                switch (readInt(device, nullptr, false, true)) {
                case MH_CIGAM:
                case MH_CIGAM_64:
                case MH_MAGIC:
                case MH_MAGIC_64:
                    foundMachO = true;
                    break;
                default: {
                    // Skip the data and go to the next archive member...
                    char szBuf[sizeof(header.ar_size) + 1] = { 0 };
                    memcpy(szBuf, header.ar_size, sizeof(header.ar_size));
                    int sz = static_cast<int>(strtoul(szBuf, nullptr, 10));
                    if (sz % 2 != 0)
                        ++sz;
                    sz -= filenameLength;
                    const auto data = device->read(sz);
                    if (data.size() != sz)
                        return {};
                }
                }

                if (foundMachO)
                    break;
            }
        }
    }

    // Wasn't an archive file, so try a fat file
    if (!foundMachO && !device->seek(pos))
        return {};

    pos = device->pos();

    fat_header fatheader;
    fatheader.magic = readInt(device, nullptr, false);
    if (fatheader.magic == FAT_MAGIC || fatheader.magic == FAT_CIGAM ||
        fatheader.magic == FAT_MAGIC_64 || fatheader.magic == FAT_CIGAM_64) {
        const bool swap = fatheader.magic == FAT_CIGAM || fatheader.magic == FAT_CIGAM_64;
        const bool is64bit = fatheader.magic == FAT_MAGIC_64 || fatheader.magic == FAT_CIGAM_64;
        fatheader.nfat_arch = readInt(device, &ok, swap);
        if (!ok)
            return {};

        QStringList archs;

        for (uint32_t n = 0; n < fatheader.nfat_arch; ++n) {
            fat_arch_64 fatarch;
            static_assert(sizeof(fat_arch_64) == 32, "sizeof(fat_arch_64) != 32");
            static_assert(sizeof(fat_arch) == 20, "sizeof(fat_arch) != 20");
            const qint64 expectedBytes = is64bit ? sizeof(fat_arch_64) : sizeof(fat_arch);
            if (device->read(reinterpret_cast<char *>(&fatarch), expectedBytes) != expectedBytes)
                return {};

            if (swap) {
                fatarch.cputype = qbswap(fatarch.cputype);
                fatarch.cpusubtype = qbswap(fatarch.cpusubtype);
            }

            const QString name = archName(fatarch.cputype, fatarch.cpusubtype);
            if (name.isEmpty()) {
                qWarning("Unknown cputype %d and cpusubtype %d",
                         fatarch.cputype, fatarch.cpusubtype);
                return {};
            }
            archs.push_back(name);
        }

        std::sort(archs.begin(), archs.end());
        return archs;
    }

    // Wasn't a fat file, so we just read a thin Mach-O from the original offset
    if (!device->seek(pos))
        return {};

    bool swap = false;
    mach_header header;
    header.magic = readInt(device, nullptr, swap);
    switch (header.magic) {
    case MH_CIGAM:
    case MH_CIGAM_64:
        swap = true;
        break;
    case MH_MAGIC:
    case MH_MAGIC_64:
        break;
    default:
        return {};
    }

    header.cputype = static_cast<cpu_type_t>(readInt(device, &ok, swap));
    if (!ok)
        return {};

    header.cpusubtype = static_cast<cpu_subtype_t>(readInt(device, &ok, swap));
    if (!ok)
        return {};

    const QString name = archName(header.cputype, header.cpusubtype);
    if (name.isEmpty()) {
        qWarning("Unknown cputype %d and cpusubtype %d",
                 header.cputype, header.cpusubtype);
        return {};
    }
    return {name};
}
#endif

JSValue UtilitiesExtension::js_getArchitecturesFromBinary(JSContext *ctx, JSValueConst,
                                                          int argc, JSValueConst *argv)
{
    try {
        const auto path = getArgument<QString>(ctx, "Utilities.getArchitecturesFromBinary",
                                               argc, argv);
        QStringList archs;
#ifdef __APPLE__
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            throw Tr::tr("In Utilities.getArchitecturesFromBinary: "
                         "Failed to open file '%1': %2")
                    .arg(file.fileName(), file.errorString());
        }
        archs = detectMachOArchs(&file);
#else
        Q_UNUSED(path)
#endif // __APPLE__
        return makeJsStringList(ctx, archs);
    } catch (const QString &error) { return throwError(ctx, error); }
}

} // namespace Internal
} // namespace qbs

void initializeJsExtensionUtilities(qbs::Internal::ScriptEngine *engine, JSValue extensionObject)
{
    qbs::Internal::UtilitiesExtension::registerClass(engine, extensionObject);
}
