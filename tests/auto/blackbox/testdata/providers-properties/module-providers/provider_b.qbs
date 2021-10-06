import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    property stringList someProp: "provider_b"
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsothermodule", undefined, someProp);
        return "";
    }
}
