import qbs.Probes

Project {
    Probes.ConanfileProbe {
        id: conanProbe
        conanfilePath: path + "/conanfile.py"
        additionalArguments: "-pr:a=qbs-test"
    }
    CppApplication {
        consoleApplication: true
        name: "p"
        files: "main.cpp"
        qbsModuleProviders: "conan"
        moduleProviders.conan.installDirectory: conanProbe.generatedFilesPath
        qbs.buildVariant: "release"
        qbs.installPrefix: ""
        install: true
        Depends { name: "conanfileprobe.testlib" }
    }
}
