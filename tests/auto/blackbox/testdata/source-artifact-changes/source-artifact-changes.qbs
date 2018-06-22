CppApplication {
    name: "app"
    Probe {
        id: toolchainProbe
        property stringList toolchain: qbs.toolchain
        configure: {
            console.info("is gcc: " + toolchain.contains("gcc"));
            found = true;
        }
    }

    Depends { name: "module_with_files" }
}
