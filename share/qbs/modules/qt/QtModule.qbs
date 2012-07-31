import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import 'qtfunctions.js' as QtFunctions

Module {
    condition: false

    Depends { name: "cpp" }
    Depends { name: "Qt.core" }

    property string qtModuleName
    property string binPath: qt.core.binPath
    property string incPath: qt.core.incPath
    property string libPath: qt.core.libPath
    property string qtLibInfix: qt.core.libInfix
    property string repository: qt.core.versionMajor === 5 ? 'qtbase' : undefined
    property string internalQtModuleName: 'Qt' + qtModuleName
    property string internalLibraryName: QtFunctions.getLibraryName(internalQtModuleName + qtLibInfix, qt.core.versionMajor, qbs.targetOS, cpp.debugInformation)
    property string qtVersion: qt.core.version

    Properties {
        condition: qtModuleName != undefined

        cpp.includePaths: {
            var modulePath = FileInfo.joinPaths(incPath, internalQtModuleName);
            var paths = [incPath, modulePath];
            if (qt.core.versionMajor >= 5)
                paths.unshift(FileInfo.joinPaths(modulePath, qtVersion, internalQtModuleName));
            if (qbs.targetOS === "mac")
                paths.unshift(libPath + '/' + internalQtModuleName + qtLibInfix + '.framework/Versions/' + qt.core.versionMajor + '/Headers');
            return paths;
        }

        cpp.dynamicLibraries: qbs.targetOS !== 'mac' ? [internalLibraryName] : undefined
        cpp.frameworks: qbs.targetOS === 'mac' ? [internalLibraryName] : undefined
        cpp.defines: [ "QT_" + qtModuleName.toUpperCase() + "_LIB" ]
    }
}
