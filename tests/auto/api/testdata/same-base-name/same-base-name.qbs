Project {
    CppApplication {
        type: "application"
        consoleApplication: true
        property bool dummy: { console.info("executable suffix: " + cpp.executableSuffix); }
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
            condition: qbs.targetOS.includes("darwin")
            files: [
                "lib.m",
                "lib.mm"
            ]
        }

        Export {
            Depends { name: "cpp" }
            cpp.frameworks: qbs.targetOS.includes("darwin") ? "Foundation" : undefined
        }
    }
}
