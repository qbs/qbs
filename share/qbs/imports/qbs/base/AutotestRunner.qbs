import qbs
import qbs.ModUtils

Product {
    name: "autotest-runner"
    type: ["autotest-result"]
    builtByDefault: false
    property stringList arguments: []
    property stringList environment: ModUtils.flattenEnvironmentDictionary(
                                         qbs.commonRunEnvironment,
                                         qbs.pathListSeparator)
    property bool limitToSubProject: true
    property stringList wrapper: []
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
            var fullCommandLine = product.wrapper
                .concat([input.filePath])
                .concat(product.arguments);
            var cmd = new Command(fullCommandLine[0], fullCommandLine.slice(1));
            cmd.description = "Running test " + input.fileName;
            cmd.environment = product.environment;
            return cmd;
        }
    }
}
