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
#include <tools/jsliterals.h>
#include <tools/profile.h>
#include <tools/settings.h>
#include <tools/version.h>

#include <QtCore/qdir.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qlibrary.h>
#include <QtCore/qregexp.h>
#include <QtCore/qtextstream.h>

#include <queue>
#include <regex>

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
        foreach (const QByteArray &line, lines) {
            const QByteArray simplifiedLine = line.simplified();
            if (simplifiedLine.startsWith(magicString))
                return QString::fromLatin1(simplifiedLine.mid(magicString.count()).trimmed());
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

static QString extractQbsArch(const QtEnvironment &qtEnv)
{
    QString qbsArch = canonicalArchitecture(qtEnv.architecture);
    if (qbsArch == QLatin1String("arm") && qtEnv.mkspecPath.contains(QLatin1String("android")))
        qbsArch = QLatin1String("armv7a");
    return qbsArch;
}

static void replaceSpecialValues(QByteArray *content, const Profile &profile,
        const QtModuleInfo &module, const QtEnvironment &qtEnvironment)
{
    content->replace("@arch@", utf8JSLiteral(extractQbsArch(qtEnvironment)));
    content->replace("@config@", utf8JSLiteral(qtEnvironment.configItems));
    content->replace("@qtConfig@", utf8JSLiteral(qtEnvironment.qtConfigItems));
    content->replace("@binPath@", utf8JSLiteral(qtEnvironment.binaryPath));
    content->replace("@libPath@", utf8JSLiteral(qtEnvironment.libraryPath));
    content->replace("@pluginPath@", utf8JSLiteral(qtEnvironment.pluginPath));
    content->replace("@incPath@", utf8JSLiteral(qtEnvironment.includePath));
    content->replace("@docPath@", utf8JSLiteral(qtEnvironment.documentationPath));
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
    allFiles->append(QFileInfo(targetPath).absoluteFilePath());
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
    foreach (const QtModuleInfo &module, modules) {
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
        } else if (module.qbsName == QLatin1String("phonon")) {
            moduleTemplateFileName = QLatin1String("phonon.qbs");
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
        return QLatin1String("6.2");

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
    if (coreLibFiles.isEmpty())
        throw ErrorInfo(Internal::Tr::tr("Could not determine whether Qt is a static build."));
    foreach (const QString &fileName, coreLibFiles) {
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
        qtEnvironment.configItems.append(QLatin1String("c++11"));

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
            qtEnvironment.macosVersion = QLatin1String("10.6");
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

        if (qtEnvironment.qtConfigItems.contains(QLatin1String("c++11")))
            qtEnvironment.macosVersion = QLatin1String("10.7");
    }

    if (qtEnvironment.mkspecPath.contains(QLatin1String("ios"))
            && qtEnvironment.qtMajorVersion >= 5) {
        qtEnvironment.iosVersion = QLatin1String("5.0");
    }

    if (qtEnvironment.mkspecPath.contains(QLatin1String("android"))) {
        if (qtEnvironment.qtMajorVersion >= 5)
            qtEnvironment.androidVersion = QLatin1String("2.3");
        else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 8)
            qtEnvironment.androidVersion = QLatin1String("1.6"); // Necessitas
    }

    // ### TODO: wince, winphone, blackberry

    Profile profile(profileName, settings);
    profile.removeProfile();
    createModules(profile, settings, qtEnvironment);
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
