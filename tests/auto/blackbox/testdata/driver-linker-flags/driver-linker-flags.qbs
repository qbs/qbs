CppApplication {
    files: "main.cpp"

    Properties {
        condition: toolchainProbe.found
        cpp.driverLinkerFlags: ["-nostartfiles"]
    }

    Probe {
        id: toolchainProbe
        condition: qbs.toolchain.includes("gcc")
        configure: {
            console.info("toolchain is GCC-like");
            found = true;
        }
    }
}
