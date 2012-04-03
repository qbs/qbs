import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import 'qtfunctions.js' as QtFunctions

Module {
    condition: false

    Depends { name: "cpp" }
    Depends { id: qtcore; name: "Qt.core" }

    property string qtModuleName
    property string binPath: qtcore.binPath
    property string incPath: qtcore.path ? FileInfo.joinPaths(qtcore.path, repository, "include") : qtcore.incPath
    property string libPath: qtcore.libPath
    property string qtLibInfix: qtcore.libInfix
    property string repository: qtcore.versionMajor === 5 ? 'qtbase' : undefined
    property string internalQtModuleName: 'Qt' + qtModuleName
    property string internalLibraryName: QtFunctions.getLibraryName(internalQtModuleName + qtLibInfix, qtcore.versionMajor, qbs.targetOS, cpp.debugInformation)

    Properties {
        condition: qtModuleName != undefined

        cpp.includePaths: {
            var paths = [incPath, FileInfo.joinPaths(incPath, internalQtModuleName)];
            if (qbs.targetOS === "mac")
                paths.unshift(libPath + '/' + internalQtModuleName + qtLibInfix + '.framework/Versions/' + qtcore.versionMajor + '/Headers');
            return paths;
        }

        cpp.dynamicLibraries: qbs.targetOS !== 'mac' ? [internalLibraryName] : undefined
        cpp.frameworks: qbs.targetOS === 'mac' ? [internalLibraryName] : undefined
        cpp.defines: [ "QT_" + qtModuleName.toUpperCase() + "_LIB" ]
    }
}
