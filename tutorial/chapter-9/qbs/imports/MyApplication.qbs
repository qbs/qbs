CppApplication {
    Depends { name: "config.myproject" }
    version: config.myproject.productVersion

    cpp.rpaths: config.myproject.libRPaths
    consoleApplication: true
}
