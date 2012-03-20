import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo

Module {
    property list<string> references;
    property string buildVariant: "debug"
    property bool debugInformation: (buildVariant == "debug")
    property string optimization: (buildVariant == "debug" ? "none" : "fast")
    property string platform: null
    property string hostOS: getHostOS()
    property string hostArchitecture: getHostDefaultArchitecture()
    property string targetOS: null
    property string targetName: null
    property string toolchain: null
    property string architecture: null
    property string endianness: null
    property string installDir: '.'
    property string sysroot
    property string installPrefix: ""

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
                    throw "Cannot copy '" + input.fileName + "' to '" + output.fileName + "'";
            }
            cmd.description = "installing " + FileInfo.fileName(output.fileName);
            cmd.highlight = "linker";
            return cmd;
        }
    }
}
