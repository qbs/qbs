import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    isEager: false
    property stringList aProperty: "zero"
    PropertyOptions {
        name: "aProperty"
        allowedValues: ["one", "two"]
    }
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, moduleName, "from_provider");
        return "";
    }
}
