import qbs 1.0
import qbs.TextFile
import qbs.FileInfo

Project {
    Product {
        type: "deps"
        name: "product1"
        Depends { name: "product2" }
        Rule {
            multiplex: true
            Artifact {
                fileTags: ["deps"]
                filePath: product.name + '.deps'
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = 'generate ' + FileInfo.fileName(output.filePath);
                cmd.highlight = 'codegen';
                cmd.content = JSON.stringify(product.dependencies);
                cmd.sourceCode = function() {
                    file = new TextFile(output.filePath, TextFile.WriteOnly);
                    file.truncate();
                    file.write(content);
                    file.close();
                }
                return cmd;
            }
        }
    }
    Product {
        type: "application"
        consoleApplication: true
        name: "product2"
        property string narf: "zort"
        Depends { name: "cpp" }
        cpp.defines: ["SMURF"]
        files: ["product2.cpp"]
    }
}
