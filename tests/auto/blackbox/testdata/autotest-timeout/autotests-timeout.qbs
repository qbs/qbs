Project {
    CppApplication {
        name: "testApp"
        type: ["application", "autotest"]
        Depends { name: "autotest" }
        cpp.cxxLanguageVersion: "c++11"
        cpp.minimumOsxVersion: "10.8" // For <chrono>
        Properties {
            condition: qbs.toolchain.contains("gcc")
            cpp.driverFlags: "-pthread"
        }
        files: "test-main.cpp"
    }
    AutotestRunner {
        Depends {
            name: "cpp" // Make sure build environment is set up properly.
            condition: qbs.hostOS.contains("windows") && qbs.toolchain.contains("gcc")
        }
    }
}
