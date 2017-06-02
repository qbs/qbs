import qbs

CppApplication {
    Probe {
        id: dummy
        configure: {
            if (qbs.toolchain.contains("msvc"))
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
