import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    isEager: false
    condition: theProbe.found
    Probe {
        id: theProbe
        condition: moduleName === "qbsothermodule"
        configure: {
            found = true
        }
    }
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsothermodule", "from_provider_b");
        return "";
    }
}
