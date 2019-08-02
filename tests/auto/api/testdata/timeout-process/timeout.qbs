Project {
    CppApplication {
        type: "application"
        consoleApplication: true // suppress bundle generation
        files: "main.cpp"
        name: "infinite-loop"
        cpp.cxxLanguageVersion: "c++11"
    }

    Product {
        type: "product-under-test"
        name: "caller"
        Depends { name: "infinite-loop" }
        Rule {
            inputsFromDependencies: "application"
            outputFileTags: "product-under-test"
            prepare: {
                var cmd = new Command(inputs["application"][0].filePath);
                cmd.description = "Calling application that runs forever";
                cmd.timeout = 3;
                return cmd;
            }
        }
    }
}
