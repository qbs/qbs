import qbs.base 1.0
import qbs.fileinfo 1.0 as FileInfo
import '../QtModule.qbs' as QtModule

QtModule {
    qtModuleName: "Gui"

    Depends { id: qtcore; name: "Qt.core" }

    FileTagger {
        pattern: "*.ui"
        fileTags: ["ui"]
    }

    Rule {
        inputs: ["ui"]

        Artifact {
//  ### TODO we want to access the module's property "qtcore.generatedFilesDir" here. But without evaluating all available properties a priori.
//            fileName: input.baseDir + '/qrc_' + input.baseName + '.cpp'
            fileName: 'GeneratedFiles/' + product.name + '/ui_' + input.baseName + '.h'
            fileTags: ["hpp"]
        }

        prepare: {
            var cmd = new Command(product.module.binPath + '/uic', [input.fileName, '-o', output.fileName])
            cmd.description = 'uic ' + FileInfo.fileName(input.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }
}

