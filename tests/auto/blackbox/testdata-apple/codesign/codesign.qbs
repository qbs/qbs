import "../multiarch-helpers.js" as Helpers

Project {
    name: "p"
    property string xcodeVersion

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

        qbs.architectures:
            project.xcodeVersion ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
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
        qbs.architectures:
            project.xcodeVersion ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
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
        qbs.architectures:
            project.xcodeVersion ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
    }
}
