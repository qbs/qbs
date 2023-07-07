import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    Probe {
        id: theProbe
        property string theValue: "value"
        configure: {
            console.info("Running probe");
            found = true;
        }
    }
    isEager: false
    property bool found: theProbe.found
    property string theValue: theProbe.theValue
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsmetatestmodule", theValue, undefined, found);
        return "";
    }
}
