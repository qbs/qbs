import qbs 1.0
import "../QtModule.qbs" as QtModule
import "../qtfunctions.js" as QtFunctions

QtModule {
    qtModuleName: "Enginio"
    includeDirName: qtModuleName
    internalLibraryName: QtFunctions.getPlatformLibraryName(qtModuleName, Qt.core, qbs)
    repository: "enginio-qt"
    Depends { name: "Qt"; submodules: ["network", "gui"] }
}

