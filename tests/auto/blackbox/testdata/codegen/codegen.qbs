import qbs.base 1.0
import qbs.fileinfo as FileInfo

Project {
    property string name: 'codegen'

    Product {
        type: 'application'
        name: project.name
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
            var code = 'int main(int, char **) { return 0; }'
            var args = ['echo ' + code + '>' + output.fileName]
            var cmd
            if (product.moduleProperty("qbs", "hostOS") == 'windows') {
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
