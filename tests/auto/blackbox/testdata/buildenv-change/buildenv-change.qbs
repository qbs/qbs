CppApplication {
    Probe {
        id: dummy
        property stringList toolchain: qbs.toolchain
        configure: {
            if (toolchain.contains("msvc"))
                console.info("msvc");
        }
    }
    files: [
        "file.c",
        "main.cpp",
        "subdir/theheader.h",
        "subdir2/theheader.h",
    ]
}
