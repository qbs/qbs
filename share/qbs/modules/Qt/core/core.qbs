import qbs 1.0
import qbs.FileInfo
import '../../utils.js' as ModUtils
import "moc.js" as Moc
import '../qtfunctions.js' as QtFunctions

Module {
    id: qtcore

    Depends { name: "cpp" }

    property string namespace
    property string libInfix: ""
    property string repository: versionMajor === 5 ? "qtbase" : undefined
    property stringList config
    property stringList qtConfig
    property path binPath
    property path incPath
    property path libPath
    property path pluginPath
    property path mkspecPath
    property string mocName: "moc"
    property string lreleaseName: "lrelease"
    property string qdocName: versionMajor >= 5 ? "qdoc" : "qdoc3"
    property stringList qdocEnvironment
    property string qdocQhpFileName
    property path docPath
    property stringList helpGeneratorArgs: versionMajor >= 5 ? ["-platform", "minimal"] : []
    property string version
    property var versionParts: version ? version.split('.').map(function(item) { return parseInt(item, 10); }) : []
    property int versionMajor: versionParts[0]
    property int versionMinor: versionParts[1]
    property int versionPatch: versionParts[2]
    property bool frameworkBuild
    property bool staticBuild
    property stringList buildVariant

    // These are deliberately not path types
    // We don't want to resolve them against the source directory
    property string generatedFilesDir: "GeneratedFiles/" + product.name
    property string qmFilesDir: product.destinationDirectory

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
        if (qbs.targetOS.contains("ios"))
            defines = defines.concat(["DARWIN_NO_CARBON", "QT_NO_CORESERVICES", "QT_NO_PRINTER",
                            "QT_NO_PRINTDIALOG", "main=qt_main"]);
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
    cpp.libraryPaths: {
        var libPaths = [libPath];
        if (staticBuild && pluginPath)
            libPaths.push(pluginPath + "/platforms");
        return libPaths;
    }
    cpp.staticLibraries: {
        if (qbs.targetOS.contains('windows') && !product.consoleApplication)
            return ["qtmain" + libInfix + (cpp.debugInformation ? "d" : "") + (!qbs.toolchain.contains("mingw") ? ".lib" : "")];
    }
    cpp.dynamicLibraries: {
        var libs = [];
        if (!frameworkBuild)
            libs=[QtFunctions.getQtLibraryName('Core' + libInfix, qtcore, qbs)];
        if (qbs.targetOS.contains('ios') && staticBuild)
            libs = libs.concat(["z", "m",
                                QtFunctions.getQtLibraryName("PlatformSupport", qtcore, qbs)]);
        if (libs.length === 0)
            return undefined;
        return libs;
    }
    cpp.linkerFlags: ((qbs.targetOS.contains('ios') && staticBuild) ?
                          ["-force_load", pluginPath + "/platforms/" +
                           QtFunctions.getPlatformLibraryName("libqios", qtcore, qbs) + ".a"] : undefined)
    cpp.frameworkPaths: frameworkBuild ? [libPath] : undefined
    cpp.frameworks: {
        var frameworks = [];
        if (frameworkBuild)
            frameworks = [QtFunctions.getQtLibraryName('Core' + libInfix, qtcore, qbs)]
        if (qbs.targetOS.contains('ios') && staticBuild)
            frameworks = frameworks.concat(["Foundation", "CoreFoundation"]);
        if (frameworks.length === 0)
            return undefined;
        return frameworks;
    }
    cpp.rpaths: qbs.targetOS.contains('linux') ? [libPath] : undefined
    cpp.positionIndependentCode: versionMajor >= 5 ? true : undefined
    cpp.cxxFlags: {
        var flags;
        if (qbs.toolchain.contains('msvc')) {
            flags = ['/Zm200'];
            if (versionMajor < 5)
                flags.push('/Zc:wchar_t-');
        }
        if (qbs.toolchain.contains('clang') && config.contains('c++11'))
            flags.push('-stdlib=libc++');
        return flags;
    }

    additionalProductFileTags: ["qm"]

    validate: {
        var requiredProperties = {
            "binPath": binPath,
            "incPath": incPath,
            "libPath": libPath,
            "mkspecPath": mkspecPath,
            "version": version,
            "config": config,
            "qtConfig": qtConfig,
            // Validate these in case 'version' is in some non-standard format
            "versionMajor": versionMajor,
            "versionMinor": versionMinor,
            "versionPatch": versionPatch
        };

        if (!staticBuild) {
            requiredProperties["pluginPath"] = pluginPath;
        }

        var missingProperties = [];
        for (var i in requiredProperties) {
            if (requiredProperties[i] === undefined) {
                missingProperties.push("Qt.core." + i);
            }
        }

        var invalidProperties = {};
        if (versionMajor <= 0)
            invalidProperties["versionMajor"] = "must be > 0";
        if (versionMinor < 0)
            invalidProperties["versionMinor"] = "must be >= 0";
        if (versionPatch < 0)
            invalidProperties["versionPatch"] = "must be >= 0";

        var errorMessage = "";
        if (missingProperties.length > 0) {
            errorMessage += "The following Qt module properties are not set. " +
                            "Set them in your profile:\n" +
                            missingProperties.sort().join("\n");
        }

        if (Object.keys(invalidProperties).length > 0) {
            errorMessage += "The following Qt module properties have invalid values:\n" +
                            Object.map(invalidProperties,
                                function(msg, prop) { return prop + ": " + msg; }).join("\n");
        }

        if (errorMessage.length > 0)
            throw errorMessage;
    }

    setupRunEnvironment: {
        if (qbs.targetOS.contains('windows')) {
            var v = getenv('PATH') || '';
            if (v.length > 0 && v.charAt(0) != ';')
                v = ';' + v;
            v = FileInfo.toWindowsSeparators(binPath) + v;
            putenv('PATH', v);
        }
    }

    FileTagger {
        pattern: "*.qrc"
        fileTags: ["qrc"]
    }

    FileTagger {
        pattern: "*.ts"
        fileTags: ["ts"]
    }

    FileTagger {
        pattern: "*.qdoc"
        fileTags: ["qdoc"]
    }

    FileTagger {
        pattern: "*.qdocconf"
        fileTags: ["qdocconf"]
    }

    FileTagger {
        pattern: "*.qhp"
        fileTags: ["qhp"]
    }

    Rule {
        inputs: ["moc_cpp"]

        Artifact {
            fileName: ModUtils.moduleProperty(product, "generatedFilesDir")
                      + '/' + input.completeBaseName + ".moc"
            fileTags: ["hpp"]
        }

        prepare: {
            var cmd = new Command(Moc.fullPath(product),
                                  Moc.args(product, input, output.fileName));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["moc_hpp"]

        Artifact {
            fileName: ModUtils.moduleProperty(product, "generatedFilesDir")
                      + "/moc_" + input.completeBaseName + ".cpp"
            fileTags: [ "cpp" ]
        }

        prepare: {
            var cmd = new Command(Moc.fullPath(product),
                                  Moc.args(product, input, output.fileName));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["moc_hpp_inc"]

        Artifact {
            fileName: ModUtils.moduleProperty(product, "generatedFilesDir")
                      + "/moc_" + input.completeBaseName + ".cpp"
            fileTags: [ "hpp" ]
        }

        prepare: {
            var cmd = new Command(Moc.fullPath(product),
                                  Moc.args(product, input, output.fileName));
            cmd.description = 'moc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }

    Rule {
        inputs: ["qrc"]

        Artifact {
            fileName: ModUtils.moduleProperty(product, "generatedFilesDir")
                      + "/qrc_" + input.completeBaseName + ".cpp";
            fileTags: ["cpp"]
        }
        prepare: {
            var cmd = new Command(ModUtils.moduleProperty(product, "binPath") + '/rcc',
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
            fileName: FileInfo.joinPaths(ModUtils.moduleProperty(product, "qmFilesDir"),
                                         input.completeBaseName + ".qm")
            fileTags: ["qm"]
        }

        prepare: {
            var cmd = new Command(ModUtils.moduleProperty(product, "binPath") + '/'
                                  + ModUtils.moduleProperty(product, "lreleaseName"),
                                  ['-silent', input.fileName, '-qm', output.fileName]);
            cmd.description = 'lrelease ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'filegen';
            return cmd;
        }
    }

    Rule {
        inputs: "qdocconf-main"
        explicitlyDependsOn: ["qdoc", "qdocconf"]

        Artifact {
            fileName: ModUtils.moduleProperty(product, "generatedFilesDir") + "/html"
            fileTags: ["qdoc-html"]
        }

        Artifact {
            fileName: ModUtils.moduleProperty(product, "generatedFilesDir") + "/html/"
                      + ModUtils.moduleProperty(product, "qdocQhpFileName")
            fileTags: ["qhp"]
        }

        prepare: {
            var outputDir = outputs["qdoc-html"][0].fileName;
            var args = [input.fileName];
            var qtVersion = ModUtils.moduleProperty(product, "versionMajor");
            if (qtVersion >= 5) {
                args.push("-outputdir");
                args.push(outputDir);
            }
            var cmd = new Command(ModUtils.moduleProperty(product, "binPath") + '/'
                                  + ModUtils.moduleProperty(product, "qdocName"), args);
            cmd.description = 'qdoc ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'filegen';
            cmd.environment = ModUtils.moduleProperty(product, "qdocEnvironment");
            cmd.environment.push("OUTDIR=" + outputDir); // Qt 4 replacement for -outputdir
            return cmd;
        }
    }

    Rule {
        inputs: "qhp"

        Artifact {
            fileName: ModUtils.moduleProperty(product, "generatedFilesDir")
                      + '/' + input.completeBaseName + ".qch"
            fileTags: ["qch"]
        }

        prepare: {
            var args = [input.fileName];
            args = args.concat(ModUtils.moduleProperty(product, "helpGeneratorArgs"));
            args.push("-o");
            args.push(output.fileName);
            var cmd = new Command(ModUtils.moduleProperty(product, "binPath") + "/qhelpgenerator",
                                  args);
            cmd.description = 'qhelpgenerator ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'filegen';
            return cmd;
        }
    }
}
