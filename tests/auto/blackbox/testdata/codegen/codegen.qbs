import qbs 1.0
import qbs.FileInfo

Project {
    property string name: 'codegen'
    property string osSpecificName: name.toUpperCase() + '_' + qbs.targetOS[0].toUpperCase()

    Product {
        type: 'application'
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
    }

    Rule {
        inputs: ['text']
        Artifact {
            fileTags: ['cpp']
            fileName: input.baseName + '.cpp'
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

            // check whether we can access project properties here
            var expected = "CODEGEN_" + product.moduleProperty("qbs", "targetOS")[0].toUpperCase();
            if (project.osSpecificName !== expected)
                throw "Wrong project property value: " + project.osSpecificName
                        + "\nexpected: " + expected;

            var code = '$NUMBERTYPE $FUNCTIONNAME($NUMBERTYPE, $STRINGTYPE) { return 0; }';
            code = expandMacros(code, product.replacements);
            var args = ['echo ' + code + '>' + output.fileName]
            var cmd
            if (product.moduleProperty("qbs", "hostOS").contains('windows')) {
                cmd = new Command('cmd.exe', ['/C'].concat(args));
            } else {
                args[0] = args[0].replace(/\(/g, '\\(')
                args[0] = args[0].replace(/\)/g, '\\)')
                args[0] = args[0].replace(/;/g, '\\;')
                cmd = new Command('/bin/sh', ['-c'].concat(args))
            }
            cmd.description = 'generate\t' + FileInfo.fileName(output.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }
}
