import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Gui"

    property string uicName: "uic"

    FileTagger {
        pattern: "*.ui"
        fileTags: ["ui"]
    }

    Rule {
        inputs: ["ui"]

        Artifact {
//  ### TODO we want to access the module's property "qt.core.generatedFilesDir" here. But without evaluating all available properties a priori.
            fileName: 'GeneratedFiles/' + product.name + '/ui_' + input.completeBaseName + '.h'
            fileTags: ["hpp"]
        }

        prepare: {
            var cmd = new Command(product.module.binPath + '/' + product.module.uicName,
                                  [input.fileName, '-o', output.fileName])
            cmd.description = 'uic ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }
}

