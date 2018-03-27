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

#include "qtprofilesetup.h"

#include "qtmoduleinfo.h"

#include <logging/translator.h>
#include <tools/architectures.h>
#include <tools/error.h>
#include <tools/hostosinfo.h>
#include <tools/jsliterals.h>
#include <tools/profile.h>
#include <tools/settings.h>
#include <tools/version.h>

#include <QtCore/qdir.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qendian.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qlibrary.h>
#include <QtCore/qregexp.h>
#include <QtCore/qtextstream.h>

#include <queue>
#include <regex>

#ifdef __APPLE__
#include <ar.h>
#include <mach/machine.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101200
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

namespace qbs {
using namespace Internal;

template <class T>
QByteArray utf8JSLiteral(T t)
{
    return toJSLiteral(t).toUtf8();
}

static QString pathToJSLiteral(const QString &path)
{
    return toJSLiteral(QDir::fromNativeSeparators(path));
}

static QString defaultQpaPlugin(const Profile &profile, const QtModuleInfo &module,
                                const QtEnvironment &qtEnv)
{
    if (qtEnv.qtMajorVersion < 5)
        return QString();

    if (qtEnv.qtMajorVersion == 5 && qtEnv.qtMinorVersion < 8) {
        QFile qConfigPri(qtEnv.mkspecBasePath + QLatin1String("/qconfig.pri"));
        if (!qConfigPri.open(QIODevice::ReadOnly)) {
            throw ErrorInfo(Tr::tr("Setting up Qt profile '%1' failed: Cannot open "
                    "file '%2' (%3).")
                    .arg(profile.name(), qConfigPri.fileName(), qConfigPri.errorString()));
        }
        const QList<QByteArray> lines = qConfigPri.readAll().split('\n');
        const QByteArray magicString = "QT_DEFAULT_QPA_PLUGIN =";
        for (const QByteArray &line : lines) {
            const QByteArray simplifiedLine = line.simplified();
            if (simplifiedLine.startsWith(magicString))
                return QString::fromLatin1(simplifiedLine.mid(magicString.size()).trimmed());
        }
    } else {
        const QString qtGuiConfigHeader = (qtEnv.frameworkBuild
                                           ? qtEnv.libraryPath
                                           : qtEnv.includePath)
                + QStringLiteral("/QtGui")
                + (qtEnv.frameworkBuild ? QStringLiteral(".framework/Headers") : QString())
                + QStringLiteral("/qtgui-config.h");
        std::queue<QString> headerFiles;
        headerFiles.push(qtGuiConfigHeader);
        while (!headerFiles.empty()) {
            QFile headerFile(headerFiles.front());
            headerFiles.pop();
            if (!headerFile.open(QIODevice::ReadOnly)) {
                throw ErrorInfo(Tr::tr("Setting up Qt profile '%1' failed: Cannot open "
                                       "file '%2' (%3).")
                                .arg(profile.name(), headerFile.fileName(),
                                     headerFile.errorString()));
            }
            static const std::regex regexp(
                        "^#define QT_QPA_DEFAULT_PLATFORM_NAME \"(.+)\".*$");
            static const std::regex includeRegexp(
                        "^#include \"(.+)\".*$");
            const QList<QByteArray> lines = headerFile.readAll().split('\n');
            for (const QByteArray &line: lines) {
                const auto lineStr = QString::fromLatin1(line.simplified()).toStdString();
                std::smatch match;
                if (std::regex_match(lineStr, match, regexp))
                    return QLatin1Char('q') + QString::fromStdString(match[1]);
                if (std::regex_match(lineStr, match, includeRegexp)) {
                    QString filePath = QString::fromStdString(match[1]);
                    if (QFileInfo(filePath).isRelative()) {
                        filePath = QDir::cleanPath(QFileInfo(headerFile.fileName()).absolutePath()
                                                   + QLatin1Char('/') + filePath);
                    }
                    headerFiles.push(filePath);
                }
            }
        }
    }

    if (module.isStaticLibrary)
        qDebug("Warning: Could not determine default QPA plugin for static Qt.");
    return QString();
}

static QByteArray minVersionJsString(const QString &minVersion)
{
    if (minVersion.isEmpty())
        return "original";
    return utf8JSLiteral(minVersion);
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
            return QString();
        }
    case CPU_TYPE_X86_64:
        switch (cpusubtype) {
        case CPU_SUBTYPE_X86_64_ALL:
            return QStringLiteral("x86_64");
        case CPU_SUBTYPE_X86_64_H:
            return QStringLiteral("x86_64h");
        default:
            return QString();
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
            return QString();
        }
    case CPU_TYPE_ARM64:
        switch (cpusubtype) {
        case CPU_SUBTYPE_ARM64_ALL:
            return QStringLiteral("arm64");
        default:
            return QString();
        }
    default:
        return QString();
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
                    return {  };

