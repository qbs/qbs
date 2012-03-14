import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import '../../utils.js' as ModUtils
import "moc.js" 1.0 as Moc
import '../qtfunctions.js' as QtFunctions

Module {
    Depends { name: "cpp" }

    property string qtVersionName: qbs.configurationValue("defaults/qtVersionName", "default")
    property string configKey: "qt/" + qtVersionName + "/"
    property string qtNamespace: qbs.configurationValue(configKey + "namespace", undefined)
    property string qtLibInfix: qbs.configurationValue(configKey + "libInfix", "")
    property string qtPath: qbs.configurationValue(configKey + "path", undefined)
    property string binPath: qtPath ? qtPath + "/bin" : qbs.configurationValue(configKey + "binPath", undefined)
    property string incPath: qtPath ? qtPath + "/include" : qbs.configurationValue(configKey + "incPath", undefined)
    property string libPath: qtPath ? qtPath + "/lib" : qbs.configurationValue(configKey + "libPath", undefined)
    property string version: qbs.configurationValue(configKey + "version", "4.7.0")
    property var versionParts: version.split('.').map(parseInt)
    property var versionMajor: versionParts[0]
    property var versionMinor: versionParts[1]
    property var versionPatch: versionParts[2]
    property string mkspecsPath: qtPath ? qtPath + "/mkspecs" : qbs.configurationValue(configKey + "mkspecsPath", undefined)
    property string generatedFilesDir: 'GeneratedFiles/' + product.name // ### TODO: changing this property does not change the path in the rule ATM.
    property string libraryInfix: cpp.debugInformation ? 'd' : ''
    cpp.defines: {
        if (!qtNamespace)
            return undefined;
        return ["QT_NAMESPACE=" + qtNamespace]
    }
    cpp.includePaths: [
        mkspecsPath + '/default',
        incPath + '/QtCore',
        incPath,
        product.buildDirectory + '/' + generatedFilesDir
    ]
    cpp.libraryPaths: [libPath]
    cpp.dynamicLibraries: qbs.targetOS !== 'mac' ? [QtFunctions.getLibraryName('QtCore' + qtLibInfix, versionMajor, qbs.targetOS, cpp.debugInformation)] : undefined
    cpp.frameworkPaths: qbs.targetOS === 'mac' ? [libPath] : undefined
    cpp.frameworks: qbs.targetOS === 'mac' ? [QtFunctions.getLibraryName('QtCore' + qtLibInfix, versionMajor, qbs.targetOS, cpp.debugInformation)] : undefined
    cpp.rpaths: qbs.targetOS === 'linux' ? [libPath] : undefined
    cpp.positionIndependentCode: versionMajor >= 5 ? true : undefined

    setupBuildEnvironment: {
        // Not really a setup in this case. Just some sanity checks.
        if (!binPath)
            throw "qt/core.binPath not set. Set the configuration values qt/default/binPath or qt/default/path.";
        if (!incPath)
            throw "qt/core.incPath not set. Set the configuration values qt/default/incPath or qt/default/path.";
        if (!libPath)
            throw "qt/core.libPath not set. Set the configuration values qt/default/libPath or qt/default/path.";
        if (!mkspecsPath)
            throw "qt/core.mkspecsPath not set. Set the configuration values qt/default/mkspecsPath or qt/default/path.";
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
            var cmd = new Command(product.module.binPath + '/moc', Moc.args(input.fileName, output.fileName, input));
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
            var cmd = new Command(product.module.binPath + '/moc', Moc.args(input.fileName, output.fileName, input));
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
            var cmd = new Command(product.module.binPath + '/moc', Moc.args(input.fileName, output.fileName, input));
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
