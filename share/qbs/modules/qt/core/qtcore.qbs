import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import '../../utils.js' as ModUtils
import "moc.js" 1.0 as Moc
import '../qtfunctions.js' as QtFunctions

Module {
    id: qtcore

    Depends { name: "cpp" }

    property string namespace
    property string libInfix: ""
    property string repository: versionMajor === 5 ? "qtbase" : undefined
    property string binPath
    property string incPath
    property string libPath
    property string mkspecPath
    property string mocName: "moc"
    property string lreleaseName: "lrelease"
    property string version: "4.7.0"
    property var versionParts: version.split('.').map(function(item) { return parseInt(item, 10); })
    property var versionMajor: versionParts[0]
    property var versionMinor: versionParts[1]
    property var versionPatch: versionParts[2]
    property bool frameworkBuild: qbs.targetOS === 'mac'
    property string generatedFilesDir: 'GeneratedFiles/' + product.name // ### TODO: changing this property does not change the path in the rule ATM.
    property string qmFilesDir: {
        if (qbs.targetOS === "mac" && product.type.indexOf('applicationbundle') >= 0)
            return product.name + ".app/Contents/Resources";
        return product.destination;
    }

    // private properties
    property string libraryInfix: cpp.debugInformation ? 'd' : ''

    cpp.defines: {
        var defines = ["QT_CORE_LIB"];
        // ### QT_NO_DEBUG must be added if the current build variant is derived
        //     from the build variant "release"
        if (!qbs.debugInformation)
            defines.push("QT_NO_DEBUG");
        if (namespace)
            defines.push("QT_NAMESPACE=" + namespace);
        return defines;
    }
    cpp.includePaths: {
        var paths = [mkspecPath];
        if (frameworkBuild)
            paths.push(libPath + '/QtCore' + libInfix + '.framework/Versions/' + versionMajor + '/Headers');
        paths.push(incPath + '/QtCore');
        paths.push(incPath);
        paths.push(product.buildDirectory + '/' + generatedFilesDir);
        return paths;
    }
    cpp.libraryPaths: [libPath]
    cpp.staticLibraries: {
        if (qbs.targetOS === 'windows' && !product.consoleApplication)
            return ["qtmain" + libInfix + (cpp.debugInformation ? "d" : "") + (qbs.toolchain !== "mingw" ? ".lib" : "")];
    }
    cpp.dynamicLibraries: frameworkBuild ? undefined : [QtFunctions.getLibraryName('Core' + libInfix, qtcore, qbs)]
    cpp.frameworkPaths: frameworkBuild ? [libPath] : undefined
    cpp.frameworks: frameworkBuild ? [QtFunctions.getLibraryName('Core' + libInfix, qtcore, qbs)] : undefined
    cpp.rpaths: qbs.targetOS === 'linux' ? [libPath] : undefined
    cpp.positionIndependentCode: versionMajor >= 5 ? true : undefined
    cpp.cxxFlags: {
        var flags;
        if (qbs.toolchain === 'msvc') {
            flags = ['/Zm200'];
            if (versionMajor < 5)
                flags.push('/Zc:wchar_t-');
        }
        return flags;
    }

    additionalProductFileTags: ["qm"]

    setupBuildEnvironment: {
        // Not really a setup in this case. Just some sanity checks.
        if (!binPath)
            throw "qt.core.binPath not set. Set qt.core.binPath in your profile.";
        if (!incPath)
            throw "qt.core.incPath not set. Set qt.core.incPath in your profile.";
        if (!libPath)
            throw "qt.core.libPath not set. Set qt.core.libPath in your profile.";
        if (!mkspecPath)
            throw "qt.core.mkspecPath not set. Set qt.core.mkspecPath in your profile.";
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

    FileTagger {
        pattern: "*.ts"
        fileTags: ["ts"]
    }

    Rule {
        inputs: ["moc_cpp"]

        Artifact {
            fileName: 'GeneratedFiles/' + product.name + '/' + input.completeBaseName + '.moc'
            fileTags: ["hpp"]
        }

        prepare: {
            var cmd = new Command(Moc.fullPath(product.module),
                                  Moc.args(product, input, output.fileName));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["moc_hpp"]

        Artifact {
            fileName: 'GeneratedFiles/' + product.name + '/moc_' + input.completeBaseName + '.cpp'
            fileTags: [ "cpp" ]
        }

        prepare: {
            var cmd = new Command(Moc.fullPath(product.module),
                                  Moc.args(product, input, output.fileName));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["moc_hpp_inc"]

        Artifact {
            fileName: 'GeneratedFiles/' + product.name + '/moc_' + input.completeBaseName + '.cpp'
            fileTags: [ "hpp" ]
        }

        prepare: {
            var cmd = new Command(Moc.fullPath(product.module),
                                  Moc.args(product, input, output.fileName));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["qrc"]

        Artifact {
//  ### TODO we want to access the module's property "generatedFilesDir" here. But without evaluating all available properties a priori.
            fileName: 'GeneratedFiles/' + product.name + '/qrc_' + input.completeBaseName + '.cpp'
            fileTags: ["cpp"]
        }
        prepare: {
            var cmd = new Command(product.module.binPath + '/rcc',
                                  [input.fileName, '-name',
                                   FileInfo.completeBaseName(input.fileName),
                                   '-o', output.fileName]);
            cmd.description = 'rcc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["ts"]

        Artifact {
            fileName: FileInfo.joinPaths(product.module.qmFilesDir, input.completeBaseName + ".qm")
            fileTags: ["qm"]
        }

        prepare: {
            var cmd = new Command(product.module.binPath + '/' + product.module.lreleaseName, ['-silent', input.fileName, '-qm', output.fileName]);
            cmd.description = 'lrelease ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'filegen';
            return cmd;
        }
    }
}
