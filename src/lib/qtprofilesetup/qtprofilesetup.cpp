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

#include "qtprofilesetup.h"

#include <logging/translator.h>
#include <tools/error.h>
#include <tools/fileinfo.h>
#include <tools/profile.h>
#include <tools/settings.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QRegExp>
#include <QTextStream>

namespace qbs {

struct QtModuleInfo
{
    QtModuleInfo(const QString &name, const QString &qbsName,
                 const QStringList &deps = QStringList())
        : name(name), qbsName(qbsName), dependencies(deps),
          hasLibrary(!qbsName.endsWith(QLatin1String("-private"))),
          isStaticLibrary(false)
    {
        const QString coreModule = QLatin1String("core");
        if (qbsName != coreModule && !dependencies.contains(coreModule))
            dependencies.prepend(coreModule);
    }

    QtModuleInfo()
        : hasLibrary(true), isStaticLibrary(false)
    {}

    QString modulePrefix; // default is empty and means "Qt".
    QString name; // As in the path to the headers and ".name" in the pri files.
    QString qbsName; // Lower-case version without "qt" prefix.
    QStringList dependencies; // qbs names.
    QStringList includePaths;
    bool hasLibrary;
    bool isStaticLibrary;
};

static QString qtModuleName(const QtModuleInfo &module)
{
    if (module.name.startsWith(QLatin1String("Qt")))
        return module.name.mid(2); // Strip off "Qt".
    else if (module.name.startsWith(QLatin1String("QAx")))
        return module.name.mid(1); // Strip off "Q".
    else
        return module.name;
}

static void copyTemplateFile(const QString &fileName, const QString &targetDirectory,
                             const QString &profileName)
{
    if (!QDir::root().mkpath(targetDirectory)) {
        throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: "
                                         "Cannot create directory '%2'.")
                        .arg(profileName, targetDirectory));
    }
    QFile sourceFile(QLatin1String(":/templates/") + fileName);
    const QString targetPath = targetDirectory + QLatin1Char('/') + fileName;
    QFile::remove(targetPath); // QFile::copy() cannot overwrite.
    if (!sourceFile.copy(targetPath)) {
        throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: "
            "Cannot copy file '%2' into directory '%3' (%4).")
                        .arg(profileName, fileName, targetDirectory, sourceFile.errorString()));
    }
    QFile targetFile(targetPath);
    if (!targetFile.setPermissions(targetFile.permissions() | QFile::WriteUser)) {
        throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: Cannot set write "
                "permission on file '%2' (%3).")
                .arg(profileName, targetPath, targetFile.errorString()));
    }
}

