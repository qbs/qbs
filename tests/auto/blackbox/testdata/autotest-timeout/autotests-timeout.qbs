import qbs.Host

Project {
    CppApplication {
        condition: {
            var result = qbs.targetPlatform === Host.platform();
            if (!result)
                console.info("targetPlatform differs from hostPlatform");
            return result;
        }
        name: "testApp"
        type: ["application", "autotest"]
        Depends { name: "autotest" }
        cpp.cxxLanguageVersion: "c++11"
        cpp.minimumOsxVersion: "10.8" // For <chrono>
        Properties {
            condition: qbs.toolchain.includes("gcc")
            cpp.driverFlags: "-pthread"
        }
        files: "test-main.cpp"
    }
    AutotestRunner {
        Depends {
            name: "cpp" // Make sure build environment is set up properly.
            condition: Host.os().includes("windows") && qbs.toolchain.includes("gcc")
        }
    }
}
