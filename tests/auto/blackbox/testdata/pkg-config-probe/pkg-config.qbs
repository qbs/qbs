import qbs
import qbs.Probes

Product {
    name: "theProduct"
    type: ["theType"]

    property string packageName

    property bool probeSuccess: theProbe.found
    property stringList libs: theProbe.libs
    property stringList cFlags: theProbe.cflags
    property string packageVersion: theProbe.modversion

    Probes.PkgConfigProbe {
        id: theProbe
        name: product.packageName
        libDirs: [path]
    }

    Transformer {
        Artifact {
            filePath: "dummy.out"
            fileTags: product.type
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.silent = true;
            cmd.sourceCode = function() {
                console.info("found: " + product.probeSuccess);
                console.info("libs: " + JSON.stringify(product.libs));
                console.info("cflags: " + JSON.stringify(product.cFlags));
                console.info("version: " + product.packageVersion);
            }
            return [cmd];
        }
    }
}
