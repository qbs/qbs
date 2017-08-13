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
#include "qtmoduleinfo.h"

#include "qtenvironment.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/profile.h>
#include <tools/set.h>

#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qhash.h>

namespace qbs {
namespace Internal {

typedef QHash<QString, QString> NamePathHash;
static void replaceQtLibNamesWithFilePath(const NamePathHash &namePathHash, QStringList *libList)
{
    for (int i = 0; i < libList->count(); ++i) {
        QString &lib = (*libList)[i];
        const NamePathHash::ConstIterator it = namePathHash.find(lib);
        if (it != namePathHash.constEnd())
            lib = it.value();
    }
}

static void replaceQtLibNamesWithFilePath(QList<QtModuleInfo> *modules, const QtEnvironment &qtEnv)
{
    // We don't want to add the libraries for Qt modules via "-l", because of the
    // danger that a wrong one will be picked up, e.g. from /usr/lib. Instead,
    // we pull them in using the full file path.
    typedef QHash<QString, QString> NamePathHash;
    NamePathHash linkerNamesToFilePathsDebug;
    NamePathHash linkerNamesToFilePathsRelease;
    foreach (const QtModuleInfo &m, *modules) {
        linkerNamesToFilePathsDebug.insert(m.libNameForLinker(qtEnv, true), m.libFilePathDebug);
        linkerNamesToFilePathsRelease.insert(m.libNameForLinker(qtEnv, false),
                                             m.libFilePathRelease);
    }
    for (int i = 0; i < modules->count(); ++i) {
        QtModuleInfo &module = (*modules)[i];
        replaceQtLibNamesWithFilePath(linkerNamesToFilePathsDebug, &module.dynamicLibrariesDebug);
        replaceQtLibNamesWithFilePath(linkerNamesToFilePathsDebug, &module.staticLibrariesDebug);
        replaceQtLibNamesWithFilePath(linkerNamesToFilePathsRelease,
                                      &module.dynamicLibrariesRelease);
        replaceQtLibNamesWithFilePath(linkerNamesToFilePathsRelease,
                                      &module.staticLibrariesRelease);
    }
}


QtModuleInfo::QtModuleInfo()
    : isPrivate(false), hasLibrary(true), isStaticLibrary(false), isPlugin(false), mustExist(true)
{
}

QtModuleInfo::QtModuleInfo(const QString &name, const QString &qbsName, const QStringList &deps)
    : name(name), qbsName(qbsName), dependencies(deps),
      isPrivate(qbsName.endsWith(QLatin1String("-private"))),
      hasLibrary(!isPrivate),
      isStaticLibrary(false),
      isPlugin(false),
      mustExist(true)
{
    const QString coreModule = QLatin1String("core");
    if (qbsName != coreModule && !dependencies.contains(coreModule))
        dependencies.prepend(coreModule);
}

QString QtModuleInfo::moduleNameWithoutPrefix() const
{
    if (modulePrefix.isEmpty() && name.startsWith(QLatin1String("Qt")))
        return name.mid(2); // Strip off "Qt".
    if (name.startsWith(modulePrefix))
        return name.mid(modulePrefix.length());
    return name;
}

QString QtModuleInfo::frameworkHeadersPath(const QtEnvironment &qtEnvironment) const
{
    return qtEnvironment.libraryPath + QLatin1Char('/') + name
            + QLatin1String(".framework/Headers");
}

QStringList QtModuleInfo::qt4ModuleIncludePaths(const QtEnvironment &qtEnvironment) const
{
    QStringList paths;
    if (isFramework(qtEnvironment)) {
        paths << frameworkHeadersPath(qtEnvironment);
    } else {
        paths << qtEnvironment.includePath
              << qtEnvironment.includePath + QLatin1Char('/') + name;
    }
    return paths;
}

QString QtModuleInfo::libraryBaseName(const QtEnvironment &qtEnvironment,
                                           bool debugBuild) const
{
    if (isPlugin)
        return libBaseName(name, debugBuild, qtEnvironment);

    // Some modules use a different naming scheme, so it doesn't get boring.
    const bool libNameBroken = name == QLatin1String("Enginio")
            || name == QLatin1String("DataVisualization");

    QString libName = modulePrefix.isEmpty() && !libNameBroken ? QLatin1String("Qt") : modulePrefix;
    if (qtEnvironment.qtMajorVersion >= 5 && !isFramework(qtEnvironment) && !libNameBroken)
        libName += QString::number(qtEnvironment.qtMajorVersion);
    libName += moduleNameWithoutPrefix();
    libName += qtEnvironment.qtLibInfix;
    return libBaseName(libName, debugBuild, qtEnvironment);
}

QString QtModuleInfo::libNameForLinker(const QtEnvironment &qtEnvironment, bool debugBuild) const
{
    if (!hasLibrary)
        return QString();
    QString libName = libraryBaseName(qtEnvironment, debugBuild);
    if (qtEnvironment.mkspecName.contains(QLatin1String("msvc")))
        libName += QLatin1String(".lib");
    return libName;
}

void QtModuleInfo::setupLibraries(const QtEnvironment &qtEnv,
                                  Internal::Set<QString> *nonExistingPrlFiles)
{
    setupLibraries(qtEnv, true, nonExistingPrlFiles);
    setupLibraries(qtEnv, false, nonExistingPrlFiles);
}

static QStringList makeList(const QByteArray &s)
{
    return QString::fromLatin1(s).split(QLatin1Char(' '), QString::SkipEmptyParts);
}

void QtModuleInfo::setupLibraries(const QtEnvironment &qtEnv, bool debugBuild,
                                  Internal::Set<QString> *nonExistingPrlFiles)
{
    if (!hasLibrary)
        return; // Can happen for Qt4 convenience modules, like "widgets".

    if (debugBuild) {
        if (!qtEnv.buildVariant.contains(QLatin1String("debug")))
            return;
        const QStringList modulesNeverBuiltAsDebug = QStringList()
                << QLatin1String("bootstrap") << QLatin1String("qmldevtools");
        foreach (const QString &m, modulesNeverBuiltAsDebug) {
            if (qbsName == m || qbsName == m + QLatin1String("-private"))
                return;
        }
    } else if (!qtEnv.buildVariant.contains(QLatin1String("release"))) {
        return;
    }

    QStringList &libs = isStaticLibrary
            ? (debugBuild ? staticLibrariesDebug : staticLibrariesRelease)
            : (debugBuild ? dynamicLibrariesDebug : dynamicLibrariesRelease);
    QStringList &frameworks = debugBuild ? frameworksDebug : frameworksRelease;
    QStringList &frameworkPaths = debugBuild ? frameworkPathsDebug : frameworkPathsRelease;
    QStringList &flags = debugBuild ? linkerFlagsDebug : linkerFlagsRelease;
    QString &libFilePath = debugBuild ? libFilePathDebug : libFilePathRelease;

    if (qtEnv.mkspecName.contains(QLatin1String("ios")) && isStaticLibrary) {
        libs << QLatin1String("z") << QLatin1String("m");
        if (qtEnv.qtMajorVersion == 5 && qtEnv.qtMinorVersion < 8) {
            const QtModuleInfo platformSupportModule(QLatin1String("QtPlatformSupport"),
                                                     QLatin1String("platformsupport"));
            libs << platformSupportModule.libNameForLinker(qtEnv, debugBuild);
        }
        if (name == QStringLiteral("qios")) {
            flags << QLatin1String("-force_load")
                  << qtEnv.pluginPath + QLatin1String("/platforms/")
                     + libBaseName(QLatin1String("libqios"), debugBuild, qtEnv)
                     + QLatin1String(".a");
        }
    }

    QString prlFilePath = isPlugin
            ? qtEnv.pluginPath + QLatin1Char('/') + pluginData.type
            : qtEnv.libraryPath;
    prlFilePath += QLatin1Char('/');
    if (isFramework(qtEnv))
        prlFilePath.append(libraryBaseName(qtEnv, false)).append(QLatin1String(".framework/"));
    const QString libDir = prlFilePath;
    if (!qtEnv.mkspecName.startsWith(QLatin1String("win")) && !isFramework(qtEnv))
        prlFilePath += QLatin1String("lib");
    prlFilePath.append(libraryBaseName(qtEnv, debugBuild));
    const bool isNonStaticQt4OnWindows = qtEnv.mkspecName.startsWith(QLatin1String("win"))
            && !isStaticLibrary && qtEnv.qtMajorVersion < 5;
    if (isNonStaticQt4OnWindows)
        prlFilePath.chop(1); // The prl file base name does *not* contain the version number...
    prlFilePath.append(QLatin1String(".prl"));
    if (nonExistingPrlFiles->contains(prlFilePath))
        return;
    QFile prlFile(prlFilePath);
    if (!prlFile.open(QIODevice::ReadOnly)) {
        // We can't error out here, as some modules in a self-built Qt don't have the expected
        // file names. Real-life example: "libQt0Feedback.prl". This is just too stupid
        // to work around, so let's ignore it.
        if (mustExist) {
            qDebug("Skipping prl file '%s', because it cannot be opened (%s).",
                   qPrintable(prlFilePath),
                   qPrintable(prlFile.errorString()));
        }
        nonExistingPrlFiles->insert(prlFilePath);
        return;
    }
    const QList<QByteArray> prlLines = prlFile.readAll().split('\n');
    foreach (const QByteArray &line, prlLines) {
        const QByteArray simplifiedLine = line.simplified();
        const int equalsOffset = simplifiedLine.indexOf('=');
        if (equalsOffset == -1)
            continue;
        if (simplifiedLine.startsWith("QMAKE_PRL_TARGET")) {
            const bool isMingw = qtEnv.mkspecName.startsWith(QLatin1String("win"))
                    && qtEnv.mkspecName.contains(QLatin1String("g++"));
            const bool isQtVersionBefore56 = qtEnv.qtMajorVersion < 5
                    || (qtEnv.qtMajorVersion == 5 && qtEnv.qtMinorVersion < 6);
            libFilePath = libDir;

            // QMAKE_PRL_TARGET has a "lib" prefix, except for mingw.
            // Of course, the exception has an exception too: For static libs, mingw *does*
            // have the "lib" prefix. TODO: Shoot the people responsible for this.
            if (isQtVersionBefore56 && isMingw && !isStaticLibrary)
                libFilePath += QLatin1String("lib");

            libFilePath += QString::fromLatin1(simplifiedLine.mid(equalsOffset + 1).trimmed());
            if (isNonStaticQt4OnWindows)
                libFilePath += QString::number(4); // This is *not* part of QMAKE_PRL_TARGET...
            if (isQtVersionBefore56) {
                if (qtEnv.mkspecName.contains(QLatin1String("msvc")))
                    libFilePath += QLatin1String(".lib");
                else if (isMingw)
                    libFilePath += QLatin1String(".a");
            }
            continue;
        }
        if (!simplifiedLine.startsWith("QMAKE_PRL_LIBS"))
            continue;

        // Assuming lib names and directories without spaces here.
        QStringList parts = QString::fromLatin1(simplifiedLine.mid(equalsOffset + 1).trimmed())
                .split(QLatin1Char(' '), QString::SkipEmptyParts);
        for (int i = 0; i < parts.count(); ++i) {
            QString part = parts.at(i);
            part.replace(QLatin1String("$$[QT_INSTALL_LIBS]"), qtEnv.libraryPath);
            if (part.startsWith(QLatin1String("-l"))) {
                libs << part.mid(2);
            } else if (part.startsWith(QLatin1String("-L"))) {
                libraryPaths << part.mid(2);
            } else if (part.startsWith(QLatin1String("-F"))) {
                frameworkPaths << part.mid(2);
            } else if (part == QLatin1String("-framework")) {
                if (++i < parts.count())
                    frameworks << parts.at(i);
            } else if (part == QLatin1String("-pthread")) {
                libs << QLatin1String("pthread");
            } else if (part.startsWith(QLatin1Char('-'))) { // Some other option
                qDebug("QMAKE_PRL_LIBS contains non-library option '%s' in file '%s'",
                       qPrintable(part), qPrintable(prlFilePath));
                flags << part;
            } else if (part.startsWith(QLatin1String("/LIBPATH:"))) {
                libraryPaths << part.mid(9).replace(QLatin1String("\\\\"), QLatin1String("/"));
            } else { // Assume it's a file path/name.
                libs << part.replace(QLatin1String("\\\\"), QLatin1String("/"));
            }
        }

        return;
    }
}

bool QtModuleInfo::isFramework(const QtEnvironment &qtEnv) const
{
    if (!qtEnv.frameworkBuild || isStaticLibrary)
        return false;
    const QStringList modulesNeverBuiltAsFrameworks = QStringList()
            << QLatin1String("bootstrap") << QLatin1String("openglextensions")
            << QLatin1String("platformsupport") << QLatin1String("qmldevtools")
            << QLatin1String("uitools") << QLatin1String("harfbuzzng");
    return !modulesNeverBuiltAsFrameworks.contains(qbsName);
}

// We erroneously called the "testlib" module "test" for quite a while. Let's not punish users
// for that.
static void addTestModule(QList<QtModuleInfo> &modules)
{
    QtModuleInfo testModule(QLatin1String("QtTest"), QLatin1String("test"),
                               QStringList() << QLatin1String("testlib"));
    testModule.hasLibrary = false;
    modules << testModule;
}

// See above.
static void addDesignerComponentsModule(QList<QtModuleInfo> &modules)
{
    QtModuleInfo module(QLatin1String("QtDesignerComponents"),
                        QLatin1String("designercomponents"),
                        QStringList() << QLatin1String("designercomponents-private"));
    module.hasLibrary = false;
    modules << module;
}


QList<QtModuleInfo> allQt4Modules(const QtEnvironment &qtEnvironment)
{
    // as per http://doc.qt.io/qt-4.8/modules.html + private stuff.
    QList<QtModuleInfo> modules;

    QtModuleInfo core(QLatin1String("QtCore"), QLatin1String("core"));
    core.compilerDefines << QLatin1String("QT_CORE_LIB");
    if (!qtEnvironment.qtNameSpace.isEmpty())
        core.compilerDefines << QLatin1String("QT_NAMESPACE=") + qtEnvironment.qtNameSpace;

    modules = QList<QtModuleInfo>()
            << core
            << QtModuleInfo(QLatin1String("QtCore"), QLatin1String("core-private"),
                            QStringList() << QLatin1String("core"))
            << QtModuleInfo(QLatin1String("QtGui"), QLatin1String("gui"))
            << QtModuleInfo(QLatin1String("QtGui"), QLatin1String("gui-private"),
                            QStringList() << QLatin1String("gui"))
            << QtModuleInfo(QLatin1String("QtMultimedia"), QLatin1String("multimedia"),
                            QStringList() << QLatin1String("gui") << QLatin1String("network"))
            << QtModuleInfo(QLatin1String("QtMultimedia"), QLatin1String("multimedia-private"),
                            QStringList() << QLatin1String("multimedia"))
            << QtModuleInfo(QLatin1String("QtNetwork"), QLatin1String("network"))
            << QtModuleInfo(QLatin1String("QtNetwork"), QLatin1String("network-private"),
                            QStringList() << QLatin1String("network"))
            << QtModuleInfo(QLatin1String("QtOpenGL"), QLatin1String("opengl"),
                            QStringList() << QLatin1String("gui"))
            << QtModuleInfo(QLatin1String("QtOpenGL"), QLatin1String("opengl-private"),
                            QStringList() << QLatin1String("opengl"))
            << QtModuleInfo(QLatin1String("QtOpenVG"), QLatin1String("openvg"),
                            QStringList() << QLatin1String("gui"))
            << QtModuleInfo(QLatin1String("QtScript"), QLatin1String("script"))
            << QtModuleInfo(QLatin1String("QtScript"), QLatin1String("script-private"),
                            QStringList() << QLatin1String("script"))
            << QtModuleInfo(QLatin1String("QtScriptTools"), QLatin1String("scripttools"),
                            QStringList() << QLatin1String("script") << QLatin1String("gui"))
            << QtModuleInfo(QLatin1String("QtScriptTools"), QLatin1String("scripttools-private"),
                            QStringList() << QLatin1String("scripttools"))
            << QtModuleInfo(QLatin1String("QtSql"), QLatin1String("sql"))
            << QtModuleInfo(QLatin1String("QtSql"), QLatin1String("sql-private"),
                            QStringList() << QLatin1String("sql"))
            << QtModuleInfo(QLatin1String("QtSvg"), QLatin1String("svg"),
                            QStringList() << QLatin1String("gui"))
            << QtModuleInfo(QLatin1String("QtSvg"), QLatin1String("svg-private"),
                            QStringList() << QLatin1String("svg"))
            << QtModuleInfo(QLatin1String("QtWebKit"), QLatin1String("webkit"),
                            QStringList() << QLatin1String("gui") << QLatin1String("network"))
            << QtModuleInfo(QLatin1String("QtWebKit"), QLatin1String("webkit-private"),
                            QStringList() << QLatin1String("webkit"))
            << QtModuleInfo(QLatin1String("QtXml"), QLatin1String("xml"))
            << QtModuleInfo(QLatin1String("QtXml"), QLatin1String("xml-private"),
                            QStringList() << QLatin1String("xml"))
            << QtModuleInfo(QLatin1String("QtXmlPatterns"), QLatin1String("xmlpatterns"),
                            QStringList() << QLatin1String("network"))
            << QtModuleInfo(QLatin1String("QtXmlPatterns"),
                            QLatin1String("xmlpatterns-private"),
                            QStringList() << QLatin1String("xmlpatterns"))
            << QtModuleInfo(QLatin1String("QtDeclarative"), QLatin1String("declarative"),
                            QStringList() << QLatin1String("gui") << QLatin1String("script"))
            << QtModuleInfo(QLatin1String("QtDeclarative"),
                            QLatin1String("declarative-private"),
                            QStringList() << QLatin1String("declarative"))
            << QtModuleInfo(QLatin1String("QtDesigner"), QLatin1String("designer"),
                            QStringList() << QLatin1String("gui") << QLatin1String("xml"))
            << QtModuleInfo(QLatin1String("QtDesigner"), QLatin1String("designer-private"),
                            QStringList() << QLatin1String("designer"))
            << QtModuleInfo(QLatin1String("QtUiTools"), QLatin1String("uitools"))
            << QtModuleInfo(QLatin1String("QtUiTools"), QLatin1String("uitools-private"),
                            QStringList() << QLatin1String("uitools"))
            << QtModuleInfo(QLatin1String("QtHelp"), QLatin1String("help"),
                            QStringList() << QLatin1String("network") << QLatin1String("sql"))
            << QtModuleInfo(QLatin1String("QtHelp"), QLatin1String("help-private"),
                            QStringList() << QLatin1String("help"))
            << QtModuleInfo(QLatin1String("QtTest"), QLatin1String("testlib"))
            << QtModuleInfo(QLatin1String("QtTest"), QLatin1String("testlib-private"),
                            QStringList() << QLatin1String("testlib"));

    if (qtEnvironment.mkspecName.startsWith(QLatin1String("win"))) {
        QtModuleInfo axcontainer(QLatin1String("QAxContainer"), QLatin1String("axcontainer"));
        axcontainer.modulePrefix = QLatin1String("Q");
        axcontainer.isStaticLibrary = true;
        axcontainer.includePaths << qtEnvironment.includePath + QLatin1String("/ActiveQt");
        modules << axcontainer;

        QtModuleInfo axserver = axcontainer;
        axserver.name = QLatin1String("QAxServer");
        axserver.qbsName = QLatin1String("axserver");
        axserver.compilerDefines = QStringList() << QLatin1String("QAXSERVER");
        modules << axserver;
    } else {
        modules << QtModuleInfo(QLatin1String("QtDBus"), QLatin1String("dbus"))
                << QtModuleInfo(QLatin1String("QtDBus"), QLatin1String("dbus-private"),
                                QStringList() << QLatin1String("dbus"));
    }

    QtModuleInfo designerComponentsPrivate(QLatin1String("QtDesignerComponents"),
            QLatin1String("designercomponents-private"),
            QStringList() << QLatin1String("gui-private") << QLatin1String("designer-private"));
    designerComponentsPrivate.hasLibrary = true;
    modules << designerComponentsPrivate;

    QtModuleInfo phonon(QLatin1String("Phonon"), QLatin1String("phonon"));
    phonon.includePaths = phonon.qt4ModuleIncludePaths(qtEnvironment);
    modules << phonon;

    // Set up include paths that haven't been set up before this point.
    for (QList<QtModuleInfo>::iterator it = modules.begin(); it != modules.end(); ++it) {
        QtModuleInfo &module = *it;
        if (!module.includePaths.isEmpty())
            continue;
        module.includePaths = module.qt4ModuleIncludePaths(qtEnvironment);
    }

    // Set up compiler defines haven't been set up before this point.
    for (QList<QtModuleInfo>::iterator it = modules.begin(); it != modules.end(); ++it) {
        QtModuleInfo &module = *it;
        if (!module.compilerDefines.isEmpty())
            continue;
        module.compilerDefines
                << QLatin1String("QT_") + module.qbsName.toUpper() + QLatin1String("_LIB");
    }

    // These are for the convenience of project file authors. It allows them
    // to add a dependency to e.g. "Qt.widgets" without a version check.
    QtModuleInfo virtualModule;
    virtualModule.hasLibrary = false;
    virtualModule.qbsName = QLatin1String("widgets");
    virtualModule.dependencies = QStringList() << QLatin1String("core") << QLatin1String("gui");
    modules << virtualModule;
    virtualModule.qbsName = QLatin1String("quick");
    virtualModule.dependencies = QStringList() << QLatin1String("declarative");
    modules << virtualModule;
    virtualModule.qbsName = QLatin1String("concurrent");
    virtualModule.dependencies = QStringList() << QLatin1String("core");
    modules << virtualModule;
    virtualModule.qbsName = QLatin1String("printsupport");
    virtualModule.dependencies = QStringList() << QLatin1String("core") << QLatin1String("gui");
    modules << virtualModule;

    addTestModule(modules);
    addDesignerComponentsModule(modules);

    const QStringList modulesThatCanBeDisabled = QStringList() << QLatin1String("xmlpatterns")
            << QLatin1String("multimedia") << QLatin1String("phonon") << QLatin1String("svg")
            << QLatin1String("webkit") << QLatin1String("script") << QLatin1String("scripttools")
            << QLatin1String("declarative") << QLatin1String("gui") << QLatin1String("dbus")
            << QLatin1String("opengl") << QLatin1String("openvg");
    for (int i = 0; i < modules.count(); ++i) {
        QString name = modules[i].qbsName;
        name.remove(QLatin1String("-private"));
        if (modulesThatCanBeDisabled.contains(name))
            modules[i].mustExist = false;
    }

    Internal::Set<QString> nonExistingPrlFiles;
    for (int i = 0; i < modules.count(); ++i) {
        QtModuleInfo &module = modules[i];
        if (qtEnvironment.staticBuild)
            module.isStaticLibrary = true;
        module.setupLibraries(qtEnvironment, &nonExistingPrlFiles);
    }
    replaceQtLibNamesWithFilePath(&modules, qtEnvironment);

    return modules;
}

static QList<QByteArray> getPriFileContentsRecursively(const Profile &profile,
                                                       const QString &priFilePath)
{
    QFile priFile(priFilePath);
    if (!priFile.open(QIODevice::ReadOnly)) {
        throw ErrorInfo(Tr::tr("Setting up Qt profile '%1' failed: Cannot open "
                "file '%2' (%3).").arg(profile.name(), priFile.fileName(), priFile.errorString()));
    }
    QList<QByteArray> lines = priFile.readAll().split('\n');
    for (int i = 0; i < lines.count(); ++i) {
        const QByteArray includeString = "include(";
        const QByteArray &line = lines.at(i).trimmed();
        if (!line.startsWith(includeString))
            continue;
        const int offset = includeString.count();
        const int closingParenPos = line.indexOf(')', offset);
        if (closingParenPos == -1) {
            qDebug("Warning: Invalid include statement in '%s'", qPrintable(priFilePath));
            continue;
        }
        const QString includedFilePath
                = QString::fromLocal8Bit(line.mid(offset, closingParenPos - offset));
        const QList<QByteArray> &includedContents
                = getPriFileContentsRecursively(profile, includedFilePath);
        int j = i;
        foreach (const QByteArray &includedLine, includedContents)
            lines.insert(++j, includedLine);
        lines.removeAt(i--);
    }
    return lines;
}

QList<QtModuleInfo> allQt5Modules(const Profile &profile, const QtEnvironment &qtEnvironment)
{
    Internal::Set<QString> nonExistingPrlFiles;
    QList<QtModuleInfo> modules;
    QDirIterator dit(qtEnvironment.mkspecBasePath + QLatin1String("/modules"));
    while (dit.hasNext()) {
        const QString moduleFileNamePrefix = QLatin1String("qt_lib_");
        const QString pluginFileNamePrefix = QLatin1String("qt_plugin_");
        const QString moduleFileNameSuffix = QLatin1String(".pri");
        dit.next();
        const bool fileHasPluginPrefix = dit.fileName().startsWith(pluginFileNamePrefix);
        if ((!fileHasPluginPrefix && !dit.fileName().startsWith(moduleFileNamePrefix))
                || !dit.fileName().endsWith(moduleFileNameSuffix)) {
            continue;
        }
        QtModuleInfo moduleInfo;
        moduleInfo.isPlugin = fileHasPluginPrefix;
        const QString fileNamePrefix
                = moduleInfo.isPlugin ? pluginFileNamePrefix : moduleFileNamePrefix;
        moduleInfo.qbsName = dit.fileName().mid(fileNamePrefix.count(),
                dit.fileName().count() - fileNamePrefix.count()
                - moduleFileNameSuffix.count());
        if (moduleInfo.isPlugin) {
            moduleInfo.name = moduleInfo.qbsName;
            moduleInfo.isStaticLibrary = true;
        }
        const QByteArray moduleKeyPrefix = QByteArray(moduleInfo.isPlugin ? "QT_PLUGIN" : "QT")
                + '.' + moduleInfo.qbsName.toLatin1() + '.';
        moduleInfo.qbsName.replace(QLatin1String("_private"), QLatin1String("-private"));
        bool hasV2 = false;
        bool hasModuleEntry = false;
        foreach (const QByteArray &line, getPriFileContentsRecursively(profile, dit.filePath())) {
            const QByteArray simplifiedLine = line.simplified();
            const int firstEqualsOffset = simplifiedLine.indexOf('=');
            if (firstEqualsOffset == -1)
                continue;
            const QByteArray key = simplifiedLine.left(firstEqualsOffset).trimmed();
            const QByteArray value = simplifiedLine.mid(firstEqualsOffset + 1).trimmed();
            if (!key.startsWith(moduleKeyPrefix) || value.isEmpty())
                continue;
            if (key.endsWith(".name")) {
                moduleInfo.name = QString::fromLocal8Bit(value);
            } else if (key.endsWith(".module")) {
                hasModuleEntry = true;
            } else if (key.endsWith(".depends")) {
                moduleInfo.dependencies = QString::fromLocal8Bit(value).split(QLatin1Char(' '));
                for (int i = 0; i < moduleInfo.dependencies.count(); ++i) {
                    moduleInfo.dependencies[i].replace(QLatin1String("_private"),
                                                       QLatin1String("-private"));
                }
            } else if (key.endsWith(".module_config")) {
                foreach (const QByteArray &elem, value.split(' ')) {
                    if (elem == "no_link")
                        moduleInfo.hasLibrary = false;
                    else if (elem == "staticlib")
                        moduleInfo.isStaticLibrary = true;
                    else if (elem == "internal_module")
                        moduleInfo.isPrivate = true;
                    else if (elem == "v2")
                        hasV2 = true;
                }
            } else if (key.endsWith(".includes")) {
                moduleInfo.includePaths = QString::fromLocal8Bit(value).split(QLatin1Char(' '));
                for (int i = 0; i < moduleInfo.includePaths.count(); ++i) {
                    moduleInfo.includePaths[i]
                            .replace(QLatin1String("$$QT_MODULE_INCLUDE_BASE"),
                                     qtEnvironment.includePath)
                            .replace(QLatin1String("$$QT_MODULE_LIB_BASE"),
                                     qtEnvironment.libraryPath);
                }
            } else if (key.endsWith(".DEFINES")) {
                moduleInfo.compilerDefines = QString::fromLocal8Bit(value)
                        .split(QLatin1Char(' '), QString::SkipEmptyParts);
            } else if (key.endsWith(".VERSION")) {
                moduleInfo.version = QString::fromLocal8Bit(value);
            } else if (key.endsWith(".plugin_types")) {
                moduleInfo.supportedPluginTypes = makeList(value);
            } else if (key.endsWith(".TYPE")) {
                moduleInfo.pluginData.type = QString::fromLatin1(value);
            } else if (key.endsWith(".EXTENDS")) {
                moduleInfo.pluginData.extends = QString::fromLatin1(value);
            } else if (key.endsWith(".CLASS_NAME")) {
                moduleInfo.pluginData.className = QString::fromLatin1(value);
            }
        }
        if (hasV2 && !hasModuleEntry)
            moduleInfo.hasLibrary = false;

        // Fix include paths for Apple frameworks.
        // The qt_lib_XXX.pri files contain wrong values for versions < 5.6.
        if (!hasV2 && moduleInfo.isFramework(qtEnvironment)) {
            moduleInfo.includePaths.clear();
            QString baseIncDir = moduleInfo.frameworkHeadersPath(qtEnvironment);
            if (moduleInfo.isPrivate) {
                baseIncDir += QLatin1Char('/') + moduleInfo.version;
                moduleInfo.includePaths
                        << baseIncDir
                        << baseIncDir + QLatin1Char('/') + moduleInfo.name;
            } else {
                moduleInfo.includePaths << baseIncDir;
            }
        }

        moduleInfo.setupLibraries(qtEnvironment, &nonExistingPrlFiles);

        modules << moduleInfo;
        if (moduleInfo.qbsName == QLatin1String("testlib"))
            addTestModule(modules);
        if (moduleInfo.qbsName == QLatin1String("designercomponents-private"))
            addDesignerComponentsModule(modules);
    }

    replaceQtLibNamesWithFilePath(&modules, qtEnvironment);
    return modules;
}

QString QtModuleInfo::libBaseName(const QString &libName, bool debugBuild,
                                  const QtEnvironment &qtEnvironment) const
{
    QString name = libName;
    if (qtEnvironment.mkspecName.startsWith(QLatin1String("win"))) {
        if (debugBuild)
            name += QLatin1Char('d');
        if (!isStaticLibrary && qtEnvironment.qtMajorVersion < 5)
            name += QString::number(qtEnvironment.qtMajorVersion);
    }
    if (qtEnvironment.mkspecName.contains(QLatin1String("macx"))
            || qtEnvironment.mkspecName.contains(QLatin1String("ios"))
            || qtEnvironment.mkspecName.contains(QLatin1String("darwin"))) {
        if (!isFramework(qtEnvironment)
                && qtEnvironment.buildVariant.contains(QLatin1String("debug"))
                && (!qtEnvironment.buildVariant.contains(QLatin1String("release")) || debugBuild)) {
            name += QLatin1String("_debug");
        }
    }
    return name;
}

} // namespace Internal
} // namespace qbs

