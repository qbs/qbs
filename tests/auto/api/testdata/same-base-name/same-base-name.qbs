Project {
    CppApplication {
        type: "application"
        consoleApplication: true
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
            condition: qbs.targetOS.contains("darwin")
            files: [
                "lib.m",
                "lib.mm"
            ]
        }

        Export {
            Depends { name: "cpp" }
            cpp.frameworks: qbs.targetOS.contains("darwin") ? "Foundation" : undefined
        }
    }
}