static void replaceListPlaceholder(QByteArray &content, const QByteArray &placeHolder,
                                   const QStringList &list)
{
    QByteArray listString;
    foreach (const QString &elem, list)
        listString += "'" + elem.toUtf8() + "', ";
    content.replace(placeHolder, listString);
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

static void createModules(Profile &profile, Settings *settings,
                               const QtEnvironment &qtEnvironment)
{
    QList<QtModuleInfo> modules;
    if (qtEnvironment.qtMajorVersion < 5) {
        modules // as per http://qt-project.org/doc/qt-4.8/modules.html + private stuff.
                << QtModuleInfo(QLatin1String("QtCore"), QLatin1String("core"))
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
                << QtModuleInfo(QLatin1String("Phonon"), QLatin1String("phonon"))
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
        modules << axcontainer;

        QtModuleInfo axserver = axcontainer;
        axserver.name = QLatin1String("QAxServer");
        axserver.qbsName = QLatin1String("axserver");
        modules << axserver;

        QtModuleInfo designerComponentsPrivate(QLatin1String("QtDesignerComponents"),
                QLatin1String("designercomponents-private"),
                QStringList() << QLatin1String("gui-private") << QLatin1String("designer-private"));
        designerComponentsPrivate.hasLibrary = true;
        modules << designerComponentsPrivate;

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
    } else {
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
                throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: Cannot open "
                        "file '%2' (%3).").arg(profile.name(), priFile.fileName(), priFile.errorString()));
            }
            const QByteArray priFileContents = priFile.readAll();
            foreach (const QByteArray &line, priFileContents.split('\n')) {
                const QByteArray simplifiedLine = line.simplified();
                const QList<QByteArray> parts = simplifiedLine.split('=');
                if (parts.count() != 2 || parts.at(1).isEmpty())
                    continue;
                const QByteArray key = parts.first().simplified();
                const QByteArray value = parts.last().simplified();
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
                    }
                } else if (key.endsWith(".includes")) {
                    moduleInfo.includePaths = QString::fromLocal8Bit(value).split(QLatin1Char(' '));
                    for (int i = 0; i < moduleInfo.includePaths.count(); ++i) {
                        moduleInfo.includePaths[i].replace(
                                    QLatin1String("$$QT_MODULE_INCLUDE_BASE"),
                                    qtEnvironment.includePath);
                    }
                }
            }
            modules << moduleInfo;
            if (moduleInfo.qbsName == QLatin1String("testlib"))
                addTestModule(modules);
            if (moduleInfo.qbsName == QLatin1String("designercomponents-private"))
                addDesignerComponentsModule(modules);
        }
    }

    const QString profileBaseDir = QString::fromLocal8Bit("%1/qbs/profiles/%2")
            .arg(QFileInfo(settings->fileName()).dir().absolutePath(), profile.name());
    const QString qbsQtModuleBaseDir = profileBaseDir + QLatin1String("/modules/Qt");
    QString removeError;
    if (!qbs::Internal::removeDirectoryWithContents(qbsQtModuleBaseDir, &removeError)) {
        throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: Could not remove "
                "the existing profile of the same name (%2).").arg(profile.name(), removeError));
    }
    copyTemplateFile(QLatin1String("QtModule.qbs"), qbsQtModuleBaseDir, profile.name());
    copyTemplateFile(QLatin1String("qtfunctions.js"), qbsQtModuleBaseDir, profile.name());
    foreach (const QtModuleInfo &module, modules) {
        const QString qbsQtModuleDir = qbsQtModuleBaseDir + QLatin1Char('/') + module.qbsName;
        if (module.qbsName == QLatin1String("core")) {
            copyTemplateFile(QLatin1String("core.qbs"), qbsQtModuleDir, profile.name());
            copyTemplateFile(QLatin1String("moc.js"), qbsQtModuleDir, profile.name());
        } else if (module.qbsName == QLatin1String("gui")) {
            copyTemplateFile(QLatin1String("gui.qbs"), qbsQtModuleDir, profile.name());
        } else if (module.qbsName == QLatin1String("phonon")) {
            copyTemplateFile(QLatin1String("phonon.qbs"), qbsQtModuleDir, profile.name());
        } else {
            copyTemplateFile(QLatin1String("module.qbs"), qbsQtModuleDir, profile.name());
            QFile moduleFile(qbsQtModuleDir + QLatin1String("/module.qbs"));
            if (!moduleFile.open(QIODevice::ReadWrite)) {
                throw ErrorInfo(Internal::Tr::tr("Setting up Qt profile '%1' failed: Cannot adapt "
                        "module file '%2' (%3).")
                        .arg(profile.name(), moduleFile.fileName(), moduleFile.errorString()));
            }
            QByteArray content = moduleFile.readAll();
            content.replace("### name", '"' + qtModuleName(module).toUtf8() + '"');
            content.replace("### has library", module.hasLibrary ? "true" : "false");
            replaceListPlaceholder(content, "### dependencies", module.dependencies);
            replaceListPlaceholder(content, "### includes", module.includePaths);
            QByteArray propertiesString;
            if (module.qbsName == QLatin1String("declarative")
                    || module.qbsName == QLatin1String("quick")) {
                const QByteArray debugMacro = module.qbsName == QLatin1String("declarative")
                            || qtEnvironment.qtMajorVersion < 5
                        ? "QT_DECLARATIVE_DEBUG" : "QT_QML_DEBUG";
                propertiesString = "property bool qmlDebugging: false\n"
                        "    cpp.defines: "
                        "qmlDebugging ? base.concat('" + debugMacro + "') : base";
            }
            if (!module.modulePrefix.isEmpty()) {
                if (!propertiesString.isEmpty())
                    propertiesString += "\n    ";
                propertiesString += "qtModulePrefix: \"" + module.modulePrefix.toUtf8() + '"';
            }
            if (module.isStaticLibrary) {
                if (!propertiesString.isEmpty())
                    propertiesString += "\n    ";
                propertiesString += "isStaticLibrary: true";
            }
            content.replace("### special properties", propertiesString);
            moduleFile.resize(0);
            moduleFile.write(content);
        }
    }
    profile.setValue(QLatin1String("preferences.qbsSearchPaths"), profileBaseDir);
}

static QString guessMinimumWindowsVersion(const QtEnvironment &qt)
{
    if (qt.mkspecName.startsWith("winrt-"))
        return QLatin1String("6.2");

    if (!qt.mkspecName.startsWith("win32-"))
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
    QDir libdir(qt.libraryPath);
    const QStringList coreLibFiles
            = libdir.entryList(QStringList(QLatin1String("*Core*")), QDir::Files);
    if (coreLibFiles.isEmpty())
        throw ErrorInfo(Internal::Tr::tr("Could not determine whether Qt is a static build."));
    foreach (const QString &fileName, coreLibFiles) {
        if (QLibrary::isLibrary(qt.libraryPath + QLatin1Char('/') + fileName))
            return false;
    }
    return true;
}

