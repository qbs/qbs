import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo

Module {
    property string buildVariant: "debug"
    property bool debugInformation: (buildVariant == "debug")
    property string optimization: (buildVariant == "debug" ? "none" : "fast")
    property string platform
    property string hostOS: getHostOS()
    property string hostArchitecture: getHostDefaultArchitecture()
    property string pathListSeparator: hostOS === "windows" ? ";" : ":"
    property string pathSeparator: hostOS === "windows" ? "\\" : "/"
    property string targetOS
    property var targetPlatform: {
        if (targetOS === "linux")
            return [ "unix", "linux" ];
        else if (targetOS === "mac")
            return [ "unix", "darwin", "mac" ];
        else if (targetOS === "windows")
            return [ "windows" ];
        else
            throw "Unknown targetOS: \"" + targetOS + "\""
    }
    property string profile
    property string toolchain
    property string architecture
    property string endianness
    property string installDir: '.'
    property string sysroot
    property string installPrefix: ""
    property string deployRoot: "./deployRoot"
    property string deployInfoFile

    PropertyOptions {
        name: "buildVariant"
        allowedValues: ['debug', 'release']
        description: "name of the build variant"
    }

    PropertyOptions {
        name: "optimization"
        allowedValues: ['none', 'fast', 'small']
        description: "optimization level"
    }

    Rule {
        inputs: ["install"]
        Artifact {
            fileTags: ["installed_content"]
            fileName: {
                var targetPath = input.modules.qbs.installDir + "/" + input.fileName
                if (input.modules.qbs.installPrefix && !FileInfo.isAbsolutePath(targetPath))
                    targetPath = input.modules.qbs.installPrefix + "/" + targetPath
                if (product.module.sysroot && FileInfo.isAbsolutePath(targetPath))
                    targetPath = product.module.sysroot + targetPath
                return targetPath
            }
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function() {
                File.remove(output.fileName);
                if (!File.copy(input.fileName, output.fileName))
                    throw "Cannot install '" + input.fileName + "' as '" + output.fileName + "'";
            }
            cmd.description = "installing " + FileInfo.fileName(output.fileName);
            cmd.highlight = "linker";
            return cmd;
        }
    }

    Rule {
        inputs: "deploy"
        multiplex: deployInfoFile != null
        Artifact {
            fileTags: "installed_content"
            fileName: {
                if (product.modules.qbs.deployInfoFile)
                    return product.modules.qbs.deployInfoFile
                return input.modules.qbs.deployRoot + "/" + input.modules.qbs.installPrefix
                    + "/" + input.modules.qbs.installDir + "/" + input.fileName
            }
        }

        prepare: {
            var cmd = new JavaScriptCommand()
            cmd.deployInfo = []
            if (product.modules.qbs.deployInfoFile) {
                for (var i in inputs.deploy) {
                    var sourceFile = inputs.deploy[i].fileName
                    var destFile = product.modules.qbs.installPrefix + "/"
                        + inputs.deploy[i].modules.qbs.installDir + "/"
                        + FileInfo.fileName(sourceFile)
                    destFile = destFile.replace(/\/+/g, "/")
                    destFile = destFile.replace(/\/\.\//g, "/")
                    cmd.deployInfo.push(sourceFile + "|" + destFile)
                }
                cmd.description = "Writing deployment information to '" + output.fileName + "'"
                cmd.sourceCode = function() {
                    var deployInfoFile = new TextFile(output.fileName, TextFile.WriteOnly)
                    for (var i in deployInfo)
                        deployInfoFile.writeLine(deployInfo[i])
                    deployInfoFile.close()
                }
            } else {
                cmd.description = "deploying " + FileInfo.fileName(output.fileName)
                cmd.sourceCode = function() {
                    File.remove(output.fileName)
                    if (!File.copy(input.fileName, output.fileName))
                        throw "Cannot deploy '" + input.fileName + "' to '" + output.fileName + "'"
                }
            }
            cmd.highlight = "linker"
            return cmd
        }
    }
}
