import qbs 1.0

DynamicLibrary {
    Depends { name: "cpp" }
    Depends { name: "qt"; submodules: ["core", "script", "test"] }
    Depends { condition: qt.core.versionMajor >= 5; name: "qt.concurrent" }
    name: "qbscore"
    targetName: (qbs.enableDebugCode && qbs.targetOS === "windows") ? (name + 'd') : name
    destinationDirectory: qbs.targetOS == "windows" ? "bin" : "lib"
    cpp.treatWarningsAsErrors: true
    cpp.includePaths: [
        ".",
        ".."     // for the plugin headers
    ]
    cpp.defines: [
        "QBS_VERSION=\"" + project.version + "\"",
        "QT_CREATOR", "QML_BUILD_STATIC_LIB",   // needed for QmlJS
        "QBS_LIBRARY", "SRCDIR=\"" + path + "\""
    ]
    Properties {
        condition: qbs.toolchain === "msvc"
        cpp.cxxFlags: ["/WX"]
    }
    Properties {
        condition: qbs.toolchain === "gcc" && qbs.targetPlatform.indexOf("windows") === -1
        cpp.cxxFlags: ["-Werror"]
    }
    Group {
        name: product.name
        files: ["qbs.h"]
    }
    Group {
        name: "api"
        prefix: name + '/'
        files: [
            "internaljobs.cpp",
            "internaljobs.h",
            "jobs.cpp",
            "jobs.h",
            "project.cpp",
            "project.h",
            "projectdata.cpp",
            "projectdata.h",
            "propertymap_p.h",
            "runenvironment.cpp",
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
            "buildproduct.cpp",
            "buildproduct.h",
            "buildproject.cpp",
            "buildproject.h",
            "command.cpp",
            "command.h",
            "cycledetector.cpp",
            "cycledetector.h",
            "executor.cpp",
            "executor.h",
            "executorjob.cpp",
            "executorjob.h",
            "forward_decls.h",
            "inputartifactscanner.cpp",
            "inputartifactscanner.h",
            "jscommandexecutor.cpp",
            "jscommandexecutor.h",
            "processcommandexecutor.cpp",
            "processcommandexecutor.h",
            "productinstaller.cpp",
            "productinstaller.h",
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
        name: "jsextensions"
        prefix: name + '/'
        files: [
            "file.cpp",
            "file.h",
            "moduleproperties.cpp",
            "moduleproperties.h",
            "process.cpp",
            "process.h",
            "textfile.cpp",
            "textfile.h"
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
            "forward_decls.h",
            "forward_decls.h",
            "functiondeclaration.h",
            "identifiersearch.cpp",
            "identifiersearch.h",
            "importversion.cpp",
            "importversion.h",
            "item.cpp",
            "item.h",
            "itemobserver.h",
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
            "propertydeclaration.cpp",
            "propertydeclaration.h",
            "scriptengine.cpp",
            "scriptengine.h",
            "value.cpp",
            "value.h"
        ]
    }
    Group {
        name: "logging"
        prefix: name + '/'
        files: [
            "ilogsink.cpp",
            "ilogsink.h",
            "logger.cpp",
            "logger.h",
            "translator.h"
        ]
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
            "buildoptions.h",
            "cleanoptions.cpp",
            "cleanoptions.h",
            "codelocation.cpp",
            "codelocation.h",
            "error.cpp",
            "error.h",
            "fileinfo.cpp",
            "fileinfo.h",
            "filetime.h",
            "hostosinfo.h",
            "id.cpp",
            "id.h",
            "installoptions.cpp",
            "installoptions.h",
            "persistence.cpp",
            "persistence.h",
            "persistentobject.h",
            "preferences.cpp",
            "preferences.h",
            "processresult.h",
            "profile.cpp",
            "profile.h",
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
            "settings.h",
            "setupprojectparameters.cpp",
            "setupprojectparameters.h",
            "weakpointer.h"
        ]
    }
    Group {
        condition: qbs.targetPlatform.indexOf("windows") != -1
        name: "tools (Windows)"
        prefix: "tools/"
        files: [
            "filetime_win.cpp"
        ]
    }
    Group {
        condition: qbs.targetPlatform.indexOf("unix") != -1
        name: "tools (Unix)"
        prefix: "tools/"
        files: [
            "filetime_unix.cpp"
        ]
    }
    Group {
        condition: project.enableTests
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
        qbs.installDir: qbs.targetOS === "windows" ? "bin" : "lib"
    }
    ProductModule {
        Depends { name: "cpp" }
        Depends { name: "qt"; submodules: ["script"]}
        cpp.includePaths: ["."]
    }
}

