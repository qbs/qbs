import qbs 1.0
import qbs.TextFile
import qbs.FileInfo

Project {
    Product {
        type: "application"
        name: "moc_cpp"

        Depends {
            name: "Qt.core"
        }

        Group {
            files: 'bla.txt'
            fileTags: ['text']
        }
    }

    Rule {
        inputs: ['text']
        Artifact {
            fileTags: ['cpp', 'moc_cpp']
            fileName: input.baseName + '.cpp'
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function () {
                var file = new TextFile(input.fileName, TextFile.ReadOnly);
                var text = file.readAll();
                file = new TextFile(output.fileName, TextFile.WriteOnly);
                file.truncate();
                file.write(text);
                file.close();
            }
            cmd.description = 'generating ' + FileInfo.fileName(output.fileName);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }
}
