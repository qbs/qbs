import qbs.TextFile
import qbs.FileInfo

Project {
    Product { name: "newDependency" }
    Product {
        type: "deps"
        name: "product1"
        Depends { name: "product2" }
        // Depends { name: 'product2' }
        // Depends { name: 'newDependency' }
        Rule {
            multiplex: true
            inputsFromDependencies: "application"
            Artifact {
                fileTags: ["deps"]
                filePath: product.name + '.deps'
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = 'generate ' + FileInfo.fileName(output.filePath);
                cmd.highlight = 'codegen';
                cmd.sourceCode = function() {
                    file = new TextFile(output.filePath, TextFile.WriteOnly);
                    file.truncate();
                    file.write(JSON.stringify(product.dependencies, undefined, 2));
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
