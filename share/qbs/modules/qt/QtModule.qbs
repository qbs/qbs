import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import 'qtfunctions.js' as QtFunctions

Module {
    condition: false

    Depends { name: "cpp" }
    Depends { name: "Qt.core" }

    property string qtModuleName
    property string binPath: qt.core.binPath
    property string incPath: {
        var result;
        if (qt.core.isInstalled || qt.core.versionMajor < 5) {
            result = qt.core.incPath;
        } else {
            result = FileInfo.joinPaths(
                        FileInfo.path(FileInfo.path(qt.core.incPath)),
                        repository, "include");
        }
        return result;
    }
    property string libPath: qt.core.libPath
    property string qtLibInfix: qt.core.libInfix
    property string repository: qt.core.versionMajor === 5 ? 'qtbase' : undefined
    property string internalQtModuleName: 'Qt' + qtModuleName
    property string internalLibraryName: QtFunctions.getLibraryName(internalQtModuleName + qtLibInfix, qt.core.versionMajor, qbs.targetOS, cpp.debugInformation)

    Properties {
        condition: qtModuleName != undefined

        cpp.includePaths: {
            var paths = [incPath, FileInfo.joinPaths(incPath, internalQtModuleName)];
            if (qbs.targetOS === "mac")
                paths.unshift(libPath + '/' + internalQtModuleName + qtLibInfix + '.framework/Versions/' + qt.core.versionMajor + '/Headers');
            return paths;
        }

        cpp.dynamicLibraries: qbs.targetOS !== 'mac' ? [internalLibraryName] : undefined
        cpp.frameworks: qbs.targetOS === 'mac' ? [internalLibraryName] : undefined
        cpp.defines: [ "QT_" + qtModuleName.toUpperCase() + "_LIB" ]
    }
}
