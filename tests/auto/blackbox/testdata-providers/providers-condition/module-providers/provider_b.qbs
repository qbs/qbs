import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    isEager: false
    condition: moduleName === "qbsothermodule"
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsothermodule", "from_provider_b");
        return "";
    }
}
