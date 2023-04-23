import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    Probe {
        id: theProbe
        configure: {
            console.info("Running probe");
            found = true;
        }
    }
    property bool found: theProbe.found
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsmetatestmodule", undefined, undefined, found);
        return "";
    }
}