                // If the file name is stored in the "extended format" manner,
                // the real filename is prepended to the data section, so skip that many bytes
                int filenameLength = 0;
                if (strncmp(header.ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0) {
                    char arName[sizeof(header.ar_name)] = { 0 };
                    memcpy(arName, header.ar_name + sizeof(AR_EFMT1) - 1,
                           sizeof(header.ar_name) - (sizeof(AR_EFMT1) - 1) - 1);
                    filenameLength = strtoul(arName, nullptr, 10);
                    if (device->read(filenameLength).size() != filenameLength)
                        return { };
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
                        return { };
                }
                }

                if (foundMachO)
                    break;
            }
        }
    }

    // Wasn't an archive file, so try a fat file
    if (!foundMachO && !device->seek(pos))
        return QStringList();

    pos = device->pos();

    fat_header fatheader;
    fatheader.magic = readInt(device, nullptr, false);
    if (fatheader.magic == FAT_MAGIC || fatheader.magic == FAT_CIGAM ||
        fatheader.magic == FAT_MAGIC_64 || fatheader.magic == FAT_CIGAM_64) {
        const bool swap = fatheader.magic == FAT_CIGAM || fatheader.magic == FAT_CIGAM_64;
        const bool is64bit = fatheader.magic == FAT_MAGIC_64 || fatheader.magic == FAT_CIGAM_64;
        fatheader.nfat_arch = readInt(device, &ok, swap);
        if (!ok)
            return QStringList();

        QStringList archs;

        for (uint32_t n = 0; n < fatheader.nfat_arch; ++n) {
            fat_arch_64 fatarch;
            static_assert(sizeof(fat_arch_64) == 32, "sizeof(fat_arch_64) != 32");
            static_assert(sizeof(fat_arch) == 20, "sizeof(fat_arch) != 20");
            const qint64 expectedBytes = is64bit ? sizeof(fat_arch_64) : sizeof(fat_arch);
            if (device->read(reinterpret_cast<char *>(&fatarch), expectedBytes) != expectedBytes)
                return QStringList();

            if (swap) {
                fatarch.cputype = qbswap(fatarch.cputype);
                fatarch.cpusubtype = qbswap(fatarch.cpusubtype);
            }

            const QString name = archName(fatarch.cputype, fatarch.cpusubtype);
            if (name.isEmpty()) {
                qWarning("Unknown cputype %d and cpusubtype %d",
                         fatarch.cputype, fatarch.cpusubtype);
                return QStringList();
            }
            archs.push_back(name);
        }

        std::sort(archs.begin(), archs.end());
        return archs;
    }

    // Wasn't a fat file, so we just read a thin Mach-O from the original offset
    if (!device->seek(pos))
        return QStringList();

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
        return QStringList();
    }

    header.cputype = static_cast<cpu_type_t>(readInt(device, &ok, swap));
    if (!ok)
        return QStringList();

    header.cpusubtype = static_cast<cpu_subtype_t>(readInt(device, &ok, swap));
    if (!ok)
        return QStringList();

    const QString name = archName(header.cputype, header.cpusubtype);
    if (name.isEmpty()) {
        qWarning("Unknown cputype %d and cpusubtype %d",
                 header.cputype, header.cpusubtype);
        return { };
    }
    return { name };
}
#endif

