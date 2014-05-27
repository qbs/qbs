import qbs 1.0

Project {
    StaticLibrary {
        name: "a"

        Depends { name: "cpp" }

        files: [
            "a.c",
        ]
    }

    StaticLibrary {
        name: "b"

        Depends { name: "a" }
        Depends { name: "cpp" }

        files: [
            "b.c",
        ]
    }

    CppApplication {
        name: "appWithoutSources"
        Properties {
            condition: qbs.toolchain.contains("msvc")
            cpp.linkerFlags: "/ENTRY:main"
        }

        Depends { name: "a" }
        Depends { name: "b" }
    }
}
