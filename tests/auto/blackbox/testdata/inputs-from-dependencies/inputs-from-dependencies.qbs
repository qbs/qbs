Project {
    Product {
        name: "TextFileContainer1"
        type: "txt_container"
        Group {
            files: ["file1.txt", "file2.txt"]
            fileTags: "txt"
        }
    }
    Product {
        name: "TextFileContainer2"
        Group {
            files: ["file3.txt"]
            fileTags: "txt"
        }
    }
    Product {
        name: "TextFileContainer3"
        Group {
            files: ["file4.txt"]
            fileTags: "txt"
        }
    }

    Product {
        name: "TextFileGatherer"
        type: "printed_txt"
        Depends { name: "TextFileContainer1" }
        Depends { name: "TextFileContainer2" }
        Rule {
            inputsFromDependencies: "txt"
            multiplex: true
            Artifact {
                filePath: "blubb"
                fileTags: "printed_txt"
                alwaysUpdated: false
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "Gathering text files";
                cmd.sourceCode = function() {
                    for (i in inputs.txt)
                        console.info(inputs.txt[i].filePath);
                };
                return cmd;
            }
        }
    }
}
