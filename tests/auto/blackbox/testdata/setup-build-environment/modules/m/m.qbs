import qbs.Environment
import qbs.TextFile

Module {
    additionalProductTypes: ["m.target"]
    Rule {
        multiplex: true
        Artifact {
            filePath: "m.output"
            fileTags: "m.target"
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "creating " + output.fileName;
            cmd.sourceCode = function() {
                var f = new TextFile(output.filePath, TextFile.WriteOnly);
                var env = Environment.getEnv("BUILD_ENV_" + product.name.toUpperCase());
                if (env)
                    f.writeLine(env);
                f.close();
            };
            return cmd;
        }
    }
}
