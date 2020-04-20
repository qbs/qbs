Project {
    CppApplication {
        type: "application"
        consoleApplication: true // suppress bundle generation
        files: "main.cpp"
        name: "infinite-loop"
        cpp.cxxLanguageVersion: "c++11"
        cpp.minimumOsxVersion: "10.8" // For <chrono>
        Properties {
            condition: qbs.toolchain.contains("gcc")
            cpp.driverFlags: "-pthread"
        }
    }

    Product {
        condition: {
            var result = qbs.targetPlatform === qbs.hostPlatform;
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        type: "product-under-test"
        name: "caller"
        Depends { name: "infinite-loop" }
        Depends {
            name: "cpp" // Make sure build environment is set up properly.
            condition: qbs.hostOS.contains("windows") && qbs.toolchain.contains("gcc")
        }
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