static QStringList extractQbsArchs(const QtModuleInfo &module, const QtEnvironment &qtEnv)
{
#ifdef __APPLE__
    if (qtEnv.mkspecName.startsWith(QLatin1String("macx-"))) {
        QStringList archs;
        if (!module.libFilePathRelease.isEmpty()) {
            QFile file(module.libFilePathRelease);
            if (file.open(QIODevice::ReadOnly))
                archs = detectMachOArchs(&file);
            else
                qWarning("Failed to open %s for reading",
                         qUtf8Printable(module.libFilePathRelease));
            if (archs.isEmpty())
                qWarning("Could not detect architectures for module %s in Qt build (%s)",
                         qPrintable(module.name), qUtf8Printable(qtEnv.mkspecName));
        }
        return archs;
    }
#else
    Q_UNUSED(module);
#endif
    QString qbsArch = canonicalArchitecture(qtEnv.architecture);
    if (qbsArch == QLatin1String("arm") && qtEnv.mkspecPath.contains(QLatin1String("android")))
        qbsArch = QLatin1String("armv7a");

    // Qt4 has "QT_ARCH = windows" in qconfig.pri for both MSVC and mingw.
    if (qbsArch == QLatin1String("windows"))
        return QStringList();

    return QStringList { qbsArch };
}

