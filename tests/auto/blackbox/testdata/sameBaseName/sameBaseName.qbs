import qbs 1.0

Project {
    CppApplication {
        type: "application"
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
            condition: qbs.targetPlatform.indexOf("darwin") !== -1
            files: [
                "lib.m",
                "lib.mm"
            ]
        }

        Export {
            Depends { name: "cpp" }
            cpp.frameworks: qbs.targetPlatform.indexOf("darwin") !== -1 ? "Foundation" : undefined
        }
    }
}
