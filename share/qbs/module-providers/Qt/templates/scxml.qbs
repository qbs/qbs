import qbs.FileInfo
import qbs.Utilities

QtModule {
    qtModuleName: "Scxml"

    property string _qscxmlcDir: Utilities.versionCompare(Qt.core.version, "6.3") >= 0
                                 ? Qt.core.libExecPath : Qt.core.binPath
    property string qscxmlcName: "qscxmlc"
    property string className
    property string namespace
    property bool generateStateMethods
    property stringList additionalCompilerFlags

    Rule {
        inputs: ["qt.scxml.compilable"]

        Artifact {
            filePath: FileInfo.joinPaths(input.moduleProperty("Qt.core", "generatedHeadersDir"),
                                         input.baseName + ".h")
            fileTags: ["hpp", "unmocable"]
        }
        Artifact {
            filePath: input.baseName + ".cpp"
            fileTags: ["cpp", "unmocable"]
        }

        prepare: {
            var compilerName = product.moduleProperty("Qt.scxml", "qscxmlcName");
            var compilerPath = FileInfo.joinPaths(input.Qt.scxml._qscxmlcDir, compilerName);
            var args = ["--header", outputs["hpp"][0].filePath,
                        "--impl", outputs["cpp"][0].filePath];
            var cxx98 = input.moduleProperty("cpp", "cxxLanguageVersion") === "c++98";
            if (cxx98)
                args.push("-no-c++11");
            var className = input.moduleProperty("Qt.scxml", "className");
            if (className)
                args.push("--classname", className);
            var namespace = input.moduleProperty("Qt.scxml", "namespace");
            if (namespace)
                args.push("--namespace", namespace);
            if (input.Qt.scxml.generateStateMethods
                    && Utilities.versionCompare(product.Qt.scxml.version, "5.9") >= 0) {
                args.push("--statemethods");
            }
            if (input.Qt.scxml.additionalCompilerFlags)
                args = args.concat(input.Qt.scxml.additionalCompilerFlags);
            args.push(input.filePath);
            var cmd = new Command(compilerPath, args);
            cmd.description = "compiling " + input.fileName;
            cmd.highlight = "codegen";
            return [cmd];
        }
    }

    architectures: @archs@
    targetPlatform: @targetPlatform@
    staticLibsDebug: @staticLibsDebug@
    staticLibsRelease: @staticLibsRelease@
    dynamicLibsDebug: @dynamicLibsDebug@
    dynamicLibsRelease: @dynamicLibsRelease@
    linkerFlagsDebug: @linkerFlagsDebug@
    linkerFlagsRelease: @linkerFlagsRelease@
    frameworksDebug: @frameworksDebug@
    frameworksRelease: @frameworksRelease@
    frameworkPathsDebug: @frameworkPathsDebug@
    frameworkPathsRelease: @frameworkPathsRelease@
    libNameForLinkerDebug: @libNameForLinkerDebug@
    libNameForLinkerRelease: @libNameForLinkerRelease@
    libFilePathDebug: @libFilePathDebug@
    libFilePathRelease: @libFilePathRelease@
    pluginTypes: @pluginTypes@
    moduleConfig: @moduleConfig@

    cpp.defines: @defines@
    cpp.systemIncludePaths: @includes@
    cpp.libraryPaths: @libraryPaths@

    @additionalContent@
}
