import qbs
import qbs.DarwinTools

Product {
    name: "autotest-runner"
    type: ["autotest-result"]
    builtByDefault: false
    property bool limitToSubProject: true
    Depends {
        productTypes: "autotest"
        limitToSubProject: product.limitToSubProject
    }
    Rule {
        inputsFromDependencies: "application"
        Artifact {
            filePath: input.filePath.replace('/', '-') + ".result.dummy" // Will never exist.
            fileTags: "autotest-result"
            alwaysUpdated: false
        }
        prepare: {
            var cmd = new Command(input.filePath);
            cmd.description = "Running test " + input.fileName;
            if (product.moduleProperty("qbs", "hostOS").contains("darwin") &&
                product.moduleProperty("qbs", "targetOS").contains("darwin")) {
                cmd.environment = [];
                var env = DarwinTools.standardDyldEnvironment(product);
                for (var i in env) {
                    cmd.environment.push(i + "=" + env[i]);
                }
            }
            return cmd;
        }
    }
}
