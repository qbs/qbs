import qbs.Probes

Module {
    property string packageName
    property string libDir

    Probes.PkgConfigProbe {
        id: theProbe
        name: packageName
        libDirs: [libDir]
    }

    property bool probeSuccess: theProbe.found
    property stringList libs: theProbe.libs
    property stringList cFlags: theProbe.cflags
    property string packageVersion: theProbe.modversion

    Rule {
        multiplex: true
        outputFileTags: "theType"
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info(product.name + " found: " + product.themodule.probeSuccess);
                console.info(product.name + " libs: " + JSON.stringify(product.themodule.libs));
                console.info(product.name + " cflags: " + JSON.stringify(product.themodule.cFlags));
                console.info(product.name + " version: " + product.themodule.packageVersion);
            }
            return [cmd];
        }
    }

}