static void replaceSpecialValues(QByteArray *content, const Profile &profile,
        const QtModuleInfo &module, const QtEnvironment &qtEnvironment)
{
    content->replace("@archs@", utf8JSLiteral(extractQbsArchs(module, qtEnvironment)));
    content->replace("@targetPlatform@", utf8JSLiteral(qbsTargetPlatformFromQtMkspec(
                                                           qtEnvironment.mkspecName)));
    content->replace("@config@", utf8JSLiteral(qtEnvironment.configItems));
    content->replace("@qtConfig@", utf8JSLiteral(qtEnvironment.qtConfigItems));
    content->replace("@binPath@", utf8JSLiteral(qtEnvironment.binaryPath));
    content->replace("@libPath@", utf8JSLiteral(qtEnvironment.libraryPath));
    content->replace("@pluginPath@", utf8JSLiteral(qtEnvironment.pluginPath));
    content->replace("@incPath@", utf8JSLiteral(qtEnvironment.includePath));
    content->replace("@docPath@", utf8JSLiteral(qtEnvironment.documentationPath));
    content->replace("@mkspecName@", utf8JSLiteral(qtEnvironment.mkspecName));
    content->replace("@mkspecPath@", utf8JSLiteral(qtEnvironment.mkspecPath));
    content->replace("@version@", utf8JSLiteral(qtEnvironment.qtVersion));
    content->replace("@libInfix@", utf8JSLiteral(qtEnvironment.qtLibInfix));
    content->replace("@availableBuildVariants@", utf8JSLiteral(qtEnvironment.buildVariant));
    content->replace("@staticBuild@", utf8JSLiteral(qtEnvironment.staticBuild));
    content->replace("@frameworkBuild@", utf8JSLiteral(qtEnvironment.frameworkBuild));
    content->replace("@name@", utf8JSLiteral(module.moduleNameWithoutPrefix()));
    content->replace("@has_library@", utf8JSLiteral(module.hasLibrary));
    content->replace("@dependencies@", utf8JSLiteral(module.dependencies));
    content->replace("@includes@", utf8JSLiteral(module.includePaths));
    content->replace("@staticLibsDebug@", utf8JSLiteral(module.staticLibrariesDebug));
    content->replace("@staticLibsRelease@", utf8JSLiteral(module.staticLibrariesRelease));
    content->replace("@dynamicLibsDebug@", utf8JSLiteral(module.dynamicLibrariesDebug));
    content->replace("@dynamicLibsRelease@", utf8JSLiteral(module.dynamicLibrariesRelease));
    content->replace("@linkerFlagsDebug@", utf8JSLiteral(module.linkerFlagsDebug));
    content->replace("@linkerFlagsRelease@", utf8JSLiteral(module.linkerFlagsRelease));
    content->replace("@libraryPaths@", utf8JSLiteral(module.libraryPaths));
    content->replace("@frameworkPathsDebug@", utf8JSLiteral(module.frameworkPathsDebug));
    content->replace("@frameworkPathsRelease@", utf8JSLiteral(module.frameworkPathsRelease));
    content->replace("@frameworksDebug@", utf8JSLiteral(module.frameworksDebug));
    content->replace("@frameworksRelease@", utf8JSLiteral(module.frameworksRelease));
    content->replace("@libFilePathDebug@", utf8JSLiteral(module.libFilePathDebug));
    content->replace("@libFilePathRelease@", utf8JSLiteral(module.libFilePathRelease));
    content->replace("@libNameForLinkerDebug@",
                    utf8JSLiteral(module.libNameForLinker(qtEnvironment, true)));
    content->replace("@libNameForLinkerRelease@",
                    utf8JSLiteral(module.libNameForLinker(qtEnvironment, false)));
    content->replace("@entryPointLibsDebug@", utf8JSLiteral(qtEnvironment.entryPointLibsDebug));
    content->replace("@entryPointLibsRelease@", utf8JSLiteral(qtEnvironment.entryPointLibsRelease));
    content->replace("@minWinVersion@", minVersionJsString(qtEnvironment.windowsVersion));
    content->replace("@minMacVersion@", minVersionJsString(qtEnvironment.macosVersion));
    content->replace("@minIosVersion@", minVersionJsString(qtEnvironment.iosVersion));
    content->replace("@minTvosVersion@", minVersionJsString(qtEnvironment.tvosVersion));
    content->replace("@minWatchosVersion@", minVersionJsString(qtEnvironment.watchosVersion));
    content->replace("@minAndroidVersion@", minVersionJsString(qtEnvironment.androidVersion));

    QByteArray propertiesString;
    QByteArray compilerDefines = utf8JSLiteral(module.compilerDefines);
    if (module.qbsName == QLatin1String("declarative")
            || module.qbsName == QLatin1String("quick")) {
        const QByteArray debugMacro = module.qbsName == QLatin1String("declarative")
                    || qtEnvironment.qtMajorVersion < 5
                ? "QT_DECLARATIVE_DEBUG" : "QT_QML_DEBUG";

        const QString indent = QLatin1String("    ");
        QTextStream s(&propertiesString);
        s << "property bool qmlDebugging: false" << endl;
        s << indent << "property string qmlPath";
        if (qtEnvironment.qmlPath.isEmpty())
            s << endl;
        else
            s << ": " << pathToJSLiteral(qtEnvironment.qmlPath) << endl;

        s << indent << "property string qmlImportsPath: "
                << pathToJSLiteral(qtEnvironment.qmlImportPath);

        const QByteArray baIndent(4, ' ');
        compilerDefines = "{\n"
                + baIndent + baIndent + "var result = " + compilerDefines + ";\n"
                + baIndent + baIndent + "if (qmlDebugging)\n"
                + baIndent + baIndent + baIndent + "result.push(\"" + debugMacro + "\");\n"
                + baIndent + baIndent + "return result;\n"
                + baIndent + "}";
    }
    content->replace("@defines@", compilerDefines);
    if (module.qbsName == QLatin1String("gui")) {
        content->replace("@defaultQpaPlugin@",
                        utf8JSLiteral(defaultQpaPlugin(profile, module, qtEnvironment)));
    }
    if (module.qbsName == QLatin1String("qml"))
        content->replace("@qmlPath@", pathToJSLiteral(qtEnvironment.qmlPath).toUtf8());
    if (module.isStaticLibrary) {
        if (!propertiesString.isEmpty())
            propertiesString += "\n    ";
        propertiesString += "isStaticLibrary: true";
    }
    if (module.isPlugin)
        content->replace("@className@", utf8JSLiteral(module.pluginData.className));
    content->replace("@special_properties@", propertiesString);
}

