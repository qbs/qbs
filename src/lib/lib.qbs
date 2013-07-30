import qbs 1.0
import "../../version.js" as Version

Product {
    Depends { name: "cpp" }
    Depends { name: "Qt"; submodules: ["core", "script", "xml"] }
    Depends { condition: project.enableUnitTests; name: "Qt.test" }
    name: "qbscore"
    type: Qt.core.staticBuild ? "staticlibrary" : "dynamiclibrary"
    targetName: (qbs.enableDebugCode && qbs.targetOS.contains("windows")) ? (name + 'd') : name
    destinationDirectory: qbs.targetOS.contains("windows") ? "bin" : "lib"
    cpp.treatWarningsAsErrors: true
    cpp.includePaths: [
        ".",
        ".."     // for the plugin headers
    ]
    cpp.defines: [
        "QBS_VERSION=\"" + Version.qbsVersion() + "\"",
        "QT_CREATOR", "QML_BUILD_STATIC_LIB",   // needed for QmlJS
        "SRCDIR=\"" + path + "\""
    ].concat(type == "staticlibrary" ? ["QBS_STATIC_LIB"] : ["QBS_LIBRARY"])
    Properties {
        condition: qbs.toolchain.contains("msvc")
        cpp.cxxFlags: ["/WX"]
    }
    Properties {
        condition: qbs.toolchain.contains("gcc") && !qbs.targetOS.contains("windows")
        cpp.cxxFlags: ["-Werror"]
    }
    cpp.installNamePrefix: "@rpath/"

    property string headerInstallPrefix: "/include/qbs"
    Group {
        name: product.name
        files: ["qbs.h"]
        qbs.install: project.installApiHeaders
        qbs.installDir: headerInstallPrefix
    }
    Group {
        name: "api"
        prefix: name + '/'
        files: [
            "internaljobs.cpp",
            "internaljobs.h",
            "jobs.cpp",
            "project.cpp",
            "projectdata.cpp",
            "projectdata_p.h",
            "propertymap_p.h",
            "runenvironment.cpp",
        ]
    }
    Group {
        name: "public api headers"
        qbs.install: project.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/api"
        prefix: "api/"
        files: [
            "jobs.h",
            "project.h",
            "projectdata.h",
            "runenvironment.h"
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
            "artifactlist.cpp",
            "artifactlist.h",
            "artifactvisitor.cpp",
            "artifactvisitor.h",
            "automoc.cpp",
            "automoc.h",
            "buildgraph.cpp",
            "buildgraph.h",
            "buildgraphloader.cpp",
            "buildgraphloader.h",
            "command.cpp",
            "command.h",
            "cycledetector.cpp",
            "cycledetector.h",
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
            "processcommandexecutor.cpp",
            "processcommandexecutor.h",
            "productbuilddata.cpp",
            "productbuilddata.h",
            "productinstaller.cpp",
            "productinstaller.h",
            "projectbuilddata.cpp",
            "projectbuilddata.h",
            "rulegraph.cpp",
            "rulegraph.h",
            "rulesapplicator.cpp",
            "rulesapplicator.h",
            "rulesevaluationcontext.cpp",
            "rulesevaluationcontext.h",
            "scanresultcache.cpp",
            "scanresultcache.h",
            "timestampsupdater.cpp",
            "timestampsupdater.h",
            "transformer.cpp",
            "transformer.h"
        ]
    }
    Group {
        name: "public buildgraph headers"
        qbs.install: project.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/buildgraph"
        files: "buildgraph/forward_decls.h"
    }
    Group {
        name: "jsextensions"
        prefix: name + '/'
        files: [
            "file.cpp",
            "file.h",
            "jsextensions.cpp",
            "jsextensions.h",
            "moduleproperties.cpp",
            "moduleproperties.h",
            "process.cpp",
            "process.h",
            "textfile.cpp",
            "textfile.h",
            "domxml.cpp",
            "domxml.h"
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
            "builtinvalue.cpp",
            "builtinvalue.h",
            "evaluationdata.h",
            "evaluator.cpp",
            "evaluator.h",
            "evaluatorscriptclass.cpp",
            "evaluatorscriptclass.h",
            "filecontext.cpp",
            "filecontext.h",
            "filetags.cpp",
            "filetags.h",
            "functiondeclaration.h",
            "identifiersearch.cpp",
            "identifiersearch.h",
            "importversion.cpp",
            "importversion.h",
            "item.cpp",
            "item.h",
            "itemobserver.h",
            "itempool.cpp",
            "itempool.h",
            "itemreader.cpp",
            "itemreader.h",
            "itemreaderastvisitor.cpp",
            "itemreaderastvisitor.h",
            "jsimports.h",
            "language.cpp",
            "language.h",
            "loader.cpp",
            "loader.h",
            "moduleloader.cpp",
            "moduleloader.h",
            "projectresolver.cpp",
            "projectresolver.h",
            "property.h",
            "propertydeclaration.cpp",
            "propertydeclaration.h",
            "propertymapinternal.cpp",
            "propertymapinternal.h",
            "scriptengine.cpp",
            "scriptengine.h",
            "scriptpropertyobserver.h",
            "value.cpp",
            "value.h"
        ]
    }
    Group {
        name: "public language headers"
        qbs.install: project.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/language"
        files: "language/forward_decls.h"
    }
    Group {
        name: "logging"
        prefix: name + '/'
        files: [
            "ilogsink.cpp",
            "logger.cpp",
            "logger.h",
            "translator.h"
        ]
    }
    Group {
        name: "public logging headers"
        qbs.install: project.installApiHeaders
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
            "buildoptions.cpp",
            "cleanoptions.cpp",
            "codelocation.cpp",
            "error.cpp",
            "fileinfo.cpp",
            "fileinfo.h",
            "filetime.h",
            "hostosinfo.h",
            "id.cpp",
            "id.h",
            "installoptions.cpp",
            "persistence.cpp",
            "persistence.h",
            "persistentobject.h",
            "preferences.cpp",
            "processresult.cpp",
            "processresult_p.h",
            "profile.cpp",
            "progressobserver.cpp",
            "progressobserver.h",
            "propertyfinder.cpp",
            "propertyfinder.h",
            "qbs_export.h",
            "qbsassert.cpp",
            "qbsassert.h",
            "qttools.cpp",
            "qttools.h",
            "scannerpluginmanager.cpp",
            "scannerpluginmanager.h",
            "scripttools.cpp",
            "scripttools.h",
            "settings.cpp",
            "setupprojectparameters.cpp",
            "weakpointer.h"
        ]
    }
    Group {
        name: "public tools headers"
        prefix: "tools/"
        files: [
            "buildoptions.h",
            "cleanoptions.h",
            "codelocation.h",
            "error.h",
            "installoptions.h",
            "preferences.h",
            "processresult.h",
            "profile.h",
            "settings.h",
            "setupprojectparameters.h",
        ]
        qbs.install: project.installApiHeaders
        qbs.installDir: headerInstallPrefix + "/tools"
    }
    Group {
        condition: qbs.targetOS.contains("windows")
        name: "tools (Windows)"
        prefix: "tools/"
        files: [
            "filetime_win.cpp"
        ]
    }
    Group {
        condition: qbs.targetOS.contains("unix")
        name: "tools (Unix)"
        prefix: "tools/"
        files: [
            "filetime_unix.cpp"
        ]
    }
    Group {
        condition: project.enableUnitTests
        name: "tests"
        files: [
            "buildgraph/tst_buildgraph.cpp",
            "buildgraph/tst_buildgraph.h",
            "language/tst_language.cpp",
            "language/tst_language.h",
            "tools/tst_tools.h",
            "tools/tst_tools.cpp"
        ]
    }
    Group {
        fileTagsFilter: "dynamiclibrary"
        qbs.install: true
        qbs.installDir: project.libInstallDir
    }
    Export {
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["script", "xml"] }
        cpp.rpaths: project.libRPaths
        cpp.includePaths: path
        cpp.defines: product.type === "staticlibrary" ? ["QBS_STATIC_LIB"] : []
    }
}
