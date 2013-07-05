import qbs 1.0
import qbs.File
import qbs.TextFile
import qbs.Xml
import qbs.FileInfo

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
            // no inputs -> just a generator
            Artifact {
                fileName: "foo.xml"
                fileTags: "xml"
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating foo.xml";
                cmd.highlight = "linker";
                cmd.sourceCode = function () {
                    File.remove(output.fileName);
                    var doc = new XmlDomDocument();
                    var root = doc.createElement("root");
                    doc.appendChild(root);

                    var tag = doc.createElement("Greeting");
                    root.appendChild(tag);
                    tag.appendChild(doc.createTextNode("text node"));
                    doc.save(output.fileName);
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