static void copyTemplateFile(const QString &fileName, const QString &targetDirectory,
        const Profile &profile, const QtEnvironment &qtEnv, QStringList *allFiles,
        const QtModuleInfo *module = 0)
{
    if (!QDir::root().mkpath(targetDirectory)) {
        throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: "
                                         "Cannot create directory '%2'.")
                        .arg(profile.name(), targetDirectory));
    }
    QFile sourceFile(QLatin1String(":/templates/") + fileName);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: "
                "Cannot open '%1' (%2).").arg(sourceFile.fileName(), sourceFile.errorString()));
    }
    QByteArray newContent = sourceFile.readAll();
    if (module)
        replaceSpecialValues(&newContent, profile, *module, qtEnv);
    sourceFile.close();
    const QString targetPath = targetDirectory + QLatin1Char('/') + fileName;
    allFiles->push_back(QFileInfo(targetPath).absoluteFilePath());
    QFile targetFile(targetPath);
    if (targetFile.open(QIODevice::ReadOnly)) {
        if (newContent == targetFile.readAll()) // No need to overwrite anything in this case.
            return;
        targetFile.close();
    }
    if (!targetFile.open(QIODevice::WriteOnly)) {
        throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: "
                "Cannot open '%1' (%2).").arg(targetFile.fileName(), targetFile.errorString()));
    }
    targetFile.resize(0);
    targetFile.write(newContent);
}

static void createModules(Profile &profile, Settings *settings,
                               const QtEnvironment &qtEnvironment)
{
    const QList<QtModuleInfo> modules = qtEnvironment.qtMajorVersion < 5
            ? allQt4Modules(qtEnvironment)
            : allQt5Modules(profile, qtEnvironment);
    const QString profileBaseDir = QString::fromLatin1("%1/profiles/%2")
            .arg(QFileInfo(settings->fileName()).dir().absolutePath(), profile.name());
    const QString qbsQtModuleBaseDir = profileBaseDir + QLatin1String("/modules/Qt");
    QStringList allFiles;
    copyTemplateFile(QLatin1String("QtModule.qbs"), qbsQtModuleBaseDir, profile, qtEnvironment,
                     &allFiles);
    copyTemplateFile(QLatin1String("QtPlugin.qbs"), qbsQtModuleBaseDir, profile, qtEnvironment,
                     &allFiles);
    for (const QtModuleInfo &module : modules) {
        const QString qbsQtModuleDir = qbsQtModuleBaseDir + QLatin1Char('/') + module.qbsName;
        QString moduleTemplateFileName;
        if (module.qbsName == QLatin1String("core")) {
            moduleTemplateFileName = QLatin1String("core.qbs");
            copyTemplateFile(QLatin1String("moc.js"), qbsQtModuleDir, profile, qtEnvironment,
                             &allFiles);
            copyTemplateFile(QLatin1String("qdoc.js"), qbsQtModuleDir, profile, qtEnvironment,
                             &allFiles);
        } else if (module.qbsName == QLatin1String("gui")) {
            moduleTemplateFileName = QLatin1String("gui.qbs");
        } else if (module.qbsName == QLatin1String("scxml")) {
            moduleTemplateFileName = QLatin1String("scxml.qbs");
        } else if (module.qbsName == QLatin1String("dbus")) {
            moduleTemplateFileName = QLatin1String("dbus.qbs");
            copyTemplateFile(QLatin1String("dbus.js"), qbsQtModuleDir, profile, qtEnvironment,
                             &allFiles);
        } else if (module.qbsName == QLatin1String("qml")) {
            moduleTemplateFileName = QLatin1String("qml.qbs");
            copyTemplateFile(QLatin1String("qml.js"), qbsQtModuleDir, profile, qtEnvironment,
                             &allFiles);
            const QString qmlcacheStr = QStringLiteral("qmlcache");
            if (QFileInfo::exists(HostOsInfo::appendExecutableSuffix(
                                      qtEnvironment.binaryPath + QStringLiteral("/qmlcachegen")))) {
                copyTemplateFile(qmlcacheStr + QStringLiteral(".qbs"),
                                 qbsQtModuleBaseDir + QLatin1Char('/') + qmlcacheStr, profile,
                                 qtEnvironment, &allFiles);
            }
        } else if (module.qbsName == QLatin1String("quick")) {
            moduleTemplateFileName = QLatin1String("quick.qbs");
            copyTemplateFile(QLatin1String("quick.js"), qbsQtModuleDir, profile,
                             qtEnvironment, &allFiles);
        } else if (module.isPlugin) {
            moduleTemplateFileName = QLatin1String("plugin.qbs");
        } else {
            moduleTemplateFileName = QLatin1String("module.qbs");
        }
        copyTemplateFile(moduleTemplateFileName, qbsQtModuleDir, profile, qtEnvironment, &allFiles,
                         &module);
    }
    QDirIterator dit(qbsQtModuleBaseDir, QDirIterator::Subdirectories);
    while (dit.hasNext()) {
        dit.next();
        const QFileInfo &fi = dit.fileInfo();
        if (!fi.isFile())
            continue;
        const QString filePath = fi.absoluteFilePath();
        if (!allFiles.contains(filePath) && !QFile::remove(filePath))
                qDebug("Warning: Failed to remove outdated file '%s'.", qPrintable(filePath));
    }
    profile.setValue(QLatin1String("preferences.qbsSearchPaths"), profileBaseDir);
}

