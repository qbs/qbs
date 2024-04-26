import "../multiarch-helpers.js" as Helpers

Project {
    name: "p"
    // we do not have the access to xcode version in qbs.architectures so we need to pass it here
    property string xcodeVersion

    property bool isBundle: true
    property bool enableSigning: true
    property bool multiArch: false
    property bool multiVariant: false

    CppApplication {
        name: "A"
        version: "1.0.0"
        bundle.isBundle: project.isBundle
        files: "app.cpp"
        codesign.enableCodeSigning: project.enableSigning
        codesign.signingType: "ad-hoc"
        install: true
        installDir: ""

        qbs.architectures:
            multiArch ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiVariant ? ["debug", "release"] : []
    }

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "B"
        version: "1.0.0"
        bundle.isBundle: project.isBundle
        files: "app.cpp"
        codesign.enableCodeSigning: project.enableSigning
        codesign.signingType: "ad-hoc"
        install: true
        installDir: ""
        qbs.architectures:
            multiArch ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiVariant ? ["debug", "release"] : []
    }

    LoadableModule {
        Depends { name: "cpp" }
        name: "C"
        version: "1.0.0"
        bundle.isBundle: project.isBundle
        files: "app.cpp"
        codesign.enableCodeSigning: project.enableSigning
        codesign.signingType: "ad-hoc"
        install: true
        installDir: ""
        qbs.architectures:
            multiArch ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiVariant ? ["debug", "release"] : []
    }
}
