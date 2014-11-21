import qbs
import qbs.File
import qbs.FileInfo
import qbs.ModUtils

import "utils.js" as NdkUtils

Module {
    Depends { name: "cpp" }

    property string abi // Corresponds to the subdir under "lib/" in the apk file.

    property string appStl: "system"
    PropertyOptions {
        name: "stlType"
        description: "Corresponds to the 'APP_STL' variable in an Android.mk file."
        allowedValues: [
            "system", "gabi++_static", "gabi++_shared", "stlport_static", "stlport_shared",
            "gnustl_static", "gnustl_shared", "c++_static", "c++_shared"
        ]
    }

    property string buildProfile // E.g. "armeabi-v7a-hard"
    property bool enableExceptions: appStl !== "system"
    property bool enableRtti: appStl !== "system"
    property string ndkDir
    property string platform: "android-9"

    // Internal properties.
    property stringList compilerFlagsDebug: []
    property stringList compilerFlagsRelease: []
    property stringList defines: ["ANDROID"]
    property bool hardFp
    property string gabiBaseDir: FileInfo.joinPaths(ndkDir, "sources/cxx-stl/gabi++")
    property string stlPortBaseDir: FileInfo.joinPaths(ndkDir, "sources/cxx-stl/stlport")
    property string gnuStlBaseDir: FileInfo.joinPaths(ndkDir, "sources/cxx-stl/gnu-libstdc++/4.9")
    property string llvmStlBaseDir: FileInfo.joinPaths(ndkDir, "sources/cxx-stl/llvm-libc++")
    property string sharedStlFilePath: {
        var infix = buildProfile;
        if (armMode === "thumb")
            infix = FileInfo.joinPaths(infix, "thumb");
        infix = FileInfo.joinPaths("libs", infix);
        if (appStl === "gabi++_shared")
            return FileInfo.joinPaths(gabiBaseDir, infix, "libgabi++_shared.so");
        if (appStl === "stlport_shared")
            return FileInfo.joinPaths(stlPortBaseDir, infix, "libstlport_shared.so");
        if (appStl === "gnustl_shared")
            return FileInfo.joinPaths(gnuStlBaseDir, infix, "libgnustl_shared.so");
        if (appStl === "c++_shared")
            return FileInfo.joinPaths(llvmStlBaseDir, infix, "libc++_shared.so");
        return undefined;
    }
    property string gdbserverFileName: "gdbserver"

    property string armMode: abi.startsWith("armeabi")
            ? (qbs.buildVariant === "debug" ? "arm" : "thumb")
            : undefined;
    PropertyOptions {
        name: "armModeType"
        description: "Determines the instruction set for armeabi configurations."
        allowedValues: ["arm", "thumb"]
    }

    cpp.commonCompilerFlags: {
        var flags = qbs.buildVariant === "debug"
                ? compilerFlagsDebug : compilerFlagsRelease;
        if (armMode)
            flags.push("-m" + armMode);
        return flags;
    }

    cpp.cxxFlags: {
        var flags = [];
        if (enableExceptions)
            flags.push("-fexceptions");
        else
            flags.push("-fno-exceptions");
        if (enableRtti)
            flags.push("-frtti");
        else
            flags.push("-fno-rtti");
        return flags;
    }

    cpp.libraryPaths: {
        var prefix = FileInfo.joinPaths(ndkDir, "platforms", platform, "arch-");
        if (buildProfile.startsWith("armeabi"))
            prefix += "arm";
        else
            prefix += buildProfile;
        prefix = FileInfo.joinPaths(prefix, "usr");
        var paths = [];
        if (buildProfile === "mips64" || buildProfile === "x86_64")
            paths.push(FileInfo.joinPaths(prefix, "lib64"));
        paths.push(FileInfo.joinPaths(prefix, "lib"));
        return paths;
    }

    cpp.dynamicLibraries: {
        var libs = ["c"];
        if (!hardFp)
            libs.push("m");
        if (sharedStlFilePath)
            libs.push(sharedStlFilePath);
        return libs;
    }
    cpp.staticLibraries: {
        var libs = ["gcc"];
        if (hardFp)
            libs.push("m_hard");
        var infix = buildProfile;
        if (armMode === "thumb")
            infix = FileInfo.joinPaths(infix, "thumb");
        infix = FileInfo.joinPaths("libs", infix);
        if (appStl === "gabi++_static")
            libs.push(FileInfo.joinPaths(gabiBaseDir, infix, "libgabi++_static.a"));
        else if (appStl === "stlport_static")
            libs.push(FileInfo.joinPaths(stlPortBaseDir, infix, "libstlport_static.a"));
        else if (appStl === "gnustl_static")
            libs.push(FileInfo.joinPaths(gnuStlBaseDir, infix, "libgnustl_static.a"));
        else if (appStl === "c++_static")
            libs.push(FileInfo.joinPaths(llvmStlBaseDir, infix, "libc++_static.a"));
        return libs;
    }
    cpp.includePaths: {
        var includes = [];
        if (appStl.startsWith("gabi++"))
            includes.push(FileInfo.joinPaths(gabiBaseDir, "include"));
        else if (appStl.startsWith("stlport"))
            includes.push(FileInfo.joinPaths(stlPortBaseDir, "stlport"));
        else if (appStl.startsWith("gnustl"))
            includes.push(FileInfo.joinPaths(gnuStlBaseDir, "include"));
        else if (appStl.startsWith("c++_"))
            includes.push(FileInfo.joinPaths(llvmStlBaseDir, "include"));
        return includes;
    }
    cpp.defines: {
        var list = defines;
        if (hardFp)
            list.push("_NDK_MATH_NO_SOFTFP=1");
        return list;
    }
    cpp.sysroot: FileInfo.joinPaths(ndkDir, "platforms", platform,
                                    "arch-" + NdkUtils.abiNameToDirName(abi))

    Rule {
        inputs: ["dynamiclibrary"]
        outputFileTags: {
            var tags = ["android.nativelibrary"];
            if (product.moduleProperty("qbs", "buildVariant") === "debug")
                tags.push("android.gdbserver");
            return tags;
        }
        outputArtifacts: {
            var destDir = FileInfo.joinPaths(project.buildDirectory, "lib",
                                             ModUtils.moduleProperty(product, "abi"));
            var artifacts = [{
                    filePath: FileInfo.joinPaths(destDir, inputs["dynamiclibrary"][0].fileName),
                    fileTags: ["android.nativelibrary"]
            }];
            if (product.moduleProperty("qbs", "buildVariant") === "debug") {
                artifacts.push({
                        filePath: FileInfo.joinPaths(destDir,
                                ModUtils.moduleProperty(product,"gdbserverFileName")),
                        fileTags: ["android.gdbserver"]
                });
            }
            var stlFilePath = ModUtils.moduleProperty(product, "sharedStlFilePath");
            if (stlFilePath) {
                artifacts.push({
                        filePath: FileInfo.joinPaths(destDir, FileInfo.fileName(stlFilePath)),
                        fileTags: ["android.deployedSharedLibrary"]
                });
            }
            return artifacts;
        }

        prepare: {
            var stlFilePath = ModUtils.moduleProperty(product, "sharedStlFilePath");
            var copyCmd = new JavaScriptCommand();
            copyCmd.description = "Copying native library and depencencies";
            copyCmd.stlFilePath = stlFilePath;
            copyCmd.sourceCode = function() {
                File.copy(inputs["dynamiclibrary"][0].filePath,
                          outputs["android.nativelibrary"][0].filePath);
                if (product.moduleProperty("qbs", "buildVariant") === "debug") {
                    var arch = ModUtils.moduleProperty(product, "abi");
                    arch = NdkUtils.abiNameToDirName(arch);
                    var gdbSrcPath = FileInfo.joinPaths(ModUtils.moduleProperty(product, "ndkDir"),
                            "prebuilt/android-" + arch, "gdbserver/gdbserver");
                    File.copy(gdbSrcPath, outputs["android.gdbserver"][0].filePath);
                }
                if (stlFilePath)
                    File.copy(stlFilePath, outputs["android.deployedSharedLibrary"][0].filePath);

            }
            var stripBinary = product.moduleProperty("cpp", "toolchainPathPrefix") + "strip";
            var stripArgs = ["--strip-unneeded", outputs["android.nativelibrary"][0].filePath];
            if (stlFilePath)
                stripArgs.push(stlFilePath);
            var stripCmd = new Command(stripBinary, stripArgs);
            stripCmd.description = "Stripping unneeded symbols";
            return [copyCmd, stripCmd];
        }
    }
}