static QString guessMinimumWindowsVersion(const QtEnvironment &qt)
{
    if (qt.mkspecName.startsWith(QLatin1String("winrt-")))
        return QLatin1String("10.0");

    if (!qt.mkspecName.startsWith(QLatin1String("win32-")))
        return QString();

    if (qt.architecture == QLatin1String("x86_64")
            || qt.architecture == QLatin1String("ia64")) {
        return QLatin1String("5.2");
    }

    QRegExp rex(QLatin1String("^win32-msvc(\\d+)$"));
    if (rex.exactMatch(qt.mkspecName)) {
        int msvcVersion = rex.cap(1).toInt();
        if (msvcVersion < 2012)
            return QLatin1String("5.0");
        else
            return QLatin1String("5.1");
    }

    return qt.qtMajorVersion < 5 ? QLatin1String("5.0") : QLatin1String("5.1");
}

static bool checkForStaticBuild(const QtEnvironment &qt)
{
    if (qt.qtMajorVersion >= 5)
        return qt.qtConfigItems.contains(QLatin1String("static"));
    if (qt.frameworkBuild)
        return false; // there are no Qt4 static frameworks
    const bool isWin = qt.mkspecName.startsWith(QLatin1String("win"));
    const QDir libdir(isWin ? qt.binaryPath : qt.libraryPath);
    const QStringList coreLibFiles
            = libdir.entryList(QStringList(QLatin1String("*Core*")), QDir::Files);
    if (coreLibFiles.empty())
        throw ErrorInfo(Internal::Tr::tr("Could not determine whether Qt is a static build."));
    for (const QString &fileName : coreLibFiles) {
        if (QLibrary::isLibrary(fileName))
            return false;
    }
    return true;
}

static QStringList fillEntryPointLibs(const QtEnvironment &qtEnvironment, const Version &qtVersion,
        bool debug)
{
    QStringList result;
    QString qtmain = qtEnvironment.libraryPath + QLatin1Char('/');
    const bool isMinGW = qtEnvironment.mkspecName.startsWith(QLatin1String("win32-g++"));
    if (isMinGW)
        qtmain += QLatin1String("lib");
    qtmain += QLatin1String("qtmain") + qtEnvironment.qtLibInfix;
    if (debug)
        qtmain += QLatin1Char('d');
    if (isMinGW) {
        qtmain += QLatin1String(".a");
    } else {
        qtmain += QLatin1String(".lib");
        if (qtVersion >= Version(5, 4, 0))
            result << QLatin1String("Shell32.lib");
    }
    result << qtmain;
    return result;
}

