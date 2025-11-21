CppApplication {
    Probe {
        id: dummy
        property stringList toolchain: qbs.toolchain
        configure: {
            if (toolchain.includes("msvc"))
                console.info("msvc");
        }
    }
    consoleApplication: true
    files: [
        "file.c",
        "main.cpp",
        "subdir/theheader.h",
        "subdir2/theheader.h",
    ]
}
