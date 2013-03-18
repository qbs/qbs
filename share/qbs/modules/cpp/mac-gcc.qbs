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
            fileName: product.targetName + ".app/Contents/Info.plist"
            fileTags: ["infoplist"]
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating Info.plist";
            cmd.highlight = "codegen";
            cmd.infoPlist = ModUtils.moduleProperty(product, "infoPlist") || {
                CFBundleDisplayName: product.targetName,
                CFBundleExecutable: product.targetName,
                CFBundleIconFile: product.targetName + ".icns",
                CFBundleInfoDictionaryVersion: "6.0",
                CFBundleName: product.targetName,
                CFBundlePackageType: "APPL",
                CFBundleSignature: "????"
            };
            cmd.sourceCode = function() {
                var infoplist = new TextFile(outputs.infoplist[0].fileName, TextFile.WriteOnly);
                infoplist.writeLine('<?xml version="1.0" encoding="UTF-8"?>');
                infoplist.writeLine('<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">');
                infoplist.writeLine('<plist version="1.0">');
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
            fileName: product.targetName + ".app/Contents/PkgInfo"
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
            fileName: product.targetName + ".app"
            fileTags: ["applicationbundle"]
        }

        prepare: {
            // This command is intentionally empty; it just lets the user know a bundle has been made
            var cmd = new JavaScriptCommand();
            cmd.description = "creating app bundle";
            cmd.highlight = "codegen";
            return cmd;
        }
    }
}
