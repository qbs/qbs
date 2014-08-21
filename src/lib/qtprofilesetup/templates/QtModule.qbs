import qbs 1.0
import qbs.FileInfo

Module {
    Depends { name: "cpp" }
    Depends { name: "Qt.core" }

    property string qtModuleName
    property path binPath: Qt.core.binPath
    property path incPath: Qt.core.incPath
    property path libPath: Qt.core.libPath
    property string qtLibInfix: Qt.core.libInfix
    property string libNameForLinkerDebug
    property string libNameForLinkerRelease
    property string libNameForLinker: qbs.buildVariant === "debug"
                                      ? libNameForLinkerDebug : libNameForLinkerRelease
    property string libFilePathDebug
    property string libFilePathRelease
    property string libFilePath: qbs.buildVariant === "debug"
                                 ? libFilePathDebug : libFilePathRelease
    property string qtVersion: Qt.core.version
    property bool hasLibrary: true
    property bool isStaticLibrary: false
    property bool isPlugin: false

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
        cpp.staticLibraries: (isStaticLibrary ? [libFilePath] : []).concat(staticLibs)
        cpp.dynamicLibraries: (!isStaticLibrary && !Qt.core.frameworkBuild
                               ? [libFilePath] : []).concat(dynamicLibs)
        cpp.frameworks: mFrameworks.concat(!isStaticLibrary && Qt.core.frameworkBuild
                        ? [libNameForLinker] : [])
        cpp.frameworkPaths: mFrameworkPaths
    }
}
