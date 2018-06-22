CppApplication {
    name: "app"
    property bool enableNewestModule: true

    Probe {
        id: osProbe
        property stringList toolchain: qbs.toolchain
        configure: {
            if (toolchain.contains("msvc"))
                console.info("is msvc");
            found = true;
        }
    }

    Depends { name: "oldmodule" }
    Depends { name: "newermodule" }
    Depends { name: "newestmodule"; condition: enableNewestModule }

    files: "main.c"
}
