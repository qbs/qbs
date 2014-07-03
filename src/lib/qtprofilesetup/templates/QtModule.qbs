import qbs 1.0
import qbs.FileInfo
import 'qtfunctions.js' as QtFunctions

Module {
    Depends { name: "cpp" }
    Depends { name: "Qt.core" }

    property string qtModuleName
    property string qtModulePrefix: 'Qt'
    property path binPath: Qt.core.binPath
    property path incPath: Qt.core.incPath
    property path libPath: Qt.core.libPath
    property string qtLibInfix: Qt.core.libInfix
    property string includeDirName: qtModulePrefix + qtModuleName
    property string internalLibraryName: QtFunctions.getQtLibraryName(qtModuleName + qtLibInfix,
                                                                      Qt.core, qbs, isStaticLibrary,
                                                                      qtModulePrefix)
    property string qtVersion: Qt.core.version
    property bool hasLibrary: true
    property bool isStaticLibrary: false

    property stringList staticLibsDebug
    property stringList staticLibsRelease
    property stringList dynamicLibsDebug
    property stringList dynamicLibsRelease
    property stringList linkerFlagsDebug
    property stringList linkerFlagsRelease
    property stringList staticLibs: qbs.buildVariant === "debug"
                                    ? staticLibsDebug : staticLibsRelease
    property stringList dynamicLibs: qbs.buildVariant === "debug"
                                    ? dynamicLibsDebug : dynamicLibsRelease
    property stringList frameworksDebug
    property stringList frameworksRelease
    property stringList frameworkPathsDebug
    property stringList frameworkPathsRelease
    property stringList mFrameworks: qbs.buildVariant === "debug"
            ? frameworksDebug : frameworksRelease
    property stringList mFrameworkPaths: qbs.buildVariant === "debug"
            ? frameworkPathsDebug: frameworkPathsRelease
    cpp.linkerFlags: qbs.buildVariant === "debug"
            ? linkerFlagsDebug : linkerFlagsRelease

    Properties {
        condition: qtModuleName != undefined && hasLibrary
        cpp.staticLibraries: staticLibs.concat(isStaticLibrary ? [internalLibraryName] : [])
        cpp.dynamicLibraries: dynamicLibs.concat(!isStaticLibrary && !Qt.core.frameworkBuild
                              ? [internalLibraryName] : [])
        cpp.frameworks: mFrameworks.concat(!isStaticLibrary && Qt.core.frameworkBuild
                        ? [internalLibraryName] : [])
        cpp.frameworkPaths: mFrameworkPaths
    }
}
