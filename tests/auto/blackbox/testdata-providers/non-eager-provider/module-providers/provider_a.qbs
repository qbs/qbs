import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    isEager: false
    relativeSearchPaths: {
        if (moduleName === "nonexistentmodule")
            return undefined;
        Helpers.writeModule(outputBaseDir, moduleName, "from_provider_a");
        return "";
    }
}
