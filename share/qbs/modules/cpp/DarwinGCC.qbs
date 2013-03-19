import qbs 1.0
import '../utils.js' as ModUtils
import 'darwin-tools.js' as Tools

UnixGCC {
    condition: false

    compilerDefines: ["__GNUC__", "__APPLE__"]
    dynamicLibrarySuffix: ".dylib"

    property var defaultInfoPlist
    property var infoPlist: defaultInfoPlist
    property bool buildDsym: qbs.buildVariant === "release"

    Rule {
        multiplex: true
        inputs: ["qbs"]

        Artifact {
            fileName: product.targetName + ".app/" +
                      (product.moduleProperty("qbs", "targetOS") === "mac" ? "Contents/" : "") +
                      "PkgInfo"
            fileTags: ["pkginfo"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating PkgInfo";
            cmd.highlight = "codegen";
            var pkgType = 'APPL';
            var pkgSign = '????';
            var infoPlist = product.moduleProperty("cpp", "infoPList");
            if (infoPlist && infoPlist.hasOwnProperty('CFBundlePackageType'))
                pkgType = infoPlist['CFBundlePackageType'];
            if (infoPlist && infoPlist.hasOwnProperty('CFBundleSignature'))
                pkgSign = infoPlist['CFBundleSignature'];
            cmd.pkgInfo =  pkgType + pkgSign;
            cmd.sourceCode = function() {
                var pkginfo = new TextFile(outputs.pkginfo[0].fileName, TextFile.WriteOnly);
                pkginfo.write(pkgInfo);
                pkginfo.close();
            }
            return cmd;
        }
    }

    Rule {
        inputs: ["application"]

        Artifact {
            fileName: input.fileName + ".app.dSYM"
            fileTags: ["dsym"]
        }

        prepare: {
            if (!product.moduleProperty("cpp", "buildDsym")) {
                // to be removed when dynamic dependencies work
                var cmd = new JavaScriptCommand();
                cmd.description = "skipping dsym generation";
                cmd.highlight = "codegen";
                return cmd;
            }
            var cmd = new Command("dsymutil", ["--out=" + outputs.dsym[0].fileName, input.fileName]);
            cmd.description = "generating dsym";
            cmd.highlight = "codegen";
            return cmd;
        }
    }
}
