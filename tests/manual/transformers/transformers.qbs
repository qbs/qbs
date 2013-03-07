import qbs.base 1.0
import qbs.fileinfo as FileInfo

Project {
    Product {
        name: "HelloWorld"
        type: "application"
        files: ["main.cpp"]

        Depends { name: "cpp" }

        Transformer {
            // no inputs -> just a generator
            Artifact {
                fileName: "foo.txt"
                fileTags: "text"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating foo.txt";
                cmd.highlight = "linker";
                cmd.sourceCode = function () {
                    File.remove(output.fileName);
                    var f = new TextFile(output.fileName, TextFile.WriteOnly);
                    f.write("Dear Sir/Madam,\n\n");
                    f.write("this is a generated file.\n\n\n");
                    f.write("Best Regards and Mellow Greetings,\nYour Build Tool.\n");
                    f.close();
                }
                return cmd;
            }
        }

        Transformer {
            inputs: ["main.cpp"]    // will be taken from the source dir
            Artifact {
                fileName: "bar.txt"
                fileTags: "text"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating bar.txt";
                cmd.highlight = "linker";
                cmd.inputFileName = input.fileName;
                cmd.sourceCode = function() {
                    File.remove(output.fileName);
                    var f = new TextFile(output.fileName, TextFile.WriteOnly);
                    f.write("Dear Sir/Madam,\n\n");
                    f.write("this file was generated from " + inputFileName + ".\n\n\n");
                    f.write("Best Regards and Mellow Greetings,\nYour Build Tool.\n");
                    f.close();
                }
                return cmd;
            }
        }
    }
}

