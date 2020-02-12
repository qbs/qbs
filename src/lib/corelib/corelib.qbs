import qbs 1.0
import qbs.Utilities

QbsLibrary {
    Depends { name: "cpp" }
    Depends { name: "Qt"; submodules: ["core-private", "network", "xml"] }
    Depends {
        name: "Qt.script"
        condition: !qbsbuildconfig.useBundledQtScript
        required: false
    }
    Depends {
        name: "qbsscriptengine"
        condition: qbsbuildconfig.useBundledQtScript || !Qt.script.present
    }
    Depends { condition: qbsbuildconfig.enableProjectFileUpdates; name: "Qt.gui" }
    name: "qbscore"
    property stringList bundledQtScriptIncludes: qbsbuildconfig.useBundledQtScript
            || !Qt.script.present ? qbsscriptengine.includePaths : []
    cpp.includePaths: base.concat(bundledQtScriptIncludes).concat([
        ".",
        "../.." // for the plugin headers
    ])
    property stringList projectFileUpdateDefines:
        qbsbuildconfig.enableProjectFileUpdates ? ["QBS_ENABLE_PROJECT_FILE_UPDATES"] : []
    property stringList enableUnitTestsDefines:
        qbsbuildconfig.enableUnitTests ? ["QBS_ENABLE_UNIT_TESTS"] : []
    property stringList systemSettingsDirDefines: qbsbuildconfig.systemSettingsDir
        ? ['QBS_SYSTEM_SETTINGS_DIR="' + qbsbuildconfig.systemSettingsDir + '"'] : []
    cpp.defines: base.concat([
        "QBS_RELATIVE_LIBEXEC_PATH=" + Utilities.cStringQuote(qbsbuildconfig.relativeLibexecPath),
        "QBS_VERSION=" + Utilities.cStringQuote(version),
    ]).concat(projectFileUpdateDefines).concat(enableUnitTestsDefines)
    .concat(systemSettingsDirDefines)

    Properties {
        condition: qbs.targetOS.contains("windows")
        cpp.dynamicLibraries: base.concat(["psapi", "shell32"])
    }
    cpp.dynamicLibraries: base

    Properties {
        condition: qbs.targetOS.contains("darwin")
        cpp.frameworks: ["Foundation", "Security"]
    }

    Group {
        name: product.name
        files: ["qbs.h"]
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix
    }
    Group {
        name: "project file updating"
        condition: qbsbuildconfig.enableProjectFileUpdates
        prefix: "api/"
        files: [
            "changeset.cpp",
            "changeset.h",
            "projectfileupdater.cpp",
            "projectfileupdater.h",
            "qmljsrewriter.cpp",
            "qmljsrewriter.h",
        ]
    }

    Group {
        name: "api"
        prefix: name + '/'
        files: [
            "internaljobs.cpp",
            "internaljobs.h",
            "jobs.cpp",
            "languageinfo.cpp",
            "project.cpp",
            "project_p.h",
            "projectdata.cpp",
            "projectdata_p.h",
            "propertymap_p.h",
            "rulecommand.cpp",
            "rulecommand_p.h",
            "runenvironment.cpp",
            "transformerdata.cpp",
            "transformerdata_p.h",
        ]
    }
    Group {
        name: "public api headers"
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/api"
        prefix: "api/"
        files: [
            "jobs.h",
            "languageinfo.h",
            "project.h",
            "projectdata.h",
            "rulecommand.h",
            "runenvironment.h",
            "transformerdata.h",
        ]
    }
    Group {
        name: "buildgraph"
        prefix: name + '/'
        files: [
            "abstractcommandexecutor.cpp",
            "abstractcommandexecutor.h",
            "artifact.cpp",
            "artifact.h",
            "artifactcleaner.cpp",
            "artifactcleaner.h",
            "artifactsscriptvalue.cpp",
            "artifactsscriptvalue.h",
            "artifactvisitor.cpp",
            "artifactvisitor.h",
            "buildgraph.cpp",
            "buildgraph.h",
            "buildgraphnode.cpp",
            "buildgraphnode.h",
            "buildgraphloader.cpp",
            "buildgraphloader.h",
            "buildgraphvisitor.h",
            "cycledetector.cpp",
            "cycledetector.h",
            "dependencyparametersscriptvalue.cpp",
            "dependencyparametersscriptvalue.h",
            "depscanner.cpp",
            "depscanner.h",
            "emptydirectoriesremover.cpp",
            "emptydirectoriesremover.h",
            "environmentscriptrunner.cpp",
            "environmentscriptrunner.h",
            "executor.cpp",
            "executor.h",
            "executorjob.cpp",
            "executorjob.h",
            "filedependency.cpp",
            "filedependency.h",
            "inputartifactscanner.cpp",
            "inputartifactscanner.h",
            "jscommandexecutor.cpp",
            "jscommandexecutor.h",
            "nodeset.cpp",
            "nodeset.h",
            "nodetreedumper.cpp",
            "nodetreedumper.h",
            "processcommandexecutor.cpp",
            "processcommandexecutor.h",
            "productbuilddata.cpp",
            "productbuilddata.h",
            "productinstaller.cpp",
            "productinstaller.h",
            "projectbuilddata.cpp",
            "projectbuilddata.h",
            "qtmocscanner.cpp",
            "qtmocscanner.h",
            "rawscanneddependency.cpp",
            "rawscanneddependency.h",
            "rawscanresults.cpp",
            "rawscanresults.h",
            "requestedartifacts.cpp",
            "requestedartifacts.h",
            "requesteddependencies.cpp",
            "requesteddependencies.h",
            "rescuableartifactdata.h",
            "rulecommands.cpp",
            "rulecommands.h",
            "rulegraph.cpp",
            "rulegraph.h",
            "rulenode.cpp",
            "rulenode.h",
            "rulesapplicator.cpp",
            "rulesapplicator.h",
            "rulesevaluationcontext.cpp",
            "rulesevaluationcontext.h",
            "scriptclasspropertyiterator.h",
            "timestampsupdater.cpp",
            "timestampsupdater.h",
            "transformer.cpp",
            "transformer.h",
            "transformerchangetracking.cpp",
            "transformerchangetracking.h",
        ]
    }
    Group {
        name: "public buildgraph headers"
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/buildgraph"
        files: "buildgraph/forward_decls.h"
    }
    Group {
        name: "generators"
        prefix: "generators/"
        files: [
            "generatableprojectiterator.cpp",
            "generatableprojectiterator.h",
            "generator.cpp",
            "generatordata.cpp",
            "generatorutils.cpp",
            "generatorutils.h",
            "generatorversioninfo.cpp",
            "generatorversioninfo.h",
            "igeneratableprojectvisitor.h",
            "ixmlnodevisitor.h",
            "xmlproject.cpp",
            "xmlproject.h",
            "xmlprojectwriter.cpp",
            "xmlprojectwriter.h",
            "xmlproperty.cpp",
            "xmlproperty.h",
            "xmlpropertygroup.cpp",
            "xmlpropertygroup.h",
            "xmlworkspace.cpp",
            "xmlworkspace.h",
            "xmlworkspacewriter.cpp",
            "xmlworkspacewriter.h",
        ]
    }
    Group {
        name: "public generator headers"
        prefix: "generators/"
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/generators"
        files: [
            "generator.h",
            "generatordata.h",
        ]
    }
    Group {
        name: "jsextensions"
        prefix: name + '/'
        files: [
            "environmentextension.cpp",
            "file.cpp",
            "fileinfoextension.cpp",
            "jsextensions.cpp",
            "jsextensions.h",
            "moduleproperties.cpp",
            "moduleproperties.h",
            "process.cpp",
            "temporarydir.cpp",
            "textfile.cpp",
            "binaryfile.cpp",
            "utilitiesextension.cpp",
            "domxml.cpp",
        ]
    }
    Group {
        name: "jsextensions (Non-Darwin-specific)"
        prefix: "jsextensions/"
        condition: !qbs.targetOS.contains("darwin")
        files: [
            "propertylist.cpp",
        ]
    }
    Group {
        name: "jsextensions (Darwin-specific)"
        prefix: "jsextensions/"
        condition: qbs.targetOS.contains("darwin")
        files: [
            "propertylist.mm",
            "propertylistutils.h",
            "propertylistutils.mm",
        ]
    }
    Group {
        name: "language"
        prefix: name + '/'
        files: [
            "artifactproperties.cpp",
            "artifactproperties.h",
            "astimportshandler.cpp",
            "astimportshandler.h",
            "astpropertiesitemhandler.cpp",
            "astpropertiesitemhandler.h",
            "asttools.cpp",
            "asttools.h",
            "builtindeclarations.cpp",
            "builtindeclarations.h",
            "deprecationinfo.h",
            "evaluationdata.h",
            "evaluator.cpp",
            "evaluator.h",
            "evaluatorscriptclass.cpp",
            "evaluatorscriptclass.h",
            "filecontext.cpp",
            "filecontext.h",
            "filecontextbase.cpp",
            "filecontextbase.h",
            "filetags.cpp",
            "filetags.h",
            "identifiersearch.cpp",
            "identifiersearch.h",
            "item.cpp",
            "item.h",
            "itemdeclaration.cpp",
            "itemdeclaration.h",
            "itemobserver.h",
            "itempool.cpp",
            "itempool.h",
            "itemreader.cpp",
            "itemreader.h",
            "itemreaderastvisitor.cpp",
            "itemreaderastvisitor.h",
            "itemreadervisitorstate.cpp",
            "itemreadervisitorstate.h",
            "itemtype.h",
            "jsimports.h",
            "language.cpp",
            "language.h",
            "loader.cpp",
            "loader.h",
            "moduleloader.cpp",
            "moduleloader.h",
            "modulemerger.cpp",
            "modulemerger.h",
            "moduleproviderinfo.h",
            "preparescriptobserver.cpp",
            "preparescriptobserver.h",
            "projectresolver.cpp",
            "projectresolver.h",
            "property.cpp",
            "property.h",
            "propertydeclaration.cpp",
            "propertydeclaration.h",
            "propertymapinternal.cpp",
            "propertymapinternal.h",
            "qualifiedid.cpp",
            "qualifiedid.h",
            "resolvedfilecontext.cpp",
            "resolvedfilecontext.h",
            "scriptengine.cpp",
            "scriptengine.h",
            "scriptimporter.cpp",
            "scriptimporter.h",
            "scriptpropertyobserver.cpp",
            "scriptpropertyobserver.h",
            "value.cpp",
            "value.h",
        ]
    }
    Group {
        name: "public language headers"
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/language"
        files: "language/forward_decls.h"
    }
    Group {
        name: "logging"
        prefix: name + '/'
        files: [
            "categories.cpp",
            "categories.h",
            "ilogsink.cpp",
            "logger.cpp",
            "logger.h",
            "translator.h"
        ]
    }
    Group {
        name: "public logging headers"
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/logging"
        files: "logging/ilogsink.h"
    }
    Group {
        name: "parser"
        prefix: name + '/'
        files: [
            "qmlerror.cpp",
            "qmlerror.h",
            "qmljsast.cpp",
            "qmljsast_p.h",
            "qmljsastfwd_p.h",
            "qmljsastvisitor.cpp",
            "qmljsastvisitor_p.h",
            "qmljsengine_p.cpp",
            "qmljsengine_p.h",
            "qmljsglobal_p.h",
            "qmljsgrammar.cpp",
            "qmljsgrammar_p.h",
            "qmljskeywords_p.h",
            "qmljslexer.cpp",
            "qmljslexer_p.h",
            "qmljsmemorypool_p.h",
            "qmljsparser.cpp",
            "qmljsparser_p.h"
        ]
    }
    Group {
        name: "tools"
        prefix: name + '/'
        files: [
            "architectures.cpp",
            "buildgraphlocker.cpp",
            "buildgraphlocker.h",
            "buildoptions.cpp",
            "clangclinfo.cpp",
            "clangclinfo.h",
            "cleanoptions.cpp",
            "codelocation.cpp",
            "commandechomode.cpp",
            "dynamictypecheck.h",
            "error.cpp",
            "executablefinder.cpp",
            "executablefinder.h",
            "fileinfo.cpp",
            "fileinfo.h",
            "filesaver.cpp",
            "filesaver.h",
            "filetime.cpp",
            "filetime.h",
            "generateoptions.cpp",
            "hostosinfo.h",
            "id.cpp",
            "id.h",
            "iosutils.h",
            "joblimits.cpp",
            "jsliterals.cpp",
            "jsliterals.h",
            "jsonhelper.h",
            "installoptions.cpp",
            "launcherinterface.cpp",
            "launcherinterface.h",
            "launcherpackets.cpp",
            "launcherpackets.h",
            "launchersocket.cpp",
            "launchersocket.h",
            "msvcinfo.cpp",
            "msvcinfo.h",
            "pathutils.h",
            "persistence.cpp",
            "persistence.h",
            "preferences.cpp",
            "processresult.cpp",
            "processresult_p.h",
            "processutils.cpp",
            "processutils.h",
            "profile.cpp",
            "profiling.cpp",
            "profiling.h",
            "progressobserver.cpp",
            "progressobserver.h",
            "projectgeneratormanager.cpp",
            "qbsassert.cpp",
            "qbsassert.h",
            "qbspluginmanager.cpp",
            "qbspluginmanager.h",
            "qbsprocess.cpp",
            "qbsprocess.h",
            "qttools.cpp",
            "qttools.h",
            "scannerpluginmanager.cpp",
            "scannerpluginmanager.h",
            "scripttools.cpp",
            "scripttools.h",
            "set.h",
            "settings.cpp",
            "settingscreator.cpp",
            "settingscreator.h",
            "settingsmodel.cpp",
            "settingsrepresentation.cpp",
            "setupprojectparameters.cpp",
            "shellutils.cpp",
            "shellutils.h",
            "stlutils.h",
            "stringconstants.h",
            "stringutils.h",
            "toolchains.cpp",
            "version.cpp",
            "visualstudioversioninfo.cpp",
            "visualstudioversioninfo.h",
            "vsenvironmentdetector.cpp",
            "vsenvironmentdetector.h",
            "weakpointer.h",
        ]
    }
    Group {
        name: "public tools headers"
        prefix: "tools/"
        files: [
            "architectures.h",
            "buildoptions.h",
            "cleanoptions.h",
            "codelocation.h",
            "commandechomode.h",
            "error.h",
            "generateoptions.h",
            "installoptions.h",
            "joblimits.h",
            "preferences.h",
            "processresult.h",
            "profile.h",
            "projectgeneratormanager.h",
            "qbs_export.h",
            "settings.h",
            "settingsmodel.h",
            "settingsrepresentation.h",
            "setupprojectparameters.h",
            "toolchains.h",
            "version.h",
        ]
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/tools"
    }
    Group {
        condition: qbs.targetOS.contains("macos")
        name: "tools (macOS)"
        prefix: "tools/"
        files: [
            "applecodesignutils.cpp",
            "applecodesignutils.h"
        ]
    }
    Group {
        name: "use_installed.pri"
        files: [
            "use_installed_corelib.pri",
            "../../../qbs_version.pri"
        ]
        qbs.install: qbsbuildconfig.installApiHeaders
        qbs.installDir: headerInstallPrefix
    }
    Export {
        Depends { name: "cpp" }
        cpp.defines: base.concat(product.projectFileUpdateDefines)
    }
}
