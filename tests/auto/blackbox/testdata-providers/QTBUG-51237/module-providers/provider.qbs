import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    property stringList theProperty
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "mymodule", "from_provider", theProperty);
        return "";
    }
}
