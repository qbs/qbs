import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    property stringList someProp: "provider_a"
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsmetatestmodule", undefined, someProp);
        return "";
    }
}
