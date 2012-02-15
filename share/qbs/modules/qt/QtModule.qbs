import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import 'qtfunctions.js' as QtFunctions

Module {
    condition: false

    Depends { name: "cpp" }
    Depends { id: qtcore; name: "Qt.core" }

    property string binPath: qtcore.binPath
    property string incPath: qtcore.incPath
    property string libPath: qtcore.libPath
    property string qtModuleName: ''
    property string internalQtModuleName: 'Qt' + qtModuleName
    cpp.includePaths: [incPath + '/' + internalQtModuleName]
    cpp.dynamicLibraries: [QtFunctions.getLibraryName(internalQtModuleName, qbs.targetOS, cpp.debugInformation)]
    cpp.frameworks: [QtFunctions.getLibraryName(internalQtModuleName, qbs.targetOS, cpp.debugInformation)]
}
