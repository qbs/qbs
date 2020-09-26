import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    property string someProp: "provider_b"
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsothermodule", someProp);
        return "";
    }
}
