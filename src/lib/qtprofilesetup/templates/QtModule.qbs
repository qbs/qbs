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

    Properties {
        condition: qtModuleName != undefined
        cpp.defines: qtModuleName.contains("private")
                     ? [] : [ "QT_" + qtModuleName.toUpperCase() + "_LIB" ]
    }

    Properties {
        condition: qtModuleName != undefined && hasLibrary
        cpp.staticLibraries: isStaticLibrary
                             ? [internalLibraryName] : undefined
        cpp.dynamicLibraries: !isStaticLibrary && !Qt.core.frameworkBuild
                              ? [internalLibraryName] : undefined
        cpp.frameworks: !isStaticLibrary && Qt.core.frameworkBuild
                        ? [internalLibraryName] : undefined
    }
}
