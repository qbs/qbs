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

    PropertyOptions {
        name: "buildVariant"
        allowedValues: ['debug', 'release']
        description: "Defines the consistence of your ice cream sandwich"
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
            fileName: input.modules.qbs.installDir + "/" + input.fileName
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function() {
                        File.remove(output.fileName);
                        File.copy(input.fileName, output.fileName);
                    }
            cmd.description = "installing " + FileInfo.fileName(output.fileName);
            cmd.highlight = "linker";
            return cmd;
        }

    }
}
