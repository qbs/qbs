import qbs

Product {
    name: "autotest-runner"
    type: "autotest-result"
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
            return cmd;
        }
    }
}
