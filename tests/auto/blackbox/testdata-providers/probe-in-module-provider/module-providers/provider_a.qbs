import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    property string sysroot: qbs.sysroot
    Probe {
        id: theProbe
        property string theValue: "value"
        property string dummy: sysroot
        configure: {
            console.info("Running probe with irrelevant value '" + dummy + "'");
            found = true;
        }
    }
    isEager: false
    property bool found: theProbe.found
    property string theValue: theProbe.theValue
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsmetatestmodule", theValue, undefined, found);
        if (sysroot !== qbs.sysroot)
            throw "this is unexpected";
        return "";
    }
}
