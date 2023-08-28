import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    isEager: false
    property bool deprecated: false
    PropertyOptions {
        name: "deprecated"
        removalVersion: "2.2.0"
    }
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, moduleName, "from_provider_a");
        return "";
    }
}
