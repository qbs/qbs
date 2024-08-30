import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    isEager: false
    condition: moduleName === "qbsmetatestmodule"
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, moduleName, "from_provider_a");
        return "";
    }
}
