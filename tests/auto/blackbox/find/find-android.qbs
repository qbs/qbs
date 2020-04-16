import qbs.TextFile

Product {
    property string packageName: ""
    qbs.targetPlatform: "android"
    multiplexByQbsProperties: ["architectures"]

    Properties {
        condition: qbs.architectures && qbs.architectures.length > 1
        aggregate: true
        multiplexedType: "json_arch"
    }

    Depends { name: "Android.sdk"; required: false }
    Depends { name: "Android.ndk"; required: false }
    type: ["json"]

    Rule {
        multiplex: true
        property stringList inputTags: "json_arch"
        inputsFromDependencies: inputTags
        inputs: product.aggregate ? [] : inputTags
        Artifact {
            filePath: ["android.json"]
            fileTags: ["json"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = output.filePath;
            cmd.sourceCode = function() {
                var tools = {};

                for (var i in inputs["json_arch"]) {
                    var tf = new TextFile(inputs["json_arch"][i].filePath, TextFile.ReadOnly);
                    var json = JSON.parse(tf.readAll());
                    tools["ndk"] = json["ndk"];
                    tools["ndk-samples"] = json["ndk-samples"];
                    tf.close();
                }

                if (product.moduleProperty("Android.sdk", "present")) {
                    tools["sdk"] = product.moduleProperty("Android.sdk", "sdkDir");
                    tools["sdk-build-tools-dx"] = product.Android.sdk.dxFilePath;
                }

                if (product.java && product.java.present)
                    tools["jar"] = product.java.jarFilePath;

                var tf;
                try {
                    tf = new TextFile(output.filePath, TextFile.WriteOnly);
                    tf.writeLine(JSON.stringify(tools, undefined, 4));
                } finally {
                    if (tf)
                        tf.close();
                }
            };
            return cmd;
        }
    }
    Rule {
        multiplex: true
        Artifact {
            filePath: ["android_arch.json"]
            fileTags: ["json_arch"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = output.filePath;
            cmd.sourceCode = function() {
                var tools = {};
                if (product.moduleProperty("Android.ndk", "present")) {
                    tools["ndk"] = product.moduleProperty("Android.ndk", "ndkDir");
                    tools["ndk-samples"] = product.Android.ndk.ndkSamplesDir;
                }

                var tf;
                try {
                    tf = new TextFile(output.filePath, TextFile.WriteOnly);
                    tf.writeLine(JSON.stringify(tools, undefined, 4));
                } finally {
                    if (tf)
                        tf.close();
                }
            };
            return cmd;
        }
    }
}

