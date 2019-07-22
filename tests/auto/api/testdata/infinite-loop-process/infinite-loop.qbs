Project {
    CppApplication {
        type: "application"
        consoleApplication: true // suppress bundle generation
        files: "main.cpp"
        name: "infinite-loop"
        cpp.cxxLanguageVersion: "c++11"
        Properties {
            condition: qbs.toolchain.contains("gcc")
            cpp.driverFlags: "-pthread"
        }
    }

    Product {
        type: "mytype"
        name: "caller"
        Depends { name: "infinite-loop" }
        Depends {
            name: "cpp" // Make sure build environment is set up properly.
            condition: qbs.hostOS.contains("windows") && qbs.toolchain.contains("gcc")
        }
        Rule {
            inputsFromDependencies: "application"
            outputFileTags: "mytype"
            prepare: {
                var cmd = new Command(inputs["application"][0].filePath);
                cmd.description = "Calling application that runs forever";
                return cmd;
            }
        }
    }
}
