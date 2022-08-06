CppApplication {
    name: "app"
    property bool enableNewestModule: true

    Probe {
        id: osProbe
        property string toolchainType: qbs.toolchainType
        property string compilerVersion: cpp.compilerVersion
        configure: {
            console.info("is msvc: " + (toolchainType === "msvc" || toolchainType === "clang-cl"));
            var isOld = (toolchainType === "msvc" && compilerVersion < "19.29.30138")
                || (toolchainType === "clang-cl" && compilerVersion < "13");
            console.info("is old msvc: " + isOld);
            found = true;
        }
    }

    Depends { name: "oldmodule" }
    Depends { name: "newermodule" }
    Depends { name: "newestmodule"; condition: enableNewestModule }

    files: "main.c"
}
