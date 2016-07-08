import qbs
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
        Artifact {
            filePath: "dummy.out"
            fileTags: ["theType"]
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info(product.name + " found: "
                             + product.moduleProperty("themodule", "probeSuccess"));
                console.info(product.name + " libs: "
                             + JSON.stringify(product.moduleProperty("themodule", "libs")));
                console.info(product.name + " cflags: "
                             + JSON.stringify(product.moduleProperty("themodule", "cFlags")));
                console.info(product.name + " version: "
                             + product.moduleProperty("themodule", "packageVersion"));
            }
            return [cmd];
        }
    }

}