void doSetupQtProfile(const QString &profileName, Settings *settings,
                      const QtEnvironment &_qtEnvironment)
{
    QtEnvironment qtEnvironment = _qtEnvironment;
    qtEnvironment.staticBuild = checkForStaticBuild(qtEnvironment);

    // determine whether user apps require C++11
    if (qtEnvironment.qtConfigItems.contains(QLatin1String("c++11")) && qtEnvironment.staticBuild)
        qtEnvironment.configItems.push_back(QLatin1String("c++11"));

    // Set the minimum operating system versions appropriate for this Qt version
    qtEnvironment.windowsVersion = guessMinimumWindowsVersion(qtEnvironment);
    if (!qtEnvironment.windowsVersion.isEmpty()) {    // Is target OS Windows?
        const Version qtVersion = Version(qtEnvironment.qtMajorVersion,
                                          qtEnvironment.qtMinorVersion,
                                          qtEnvironment.qtPatchVersion);
        qtEnvironment.entryPointLibsDebug = fillEntryPointLibs(qtEnvironment, qtVersion, true);
        qtEnvironment.entryPointLibsRelease = fillEntryPointLibs(qtEnvironment, qtVersion, false);
    } else if (qtEnvironment.mkspecPath.contains(QLatin1String("macx"))) {
        if (qtEnvironment.qtMajorVersion >= 5) {
            QFile qmakeConf(qtEnvironment.mkspecPath + QStringLiteral("/qmake.conf"));
            if (qmakeConf.open(QIODevice::ReadOnly)) {
                static const std::regex re(
                            "^QMAKE_(MACOSX|IOS|TVOS|WATCHOS)_DEPLOYMENT_TARGET\\s*=\\s*(.*)\\s*$");
                while (!qmakeConf.atEnd()) {
                    std::smatch match;
                    const auto line = qmakeConf.readLine().trimmed().toStdString();
                    if (std::regex_match(line, match, re)) {
                        const auto platform = QString::fromStdString(match[1]);
                        const auto version = QString::fromStdString(match[2]);
                        if (platform == QStringLiteral("MACOSX"))
                            qtEnvironment.macosVersion = version;
                        else if (platform == QStringLiteral("IOS"))
                            qtEnvironment.iosVersion = version;
                        else if (platform == QStringLiteral("TVOS"))
                            qtEnvironment.tvosVersion = version;
                        else if (platform == QStringLiteral("WATCHOS"))
                            qtEnvironment.watchosVersion = version;
                    }
                }
            }

            const bool isMac = qtEnvironment.mkspecName != QStringLiteral("macx-ios-clang")
                    && qtEnvironment.mkspecName != QStringLiteral("macx-tvos-clang")
                    && qtEnvironment.mkspecName != QStringLiteral("macx-watchos-clang");
            if (isMac) {
                // Qt 5.0.x placed the minimum version in a different file
                if (qtEnvironment.macosVersion.isEmpty()) {
                    qtEnvironment.macosVersion = QLatin1String("10.6");
                }

                // If we're using C++11 with libc++, make sure the deployment target is >= 10.7
                if (Version::fromString(qtEnvironment.macosVersion) < Version(10, 7)
                        && qtEnvironment.qtConfigItems.contains(QLatin1String("c++11")))
                    qtEnvironment.macosVersion = QLatin1String("10.7");
            }
        } else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 6) {
            QDir qconfigDir;
            if (qtEnvironment.frameworkBuild) {
                qconfigDir.setPath(qtEnvironment.libraryPath);
                qconfigDir.cd(QLatin1String("QtCore.framework/Headers"));
            } else {
                qconfigDir.setPath(qtEnvironment.includePath);
                qconfigDir.cd(QLatin1String("Qt"));
            }
            QFile qconfig(qconfigDir.absoluteFilePath(QLatin1String("qconfig.h")));
            if (qconfig.open(QIODevice::ReadOnly)) {
                bool qtCocoaBuild = false;
                QTextStream ts(&qconfig);
                QString line;
                do {
                    line = ts.readLine();
                    if (QRegExp(QLatin1String("\\s*#define\\s+QT_MAC_USE_COCOA\\s+1\\s*"),
                                Qt::CaseSensitive).exactMatch(line)) {
                        qtCocoaBuild = true;
                        break;
                    }
                } while (!line.isNull());

                if (ts.status() == QTextStream::Ok) {
                    qtEnvironment.macosVersion = qtCocoaBuild ? QLatin1String("10.5")
                                                              : QLatin1String("10.4");
                }
            }

            if (qtEnvironment.macosVersion.isEmpty()) {
                throw ErrorInfo(Internal::Tr::tr("Error reading qconfig.h; could not determine "
                                                 "whether Qt is using Cocoa or Carbon"));
            }
        }
    } else if (qtEnvironment.mkspecPath.contains(QLatin1String("android"))) {
        if (qtEnvironment.qtMajorVersion >= 5)
            qtEnvironment.androidVersion = QLatin1String("2.3");
        else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 8)
            qtEnvironment.androidVersion = QLatin1String("1.6"); // Necessitas
    }

    Profile profile(profileName, settings);
    profile.removeProfile();
    createModules(profile, settings, qtEnvironment);
}

