Project {
    name: "p"

    property bool isBundle: true
    property bool enableSigning: true

    CppApplication {
        name: "A"
        bundle.isBundle: project.isBundle
        files: "app.cpp"
        codesign.enableCodeSigning: project.enableSigning
        codesign.signingType: "ad-hoc"
        install: true
        installDir: ""
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "B"
        bundle.isBundle: project.isBundle
        files: "app.cpp"
        codesign.enableCodeSigning: project.enableSigning
        codesign.signingType: "ad-hoc"
        install: true
        installDir: ""
    }

    LoadableModule {
        Depends { name: "cpp" }
        name: "C"
        bundle.isBundle: project.isBundle
        files: "app.cpp"
        codesign.enableCodeSigning: project.enableSigning
        codesign.signingType: "ad-hoc"
        install: true
        installDir: ""
    }
}
