import qbs 1.0
import qbs.FileInfo
import "../QtModule.qbs" as QtModule
import "../qtfunctions.js" as QtFunctions

QtModule {
    qtModuleName: "AxServer"
    repository: "qtactiveqt"
    includeDirName: "ActiveQt"
    cpp.dynamicLibraries: [
        internalLibraryName,
        QtFunctions.getQtLibraryName("AxBase" + qtLibInfix, Qt.core, qbs),
        "Ole32.lib", "OleAut32.lib", "Gdi32.lib", "User32.lib"
    ]
}
