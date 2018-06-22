CppApplication {
    consoleApplication: true
    Depends { name: "themodule" }
    cpp.includePaths: ["subdir"]
    files: ["main.cpp"]
    Group {
        prefix: "subdir/"
        cpp.defines: ["REQUIRED_FOR_FILE1", "BREAKS_FILE2"]

        fileTags: ["cpp"]

        // This group has no files, and that's okay.

        Group {
            files: ["other.cpp", "other.h"]
            Group {
                cpp.defines: outer.concat(["ALSO_REQUIRED_FOR_FILE1"])
                files: ["file1.cpp", "file1.h"]
            }
            Group {
                cpp.defines: ["REQUIRED_FOR_FILE2"]
                files: ["file2.cpp", "file2.h"]
            }
            Group {
                name: "disabled"
                condition: false
                Group {
                    name: "indirectly disabled"
                    condition: true
                    files: ["main2.cpp"]
                }
            }
            Group {
                name: "no tags"
                fileTags: []
                files: ["main3.cpp"]
            }
        }
    }
}
