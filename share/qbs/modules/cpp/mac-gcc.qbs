import qbs 1.0
import '../utils.js' as ModUtils

UnixGCC {
    condition: qbs.hostOS === 'mac' && qbs.targetOS === 'mac' && qbs.toolchain === 'gcc'

    compilerDefines: ["__GNUC__", "__APPLE__"]
    dynamicLibrarySuffix: ".dylib"

    property string infoPlist
    property string pkgInfo

    Rule {
        multiplex: true
        inputs: ["qbs"]

        Artifact {
            fileName: product.name + ".app/Info.plist"
            fileTags: ["infoplist"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating Info.plist";
            cmd.highlight = "codegen";
            cmd.infoPlist = ModUtils.moduleProperty(product, "infoPlist") || {};
            cmd.sourceCode = function() {
                var infoplist = new TextFile(outputs.infoplist[0].fileName, TextFile.WriteOnly);
                infoplist.writeLine('<?xml version="1.0" encoding="UTF-8"?>');
                infoplist.writeLine('<!DOCTYPE plist SYSTEM "file://localhost/System/Library/DTDs/PropertyList.dtd">');
                infoplist.writeLine('<plist version="0.9">');
                infoplist.writeLine('<dict>');
                for (var key in infoPlist) {
                    infoplist.writeLine('    <key>' + key + '</key>');
                    var value = infoPlist[key];
                    // Plist files depends on variable types
                    var type = typeof(value);
                    if (type === "string") {
                        // It's already ok
                    } else if (type === "boolean") {
                        if (value)
                            type = "true";
                        else
                            type = "false";
                        value = undefined;
                    } else if (type === "number") {
                        // Is there better way?
                        if (value % 1 === 0)
                            type = "integer";
                        else
                            type = "real";
                    } else {
                        // FIXME: Add Dict support:
                        throw "Unsupported type '" + type + "'";
                    }
                    if (value === undefined)
                        infoplist.writeLine('    <' + type + '/>');
                    else
                        infoplist.writeLine('    <' + type + '>' + value + '</' + type + '>');
                }
                infoplist.writeLine('</dict>');
                infoplist.writeLine('</plist>');
                infoplist.close();
            }
            return cmd;
        }
    }

    Rule {
        multiplex: true
        inputs: ["qbs"]

        Artifact {
            fileName: product.name + ".app/PkgInfo"
            fileTags: ["pkginfo"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating PkgInfo";
            cmd.highlight = "codegen";
            cmd.pkgInfo = ModUtils.moduleProperty(product, "pkgInfo") || "FOO";
            cmd.sourceCode = function() {
                var pkginfo = new TextFile(outputs.pkginfo[0].fileName, TextFile.WriteOnly);
                pkginfo.write(pkgInfo);
                pkginfo.close();
            }
            return cmd;
        }
    }

    Rule {
        multiplex: true
        inputs: ["application", "infoplist", "pkginfo"]

        Artifact {
            fileName: product.name + ".app/Contents/MacOS/" + product.name
            fileTags: ["applicationbundle"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating app bundle";
            cmd.highlight = "codegen";
            cmd.sourceCode = function() {
                File.remove(outputs.applicationbundle[0].fileName);
                File.copy(inputs.application[0].fileName, outputs.applicationbundle[0].fileName);
            }
            return cmd;
        }
    }
}
