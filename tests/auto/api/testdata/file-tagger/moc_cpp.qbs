import qbs.TextFile
import qbs.FileInfo

Project {
    Product {
        type: "application"
        consoleApplication: true
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
            fileTags: ['cpp']
            filePath: input.baseName + '.cpp'
        }
        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.sourceCode = function () {
                var file = new TextFile(input.filePath, TextFile.ReadOnly);
                var text = file.readAll();
                file.close();
                file = new TextFile(output.filePath, TextFile.WriteOnly);
                file.truncate();
                file.write(text);
                file.close();
            }
            cmd.description = 'generating ' + FileInfo.fileName(output.filePath);
            cmd.highlight = 'codegen';
            return cmd;
        }
    }
}
