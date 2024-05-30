CppApplication {
    consoleApplication: true
    name: "p"
    files: "main.cpp"
    qbsModuleProviders: "conan"
    qbs.buildVariant: "release"
    qbs.installPrefix: ""
    install: true
    Depends { name: "conanmoduleprovider.testlib" }
    Depends { name: "conanmoduleprovider.testlibheader" }
}
