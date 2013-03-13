import qbs 1.0

Project {
    CppApplication {
        Depends { name: "basenamelib" }
        name: "basename"
        files: "main.c"
    }

    StaticLibrary {
        Depends { name: "cpp" }
        name: "basenamelib"
        files: [
            "lib.c",
            "lib.cpp"
        ]

        Group {
            condition: qbs.targetOS === "mac" || qbs.targetOS === "ios"
            files: [
                "lib.m",
                "lib.mm"
            ]
        }

        ProductModule {
            Depends { name: "cpp" }
            cpp.frameworks: (qbs.targetOS === "mac" || qbs.targetOS === "ios") ? "Foundation" : undefined
        }
    }
}
