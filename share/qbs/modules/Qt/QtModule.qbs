import qbs 1.0
import qbs.FileInfo
import 'qtfunctions.js' as QtFunctions

Module {
    Depends { name: "cpp" }
    Depends { name: "Qt.core" }

    property string qtModuleName
    property string binPath: Qt.core.binPath
    property string incPath: Qt.core.incPath
    property string libPath: Qt.core.libPath
    property string qtLibInfix: Qt.core.libInfix
    property string repository: Qt.core.versionMajor === 5 ? 'qtbase' : undefined
    property string includeDirName: 'Qt' + qtModuleName
    property string internalLibraryName: QtFunctions.getQtLibraryName(qtModuleName + qtLibInfix, Qt.core, qbs)
    property string qtVersion: Qt.core.version

    Properties {
        condition: qtModuleName != undefined

        cpp.includePaths: {
            var modulePath = FileInfo.joinPaths(incPath, includeDirName);
            var paths = [incPath, modulePath];
            if (Qt.core.versionMajor >= 5)
                paths.unshift(FileInfo.joinPaths(modulePath, qtVersion, includeDirName));
            if (Qt.core.frameworkBuild)
                paths.unshift(libPath + '/' + includeDirName + qtLibInfix + '.framework/Versions/' + Qt.core.versionMajor + '/Headers');
            return paths;
        }

        cpp.dynamicLibraries: Qt.core.frameworkBuild ? undefined : [internalLibraryName]
        cpp.frameworks: Qt.core.frameworkBuild ? [internalLibraryName] : undefined
        cpp.defines: [ "QT_" + qtModuleName.toUpperCase() + "_LIB" ]
    }
}
