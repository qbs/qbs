import qbs.BundleTools
import qbs.TextFile

CppApplication {
    Depends { name: "bundle" }
    cpp.minimumMacosVersion: "10.7"
    files: ["main.c", "ByteArray-Info.plist"]
    type: base.concat(["txt_output"])

    Properties {
        condition: qbs.targetOS.includes("darwin")
        bundle.isBundle: true
        bundle.identifierPrefix: "com.test"
    }

    Rule {
        inputs: ["aggregate_infoplist"]
        Artifact {
            filePath: input.fileName + ".out"
            fileTags: ["txt_output"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating" + output.fileName + " from " + input.fileName;
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                var plist = new BundleTools.infoPlistContents(input.filePath);
                var content = plist["DataKey"];
                var int8view = new Uint8Array(content);
                file = new TextFile(output.filePath, TextFile.WriteOnly);
                file.write(String.fromCharCode.apply(null, int8view));
                file.close();
            }
            return [cmd];
        }
    }
}
