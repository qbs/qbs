CppApplication {
    name: "p"
    Depends { name: "libA" }
    files: "main.cpp"
    qbsModuleProviders: "qbspkgconfig"
    property bool dummy: {
        console.info("includePaths: " + JSON.stringify(cpp.includePaths));
        console.info("systemIncludePaths: " + JSON.stringify(cpp.systemIncludePaths));
        return true;
    }
}
