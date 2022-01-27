import qbs.FileInfo
import qbs.Host
import qbs.TextFile

Product {
    type: ["dummy"]
    Rule {
        multiplex: true
        outputFileTags: "dummy"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                var output = new TextFile(FileInfo.joinPaths(product.sourceDirectory, "output.txt"),
                                          TextFile.WriteOnly);
                output.writeLine("architecture: " +Host.architecture());
                output.writeLine("os: " + Host.os());
                output.writeLine("platform: " + Host.platform());
                output.writeLine("osVersion: " + Host.osVersion());
                output.writeLine("osBuildVersion: " + Host.osBuildVersion());
                output.writeLine("osVersionParts: " + Host.osVersionParts());
                output.writeLine("osVersionMajor: " + Host.osVersionMajor());
                output.writeLine("osVersionMinor: " + Host.osVersionMinor());
                output.writeLine("osVersionPatch: " + Host.osVersionPatch());
                output.writeLine("nullDevice: " + Host.nullDevice());
                output.close();
            };
            return [cmd];
        }
    }
}
