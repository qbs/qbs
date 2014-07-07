/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/
#include "qtmoduleinfo.h"

#include "qtenvironment.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/profile.h>

#include <QDirIterator>
#include <QFile>

namespace qbs {
namespace Internal {

QtModuleInfo::QtModuleInfo() : isPrivate(false), hasLibrary(true), isStaticLibrary(false)
{
}

QtModuleInfo::QtModuleInfo(const QString &name, const QString &qbsName, const QStringList &deps)
    : name(name), qbsName(qbsName), dependencies(deps),
      isPrivate(qbsName.endsWith(QLatin1String("-private"))),
      hasLibrary(!isPrivate),
      isStaticLibrary(false)
{
    const QString coreModule = QLatin1String("core");
    if (qbsName != coreModule && !dependencies.contains(coreModule))
        dependencies.prepend(coreModule);
}

QString QtModuleInfo::moduleName() const
{
    if (name.startsWith(QLatin1String("Qt")))
        return name.mid(2); // Strip off "Qt".
    if (name.startsWith(QLatin1String("QAx")))
        return name.mid(1); // Strip off "Q".
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
    if (qtEnvironment.frameworkBuild && !isStaticLibrary) {
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
    // Enginio has a different naming scheme, so it doesn't get boring.
    const bool isEnginio = name == QLatin1String("Enginio");

    QString libName = modulePrefix.isEmpty() && !isEnginio ? QLatin1String("Qt") : modulePrefix;
    if (qtEnvironment.qtMajorVersion >= 5 && (!qtEnvironment.frameworkBuild || isStaticLibrary)
            && !isEnginio) {
        libName += QString::number(qtEnvironment.qtMajorVersion);
    }
    libName += name.startsWith(QLatin1String("Qt")) ? name.mid(2) : name;
    libName += qtEnvironment.qtLibInfix;
    return libBaseName(libName, isStaticLibrary, debugBuild, qtEnvironment);
}

QString QtModuleInfo::libNameForLinker(const QtEnvironment &qtEnvironment, bool debugBuild) const
{
    QString libName = libraryBaseName(qtEnvironment, debugBuild);
    if (qtEnvironment.mkspecName.contains(QLatin1String("msvc")))
        libName += QLatin1String(".lib");
    return libName;
}

void QtModuleInfo::setupLibraries(const QtEnvironment &qtEnv)
{
    setupLibraries(qtEnv, true);
    setupLibraries(qtEnv, false);
}

void QtModuleInfo::setupLibraries(const QtEnvironment &qtEnv, bool debugBuild)
{
    QStringList &libs = isStaticLibrary
            ? (debugBuild ? staticLibrariesDebug : staticLibrariesRelease)
            : (debugBuild ? dynamicLibrariesDebug : dynamicLibrariesRelease);
    QStringList &frameworks = debugBuild ? frameworksDebug : frameworksRelease;
    QStringList &frameworkPaths = debugBuild ? frameworkPathsDebug : frameworkPathsRelease;
    QStringList &flags = debugBuild ? linkerFlagsDebug : linkerFlagsRelease;

    if (qtEnv.mkspecName.contains(QLatin1String("ios")) && isStaticLibrary) {
        QtModuleInfo platformSupportModule = *this;
        platformSupportModule.name = QLatin1String("QtPlatformSupport");
        libs << QLatin1String("z") << QLatin1String("m")
             << platformSupportModule.libNameForLinker(qtEnv, debugBuild);
        flags << QLatin1String("-force_load")
              << qtEnv.pluginPath + QLatin1String("/platforms/")
                 + libBaseName(QLatin1String("libqios"), true, debugBuild, qtEnv)
                 + QLatin1String(".a");
    }

    QString prlFilePath = qtEnv.libraryPath + QLatin1Char('/');
    if (qtEnv.frameworkBuild)
        prlFilePath.append(libraryBaseName(qtEnv, false)).append(QLatin1String(".framework/"));
    if (!qtEnv.mkspecName.contains(QLatin1String("win")) && !qtEnv.frameworkBuild)
        prlFilePath += QLatin1String("lib");
    prlFilePath.append(libraryBaseName(qtEnv, debugBuild)).append(QLatin1String(".prl"));
    QFile prlFile(prlFilePath);
    if (!prlFile.open(QIODevice::ReadOnly)) {
        // We can't error out here, as some modules in a self-built Qt don't have the expected
        // file names. Real-life example: "libQt0Feedback.prl". This is just too stupid
        // to work around, so let's ignore it.
        qDebug("Skipping prl file '%s', because it cannot be opened (%s).", qPrintable(prlFilePath),
               qPrintable(prlFile.errorString()));
        return;
    }
    const QList<QByteArray> prlLines = prlFile.readAll().split('\n');
    foreach (const QByteArray &line, prlLines) {
        const QByteArray simplifiedLine = line.simplified();
        if (!simplifiedLine.startsWith("QMAKE_PRL_LIBS"))
            continue;
        const int equalsOffset = simplifiedLine.indexOf('=');
        if (equalsOffset == -1)
            continue;

        // Assuming lib names and directories without spaces here.
        QStringList parts = QString::fromLatin1(simplifiedLine.mid(equalsOffset + 1).trimmed())
                .split(QLatin1Char(' '), QString::SkipEmptyParts);
        for (int i = 0; i < parts.count(); ++i) {
            QString part = parts.at(i);
            if (part.startsWith(QLatin1String("-l"))) {
                libs << part.mid(2);
            } else if (part.startsWith(QLatin1String("-L"))) {
                libraryPaths << part.mid(2);
            } else if (part.startsWith(QLatin1String("-F"))) {
                frameworkPaths << part.mid(2);
            } else if (part == QLatin1String("-framework")) {
                if (++i < parts.count())
                    frameworks << parts.at(i);
            } else if (part.startsWith(QLatin1Char('-'))) { // Some other option, e.g. "-pthread".
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
    // as per http://qt-project.org/doc/qt-4.8/modules.html + private stuff.
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
            << QtModuleInfo(QLatin1String("QtScriptTools"), QLatin1String("scripttols"),
                            QStringList() << QLatin1String("script") << QLatin1String("gui"))
            << QtModuleInfo(QLatin1String("QtScriptTools"), QLatin1String("scripttols-private"),
                            QStringList() << QLatin1String("scripttols"))
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
                            QStringList() << "network")
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
                            QStringList() << QLatin1String("testlib"))
            << QtModuleInfo(QLatin1String("QtDBus"), QLatin1String("dbus"))
            << QtModuleInfo(QLatin1String("QtDBus"), QLatin1String("dbus-private"),
                            QStringList() << QLatin1String("dbus"));

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
                << QLatin1String("QT_") + module.name.toUpper() + QLatin1String("_LIB");
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

    return modules;
}

QList<QtModuleInfo> allQt5Modules(const Profile &profile, const QtEnvironment &qtEnvironment)
{
    QList<QtModuleInfo> modules;
    QDirIterator dit(qtEnvironment.mkspecBasePath + QLatin1String("/modules"));
    while (dit.hasNext()) {
        const QString moduleFileNamePrefix = QLatin1String("qt_lib_");
        const QString moduleFileNameSuffix = QLatin1String(".pri");
        dit.next();
        if (!dit.fileName().startsWith(moduleFileNamePrefix)
                || !dit.fileName().endsWith(moduleFileNameSuffix)) {
            continue;
        }
        QtModuleInfo moduleInfo;
        moduleInfo.qbsName = dit.fileName().mid(moduleFileNamePrefix.count(),
                dit.fileName().count() - moduleFileNamePrefix.count()
                - moduleFileNameSuffix.count());
        moduleInfo.qbsName.replace(QLatin1String("_private"), QLatin1String("-private"));
        QFile priFile(dit.filePath());
        if (!priFile.open(QIODevice::ReadOnly)) {
            throw ErrorInfo(Tr::tr("Setting up Qt profile '%1' failed: Cannot open "
                    "file '%2' (%3).").arg(profile.name(), priFile.fileName(), priFile.errorString()));
        }
        const QByteArray priFileContents = priFile.readAll();
        foreach (const QByteArray &line, priFileContents.split('\n')) {
            const QByteArray simplifiedLine = line.simplified();
            const int firstEqualsOffset = simplifiedLine.indexOf('=');
            if (firstEqualsOffset == -1)
                continue;
            const QByteArray key = simplifiedLine.left(firstEqualsOffset).trimmed();
            const QByteArray value = simplifiedLine.mid(firstEqualsOffset + 1).trimmed();
            if (key.isEmpty() || value.isEmpty())
                continue;
            if (key.endsWith(".name")) {
                moduleInfo.name = QString::fromLocal8Bit(value);
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
                }
            } else if (key.endsWith(".includes")) {
                moduleInfo.includePaths = QString::fromLocal8Bit(value).split(QLatin1Char(' '));
                for (int i = 0; i < moduleInfo.includePaths.count(); ++i) {
                    moduleInfo.includePaths[i].replace(
                                QLatin1String("$$QT_MODULE_INCLUDE_BASE"),
                                qtEnvironment.includePath);
                }
            } else if (key.endsWith(".DEFINES")) {
                moduleInfo.compilerDefines = QString::fromLocal8Bit(value)
                        .split(QLatin1Char(' '), QString::SkipEmptyParts);
            } else if (key.endsWith(".VERSION")) {
                moduleInfo.version = QString::fromLocal8Bit(value);
            }
        }

        // Fix include paths for OS X frameworks. The qt_lib_XXX.pri files contain wrong values.
        if (qtEnvironment.frameworkBuild && !moduleInfo.isStaticLibrary) {
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

        moduleInfo.setupLibraries(qtEnvironment);

        modules << moduleInfo;
        if (moduleInfo.qbsName == QLatin1String("testlib"))
            addTestModule(modules);
        if (moduleInfo.qbsName == QLatin1String("designercomponents-private"))
            addDesignerComponentsModule(modules);
    }
    return modules;
}

QString libBaseName(const QString &libName, bool staticLib, bool debugBuild,
                             const QtEnvironment &qtEnvironment)
{
    QString name = libName;
    if (qtEnvironment.mkspecName.contains(QLatin1String("win"))) {
        if (debugBuild)
            name += QLatin1Char('d');
        if (!staticLib && qtEnvironment.qtMajorVersion < 5)
            name += QString::number(qtEnvironment.qtMajorVersion);
    }
    if (qtEnvironment.mkspecName.contains(QLatin1String("macx"))
            || qtEnvironment.mkspecName.contains(QLatin1String("darwin"))) {
        if (!qtEnvironment.frameworkBuild
                && qtEnvironment.buildVariant.contains(QLatin1String("debug"))
                && (!qtEnvironment.buildVariant.contains(QLatin1String("release")) || debugBuild)) {
            name += QLatin1String("_debug");
        }
    }
    return name;
}

} // namespace Internal
} // namespace qbs

