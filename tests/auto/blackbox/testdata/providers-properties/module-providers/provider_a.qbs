import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    property string someProp: "provider_a"
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsmetatestmodule", someProp);
        return "";
    }
}
