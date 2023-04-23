import "../../qbs-module-providers-helpers.js" as Helpers

ModuleProvider {
    property string sysroot: qbs.sysroot
    relativeSearchPaths: {
        Helpers.writeModule(outputBaseDir, "qbsmetatestmodule", sysroot);
        return "";
    }
}
