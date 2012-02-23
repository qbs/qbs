import qbs.base 1.0

UnixGCC {
    condition: qbs.hostOS == 'mac' && qbs.targetOS == 'mac' && qbs.toolchain == 'gcc'

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
            cmd.sourceCode = function() {
                var infoplist = new TextFile(outputs.infoplist[0].fileName, TextFile.WriteOnly);
                infoplist.write("<foobar>");
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
            cmd.sourceCode = function() {
                var pkginfo = new TextFile(outputs.pkginfo[0].fileName, TextFile.WriteOnly);
                pkginfo.write("FOO");
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
                File.copy(inputs.application[0].fileName, outputs.applicationbundle[0].fileName);
            }
            return cmd;
        }
    }
}
