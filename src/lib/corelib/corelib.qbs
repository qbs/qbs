import qbs.Utilities

QbsLibrary {
    Depends { name: "cpp" }
    Depends { name: "Qt"; submodules: ["core-private", "network", "xml"] }
    Depends {
        name: "Qt.core5compat";
        condition: Utilities.versionCompare(Qt.core.version, "6.0.0") >= 0
    }
    Depends { name: "quickjs" }
    Depends { name: "qbspkgconfig" }
    name: "qbscore"
    cpp.includePaths: base.concat([
        ".",
        "../.." // for the plugin headers
    ])
    cpp.defines: {
        var defines = base.concat([
            "QBS_RELATIVE_LIBEXEC_PATH=" + Utilities.cStringQuote(qbsbuildconfig.relativeLibexecPath),
            "QBS_VERSION=" + Utilities.cStringQuote(version),
        ]);
        if (project.withTests)
            defines.push("QBS_WITH_TESTS");
        if (qbsbuildconfig.enableUnitTests)
            defines.push("QBS_ENABLE_UNIT_TESTS");
        if (qbsbuildconfig.systemSettingsDir)
            defines.push('QBS_SYSTEM_SETTINGS_DIR="' + qbsbuildconfig.systemSettingsDir + '"');
        return defines;
    }

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
            "host.cpp",
            "jsextension.h",
            "jsextensions.cpp",
            "jsextensions.h",
            "moduleproperties.cpp",
            "moduleproperties.h",
            "pkgconfigjs.cpp",
            "pkgconfigjs.h",
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
            "propertylist_darwin.mm",
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
            "asttools.cpp",
            "asttools.h",
            "builtindeclarations.cpp",
            "builtindeclarations.h",
            "deprecationinfo.h",
            "evaluator.cpp",
            "evaluator.h",
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
            "itemtype.h",
            "jsimports.h",
            "language.cpp",
            "language.h",
            "moduleproviderinfo.h",
            "preparescriptobserver.cpp",
            "preparescriptobserver.h",
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
        name: "loader"
        prefix: name + '/'
        files: [
            "astimportshandler.cpp",
            "astimportshandler.h",
            "astpropertiesitemhandler.cpp",
            "astpropertiesitemhandler.h",
            "dependenciesresolver.cpp",
            "dependenciesresolver.h",
            "groupshandler.cpp",
            "groupshandler.h",
            "itemreader.cpp",
            "itemreader.h",
            "itemreaderastvisitor.cpp",
            "itemreaderastvisitor.h",
            "itemreadervisitorstate.cpp",
            "itemreadervisitorstate.h",
            "loaderutils.cpp",
            "loaderutils.h",
            "localprofiles.cpp",
            "localprofiles.h",
            "moduleinstantiator.cpp",
            "moduleinstantiator.h",
            "moduleloader.cpp",
            "moduleloader.h",
            "modulepropertymerger.cpp",
            "modulepropertymerger.h",
            "moduleproviderloader.cpp",
            "moduleproviderloader.h",
            "probesresolver.cpp",
            "probesresolver.h",
            "productitemmultiplexer.cpp",
            "productitemmultiplexer.h",
            "productresolver.cpp",
            "productresolver.h",
            "productscollector.cpp",
            "productscollector.h",
            "productsresolver.cpp",
            "productsresolver.h",
            "projectresolver.cpp",
            "projectresolver.h",
        ]
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
            "qmljs.g",
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
            "deprecationwarningmode.cpp",
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
            "pimpl.h",
            "persistence.cpp",
            "persistence.h",
            "porting.h",
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
            "propagate_const.h",
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
            "deprecationwarningmode.h",
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
}
