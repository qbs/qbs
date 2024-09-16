import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    isEager: false
    condition: theProbe.found
    Probe {
        id: theProbe
        condition: moduleName === "qbsmetatestmodule"
        configure: {
            found = true
        }
    }
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, moduleName, "from_provider_a");
        return "";
    }
}