void doSetupQtProfile(const QString &profileName, Settings *settings,
                      const QtEnvironment &_qtEnvironment)
{
    QtEnvironment qtEnvironment = _qtEnvironment;
    const bool staticBuild = checkForStaticBuild(qtEnvironment);

    // determine whether user apps require C++11
    if (qtEnvironment.qtConfigItems.contains(QLatin1String("c++11")) && staticBuild)
        qtEnvironment.configItems.append(QLatin1String("c++11"));

    Profile profile(profileName, settings);
    profile.removeProfile();
    const QString settingsTemplate(QLatin1String("Qt.core.%1"));
    profile.setValue(settingsTemplate.arg("config"), qtEnvironment.configItems);
    profile.setValue(settingsTemplate.arg("qtConfig"), qtEnvironment.qtConfigItems);
    profile.setValue(settingsTemplate.arg("binPath"), qtEnvironment.binaryPath);
    profile.setValue(settingsTemplate.arg("libPath"), qtEnvironment.libraryPath);
    profile.setValue(settingsTemplate.arg("pluginPath"), qtEnvironment.pluginPath);
    profile.setValue(settingsTemplate.arg("incPath"), qtEnvironment.includePath);
    profile.setValue(settingsTemplate.arg("mkspecPath"), qtEnvironment.mkspecPath);
    profile.setValue(settingsTemplate.arg("docPath"), qtEnvironment.documentationPath);
    profile.setValue(settingsTemplate.arg("version"), qtEnvironment.qtVersion);
    profile.setValue(settingsTemplate.arg("namespace"), qtEnvironment.qtNameSpace);
    profile.setValue(settingsTemplate.arg("libInfix"), qtEnvironment.qtLibInfix);
    profile.setValue(settingsTemplate.arg("buildVariant"), qtEnvironment.buildVariant);
    profile.setValue(settingsTemplate.arg(QLatin1String("staticBuild")), staticBuild);

    // Set the minimum operating system versions appropriate for this Qt version
    const QString windowsVersion = guessMinimumWindowsVersion(qtEnvironment);
    QString osxVersion, iosVersion, androidVersion;

    if (qtEnvironment.mkspecPath.contains("macx")) {
        profile.setValue(settingsTemplate.arg("frameworkBuild"), qtEnvironment.frameworkBuild);
        if (qtEnvironment.qtMajorVersion >= 5) {
            osxVersion = QLatin1String("10.6");
        } else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 6) {
            QDir qconfigDir;
            if (qtEnvironment.frameworkBuild) {
                qconfigDir.setPath(qtEnvironment.libraryPath);
                qconfigDir.cd("QtCore.framework/Headers");
            } else {
                qconfigDir.setPath(qtEnvironment.includePath);
                qconfigDir.cd("Qt");
            }
            QFile qconfig(qconfigDir.absoluteFilePath("qconfig.h"));
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

                if (ts.status() == QTextStream::Ok)
                    osxVersion = qtCocoaBuild ? QLatin1String("10.5") : QLatin1String("10.4");
            }

            if (osxVersion.isEmpty()) {
                throw ErrorInfo(Internal::Tr::tr("Error reading qconfig.h; could not determine "
                                                 "whether Qt is using Cocoa or Carbon"));
            }
        }

        if (qtEnvironment.configItems.contains("c++11"))
            osxVersion = QLatin1String("10.7");
    }

    if (qtEnvironment.mkspecPath.contains("ios") && qtEnvironment.qtMajorVersion >= 5)
        iosVersion = QLatin1String("5.0");

    if (qtEnvironment.mkspecPath.contains("android")) {
        if (qtEnvironment.qtMajorVersion >= 5)
            androidVersion = QLatin1String("2.3");
        else if (qtEnvironment.qtMajorVersion == 4 && qtEnvironment.qtMinorVersion >= 8)
            androidVersion = QLatin1String("1.6"); // Necessitas
    }

    // ### TODO: wince, winphone, blackberry

    if (!windowsVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumWindowsVersion"), windowsVersion);

    if (!osxVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumOsxVersion"), osxVersion);

    if (!iosVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumIosVersion"), iosVersion);

    if (!androidVersion.isEmpty())
        profile.setValue(QLatin1String("cpp.minimumAndroidVersion"), androidVersion);

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
