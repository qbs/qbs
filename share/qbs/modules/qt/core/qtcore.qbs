import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import '../../utils.js' as ModUtils
import "moc.js" 1.0 as Moc
import '../qtfunctions.js' as QtFunctions

Module {
    Depends { name: "cpp" }

    property string namespace
    property string libInfix: ""
    property string path
    property string repository: versionMajor === 5 ? "qtbase" : undefined
    property string binPath: path ? FileInfo.joinPaths(path, repoSubDir, "bin") : undefined
    property string incPath: path ? FileInfo.joinPaths(path, repoSubDir, "include") : undefined
    property string libPath: path ? FileInfo.joinPaths(path, repoSubDir, "lib") : undefined
    property string version: "4.7.0"
    property var versionParts: version.split('.').map(function(item) { return parseInt(item, 10); })
    property var versionMajor: versionParts[0]
    property var versionMinor: versionParts[1]
    property var versionPatch: versionParts[2]
    property string mkspecsPath: path ? FileInfo.joinPaths(path, 'qtbase',  "mkspecs") : undefined
    property string generatedFilesDir: 'GeneratedFiles/' + product.name // ### TODO: changing this property does not change the path in the rule ATM.
    property bool isInstalled

    // private properties
    property string libraryInfix: cpp.debugInformation ? 'd' : ''
    property string repoSubDir: isInstalled ? repository : undefined

    cpp.defines: {
        var defines = ["QT_CORE_LIB"];
        // ### QT_NO_DEBUG must be added if the current build variant is derived
        //     from the build variant "release"
        if (!qbs.debugInformation)
            defines.push("QT_NO_DEBUG");
        if (!product.consoleApplication && qbs.toolchain === "mingw")
            defines.push("QT_NEEDS_QMAIN");
        if (namespace)
            defines.push("QT_NAMESPACE=" + namespace);
        return defines;
    }
    cpp.includePaths: {
        var paths = [mkspecsPath + '/default'];
        if (qbs.targetOS === "mac")
            paths.push(libPath + '/QtCore' + libInfix + '.framework/Versions/' + versionMajor + '/Headers');
        paths.push(incPath + '/QtCore');
        paths.push(incPath);
        paths.push(product.buildDirectory + '/' + generatedFilesDir);
        return paths;
    }
    cpp.libraryPaths: [libPath]
    cpp.staticLibraries: {
        if (qbs.targetOS === 'windows' && !product.consoleApplication)
            return ["qtmain" + libInfix + (cpp.debugInformation ? "d" : "") + ".lib"];
    }
    cpp.dynamicLibraries: qbs.targetOS !== 'mac' ? [QtFunctions.getLibraryName('QtCore' + libInfix, versionMajor, qbs.targetOS, cpp.debugInformation)] : undefined
    cpp.frameworkPaths: qbs.targetOS === 'mac' ? [libPath] : undefined
    cpp.frameworks: qbs.targetOS === 'mac' ? [QtFunctions.getLibraryName('QtCore' + libInfix, versionMajor, qbs.targetOS, cpp.debugInformation)] : undefined
    cpp.rpaths: qbs.targetOS === 'linux' ? [libPath] : undefined
    cpp.positionIndependentCode: versionMajor >= 5 ? true : undefined

    setupBuildEnvironment: {
        // Not really a setup in this case. Just some sanity checks.
        if (!binPath)
            throw "qt.core.binPath not set. Set qt.core.binPath or qt.core.path in your profile.";
        if (!incPath)
            throw "qt.core.incPath not set. Set qt.core.incPath or qt.core.path in your profile.";
        if (!libPath)
            throw "qt.core.libPath not set. Set qt.core.libPath or qt.core.path in your profile.";
        if (!mkspecsPath)
            throw "qt.core.mkspecsPath not set. Set qt.core.mkspecsPath or qt.core.path in your profile.";
    }

    setupRunEnvironment: {
        var v = getenv('PATH') || ''
        if (v.length > 0 && v.charAt(0) != ';')
            v = ';' + v
        var y = binPath
        if (qbs.targetOS === 'windows')
            v = FileInfo.toWindowsSeparators(y) + v
        else
            v = y + v
        putenv('PATH', v)
    }

    FileTagger {
        pattern: "*.qrc"
        fileTags: ["qrc"]
    }

    Rule {
        inputs: ["moc_cpp"]

        Artifact {
            fileName: 'GeneratedFiles/' + product.name + '/' + input.baseName + '.moc'
//            fileName: input.baseDir + '/' + input.baseName + '.moc'
            fileTags: ["hpp"]
        }

        prepare: {
            var cmd = new Command(product.module.binPath + '/moc', Moc.args(product, input.fileName, output.fileName, input));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["moc_hpp"]

        Artifact {
            fileName: 'GeneratedFiles/' + product.name + '/moc_' + input.baseName + '.cpp'
            fileTags: [ "cpp" ]
        }

        prepare: {
            var cmd = new Command(product.module.binPath + '/moc', Moc.args(product, input.fileName, output.fileName, input));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["moc_hpp_inc"]

        Artifact {
            fileName: 'GeneratedFiles/' + product.name + '/moc_' + input.baseName + '.cpp'
            fileTags: [ "hpp" ]
        }

        prepare: {
            var cmd = new Command(product.module.binPath + '/moc', Moc.args(product, input.fileName, output.fileName, input));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["qrc"]

        Artifact {
//  ### TODO we want to access the module's property "generatedFilesDir" here. But without evaluating all available properties a priori.
            fileName: 'GeneratedFiles/' + product.name + '/qrc_' + input.baseName + '.cpp'
            fileTags: ["cpp"]
        }
        prepare: {
            var cmd = new Command(product.module.binPath + '/rcc', [input.fileName, '-name', FileInfo.baseName(input.fileName), '-o', output.fileName]);
            cmd.description = 'rcc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }
}
