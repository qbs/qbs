CppApplication {
    name: "p"
    consoleApplication: true
    Depends { name: "qbs-metatest-module"; }
    files: "main.cpp"
    moduleProviders.qbspkgconfig.libDirs: "libdir"
    qbsModuleProviders: "qbspkgconfig"
}
