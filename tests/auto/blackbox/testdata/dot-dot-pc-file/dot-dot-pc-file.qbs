CppApplication {
    name: "p"
    Depends { name: "qbs-metatest-module"; }
    files: "main.cpp"
    moduleProviders.qbspkgconfig.libDirs: "libdir"
    qbsModuleProviders: "qbspkgconfig"
}
