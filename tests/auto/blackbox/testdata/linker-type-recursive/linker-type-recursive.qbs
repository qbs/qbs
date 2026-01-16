import qbs.Host

Project {
    StaticLibrary {
        name: "StaticCxx"
        files: "static.cpp"
    }
    StaticLibrary {
        name: "StaticC"
        Depends { name: "StaticCxx" }
        files: "static.c"
    }
    CppApplication {
        Depends { name: "StaticC" }
        property bool dummy: {
            if (qbs.targetPlatform !== Host.platform() || qbs.architecture !== Host.architecture())
                console.info("no host target");
        }
        files: "main.c"
    }
}
