import qbs.FileInfo

Project {
    property string name: 'codegen'
    property string osSpecificName: name.toUpperCase() + '_' + qbs.targetPlatform.toUpperCase()

    Product {
        type: 'application'
        consoleApplication: true
        name: project.name
        property var replacements: ({
                NUMBERTYPE: "int",
                STRINGTYPE: "char **",
                FUNCTIONNAME: "main"
            })
        Group {
            files: 'foo.txt'
            fileTags: ['text']
        }
        Depends { name: 'cpp' }
        Depends { name: 'Qt.core' }
    }

    Rule {
        inputs: ['text']
        Artifact {
            fileTags: ['cpp']
            filePath: input.baseName + '.cpp'
        }
        prepare: {
            function expandMacros(str, table)
            {
                var rex = /\$\w+/;
                var m = rex.exec(str);
                while (m != null) {
                    str = str.substr(0, m.index)
                            + table[m[0].substr(1)]
                            + str.substr(m.index + m[0].length);
                    m = rex.exec(str);
                }
                return str;
            }

            // check whether multipart module name translation is working
            var actual = product.moduleProperty("Qt.core", "mocName");
            if (!actual || !actual.contains("moc"))
                throw "multipart module name translation is broken";

            // check whether we can access project properties here
            var expected = "CODEGEN_" + product.moduleProperty("qbs",
                                                               "targetPlatform").toUpperCase();
            if (project.osSpecificName !== expected)
                throw "Wrong project property value: " + project.osSpecificName
                        + "\nexpected: " + expected;

            var code = '$NUMBERTYPE $FUNCTIONNAME($NUMBERTYPE, $STRINGTYPE) { return 0; }';
            code = expandMacros(code, product.replacements);
            var args = ['echo ' + code + '>' + output.filePath]
            var cmd
            if (product.moduleProperty("qbs", "hostOS").contains('windows')) {
                cmd = new Command(product.qbs.windowsShellPath, ['/C'].concat(args));
            } else {
                args[0] = args[0].replace(/\(/g, '\\(')
                args[0] = args[0].replace(/\)/g, '\\)')
                args[0] = args[0].replace(/;/g, '\\;')
                cmd = new Command(product.qbs.shellPath, ['-c'].concat(args))
            }
            cmd.description = 'generate\t' + FileInfo.fileName(output.filePath);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }
}
