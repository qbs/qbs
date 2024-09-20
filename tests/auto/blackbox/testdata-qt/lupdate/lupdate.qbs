import qbs.File

Project {
    QtApplication {
        name: "qt1"

        Depends { name: "Qt.widgets" }

        /*cpp.includePaths: "subdir"*/
        // cpp.cxxLanguageVersion: "c++17"
        Group {
            name: "ignored"
            files: "qt1-main.cpp"
            fileTags: ["cpp", "qt.untranslatable"]
        }
        Group {
            name: "with extra include path"
            cpp.includePaths: outer.concat("subdir")
            files: "qt1-test.cpp"
        }
        Group {
            name: "translatable generator input"
            files: "input.txt"
            fileTags: "cpp.in"
        }
        Group {
            name: "non-translatable generator input"
            files: "input2.txt"
            fileTags: "cpp.in.nontranslatable"
        }
        files: [
            "mainwindow.ui",
            "qt1-dummy.cpp",
            "qt1-dummy2.cpp",
            "subdir/header.h",
        ]
        Rule {
            inputs: "cpp.in"
            Artifact { filePath: "generated_" + input.baseName + ".cpp"; fileTags: "cpp" }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.highlight = "filegen";
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
                return cmd;
            }
        }
        Rule {
            inputs: "cpp.in.nontranslatable"
            Artifact {
                filePath: "generated_" + input.baseName + ".cpp";
                fileTags: ["cpp", "qt.untranslatable"]
            }
            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating " + output.fileName;
                cmd.highlight = "filegen";
                cmd.sourceCode = function() { File.copy(input.filePath, output.filePath); };
                return cmd;
            }
        }
    }
    QtApplication {
        name: "qt2"
        files: [
            "qt2-dummy.cpp",
            "qt2-dummy2.cpp",
            "qt2-main.cpp",
            /*"qt2-new.cpp"*/
        ]
    }
    CppApplication {
        name: "noqt"
        files: "noqt-main.cpp"
    }
    Product {
        name: "translations"
        Depends { name: "Qt.core" }
        files: "lupdate_de_DE.ts"
    }
    QtLupdateRunner {}
}