QString qbsTargetPlatformFromQtMkspec(const QString &mkspec)
{
    int idx = mkspec.lastIndexOf(QLatin1Char('/'));
    if (idx != -1)
        return qbsTargetPlatformFromQtMkspec(mkspec.mid(idx + 1));
    if (mkspec.startsWith(QLatin1String("aix-")))
        return QLatin1String("aix");
    if (mkspec.startsWith(QLatin1String("android-")))
        return QLatin1String("android");
    if (mkspec.startsWith(QLatin1String("cygwin-")))
        return QLatin1String("windows");
    if (mkspec.startsWith(QLatin1String("darwin-")))
        return QLatin1String("macos");
    if (mkspec.startsWith(QLatin1String("freebsd-")))
        return QLatin1String("freebsd");
    if (mkspec.startsWith(QLatin1String("haiku-")))
        return QLatin1String("haiku");
    if (mkspec.startsWith(QLatin1String("hpux-")) || mkspec.startsWith(QLatin1String("hpuxi-")))
        return QLatin1String("hpux");
    if (mkspec.startsWith(QLatin1String("hurd-")))
        return QLatin1String("hurd");
    if (mkspec.startsWith(QLatin1String("integrity-")))
        return QLatin1String("integrity");
    if (mkspec.startsWith(QLatin1String("linux-")))
        return QLatin1String("linux");
    if (mkspec.startsWith(QLatin1String("macx-"))) {
        if (mkspec.startsWith(QLatin1String("macx-ios-")))
            return QLatin1String("ios");
        if (mkspec.startsWith(QLatin1String("macx-tvos-")))
            return QLatin1String("tvos");
        if (mkspec.startsWith(QLatin1String("macx-watchos-")))
            return QLatin1String("watchos");
        return QLatin1String("macos");
    }
    if (mkspec.startsWith(QLatin1String("netbsd-")))
        return QLatin1String("netbsd");
    if (mkspec.startsWith(QLatin1String("openbsd-")))
        return QLatin1String("openbsd");
    if (mkspec.startsWith(QLatin1String("qnx-")))
        return QLatin1String("qnx");
    if (mkspec.startsWith(QLatin1String("solaris-")))
        return QLatin1String("solaris");
    if (mkspec.startsWith(QLatin1String("vxworks-")))
        return QLatin1String("vxworks");
    if (mkspec.startsWith(QLatin1String("win32-")) || mkspec.startsWith(QLatin1String("winrt-")))
        return QLatin1String("windows");
    return QString();
}

ErrorInfo setupQtProfile(const QString &profileName, Settings *settings,
                         const QtEnvironment &qtEnvironment)
{
    try {
        doSetupQtProfile(profileName, settings, qtEnvironment);
        return ErrorInfo();
    } catch (const ErrorInfo &e) {
        return e;
    }
}

} // namespace qbs
