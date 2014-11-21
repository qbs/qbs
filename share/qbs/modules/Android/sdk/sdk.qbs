import qbs
import qbs.File
import qbs.FileInfo
import qbs.ModUtils
import qbs.TextFile

Module {
    property path sdkDir
    property string buildToolsVersion
    property string platform

    // Internal properties.
    property path aaptFilePath: FileInfo.joinPaths(buildToolsDir, "aapt")
    property path androidJarFilePath: FileInfo.joinPaths(sdkDir, "platforms", platform,
                                                         "android.jar")
    property path buildToolsDir: FileInfo.joinPaths(sdkDir, "build-tools", buildToolsVersion)
    property path generatedJavaFilesBaseDir: FileInfo.joinPaths(product.buildDirectory, "gen")
    property path generatedJavaFilesDir: FileInfo.joinPaths(generatedJavaFilesBaseDir,
                                         product.packageName.split('.').join('/'))

    Depends { name: "java" }
    java.languageVersion: "1.5"
    java.runtimeVersion: "1.5"
    java.bootClassPaths: androidJarFilePath

    FileTagger {
        patterns: ["AndroidManifest.xml"]
        fileTags: ["android.manifest"]
    }

    FileTagger {
        patterns: ["*.aidl"]
        fileTags: ["android.aidl"]
    }


    Rule {
        inputs: ["android.aidl"]
        Artifact {
            filePath: FileInfo.joinPaths(qbs.getHash(input.filePath),
                                         input.completeBaseName + ".java")
            fileTags: ["java.java"]
        }

        prepare: {
            var aidl = FileInfo.joinPaths(ModUtils.moduleProperty(product, "buildToolsDir"),
                                          "aidl");
            cmd = new Command(aidl, [input.filePath, output.filePath]);
            cmd.description = "Processing " + input.fileName;
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        inputs: ["android.resources", "android.assets", "android.manifest"]
        Artifact {
            filePath: FileInfo.joinPaths(ModUtils.moduleProperty(product, "generatedJavaFilesDir"),
                                         "R.java")
            fileTags: ["java.java"]
        }

        Artifact {
            filePath: product.name + ".ap_"
            fileTags: ["android.ap_"]
        }

        prepare: {
            var manifestFilePath = inputs["android.manifest"][0].filePath;
            var args = ["package", "-f", "-m", "--no-crunch",
                        "-M", manifestFilePath,
                        "-S", product.resourcesDir,
                        "-I", ModUtils.moduleProperty(product, "androidJarFilePath"),
                        "-J", ModUtils.moduleProperty(product, "generatedJavaFilesBaseDir"),
                        "-F", outputs["android.ap_"][0].filePath, "--generate-dependencies"];
            if (product.moduleProperty("qbs", "buildVariant") === "debug")
                args.push("--debug-mode");
            if (File.exists(product.assetsDir))
                args.push("-A", product.assetsDir);
            var cmd = new Command(ModUtils.moduleProperty(product, "aaptFilePath"), args);
            cmd.description = "Processing resources";
            return [cmd];
        }
    }

    Rule {
        inputs: ["android.manifest"] // FIXME: Workaround for the fact that rules need inputs
        Artifact {
            filePath: FileInfo.joinPaths(ModUtils.moduleProperty(product, "generatedJavaFilesDir"),
                                         "BuildConfig.java")
            fileTags: ["java.java"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "Generating BuildConfig.java";
            cmd.sourceCode = function() {
                var debugValue = product.moduleProperty("qbs", "buildVariant") === "debug"
                        ? "true" : "false";
                var ofile = new TextFile(output.filePath, TextFile.WriteOnly);
                ofile.writeLine("package " + ModUtils.moduleProperty(product, "packageName") +  ";")
                ofile.writeLine("public final class BuildConfig {");
                ofile.writeLine("    public final static boolean DEBUG = " + debugValue + ";");
                ofile.writeLine("}");
                ofile.close();
            };
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        inputs: ["java.class"]
        Artifact {
            filePath: "classes.dex"
            fileTags: ["android.dex"]
        }
        prepare: {
            var dxFilePath = FileInfo.joinPaths(ModUtils.moduleProperty(product, "buildToolsDir"),
                                                "dx");
            var args = ["--dex", "--output", output.filePath,
                        product.moduleProperty("java", "classFilesDir")];
            var cmd = new Command(dxFilePath, args);
            cmd.description = "Creating " + output.fileName;
            return [cmd];
        }
    }

    // TODO: ApkBuilderMain is deprecated. Do we have to provide our own tool directly
    //       accessing com.android.sdklib.build.ApkBuilder or is there a simpler way?
    Rule {
        multiplex: true
        inputs: ["android.dex", "android.ap_"]
        inputsFromDependencies: ["android.nativelibrary"]
        Artifact {
            filePath: product.name + ".apk.unaligned"
            fileTags: ["android.apk.unaligned"]
        }
        prepare: {
            var args = ["-classpath", FileInfo.joinPaths(ModUtils.moduleProperty(product, "sdkDir"),
                                                         "tools/lib/sdklib.jar"),
                        "com.android.sdklib.build.ApkBuilderMain", output.filePath,
                        "-z", inputs["android.ap_"][0].filePath,
                        "-f", inputs["android.dex"][0].filePath];
            if (product.moduleProperty("qbs", "buildVariant") === "debug")
                args.push("-d");
            if (inputs["android.nativelibrary"])
                args.push("-nf", FileInfo.joinPaths(project.buildDirectory, "lib"));
            var cmd = new Command(product.moduleProperty("java", "interpreterFilePath"), args);
            cmd.description = "Generating " + output.fileName;
            return [cmd];
        }
    }

    Rule {
        multiplex: true
        inputs: ["android.apk.unaligned"]
        Artifact {
            filePath: product.name + ".apk"
            fileTags: ["android.apk"]
        }
        prepare: {
            var zipalign = FileInfo.joinPaths(ModUtils.moduleProperty(product, "buildToolsDir"),
                                              "zipalign");
            var args = ["-f", "4", inputs["android.apk.unaligned"][0].filePath, output.filePath];
            var cmd = new Command(zipalign, args);
            cmd.description = "Creating " + output.fileName;
            return [cmd];
        }
    }
}
