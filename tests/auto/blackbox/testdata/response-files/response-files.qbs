import qbs
import qbs.FileInfo
import qbs.TextFile

Project {
    CppApplication {
        name: "cat-response-file"
        files: ["cat-response-file.cpp"]
        cpp.enableExceptions: true
    }
    Product {
        name: "response-file-text"
        type: ["text"]
        Depends { name: "cat-response-file" }
        Group {
            fileTagsFilter: ["text"]
            qbs.install: true
        }
        Rule {
            inputsFromDependencies: ["application"]
            Artifact {
                filePath: "response-file-content.txt"
                fileTags: ["text"]
            }
            prepare: {
                var filePath = inputs["application"][0].filePath;
                var args = [output.filePath, "foo", "with space", "bar"];
                var cmd = new Command(filePath, args);
                cmd.responseFileThreshold = 1;
                cmd.responseFileArgumentIndex = 1;
                cmd.responseFileUsagePrefix = '@';
                cmd.silent = true;
                return cmd;
            }
        }
    }
}
