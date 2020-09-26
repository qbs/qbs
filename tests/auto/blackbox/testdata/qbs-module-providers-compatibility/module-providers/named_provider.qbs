import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsmetatestmodule", "from_named_provider");
        return "";
    }
}
