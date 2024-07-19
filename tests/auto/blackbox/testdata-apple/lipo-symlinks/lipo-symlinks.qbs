import "../multiarch-helpers.js" as Helpers

Project {
    name: "p"
    // we do not have the access to xcode version in qbs.architectures so we need to pass it here
    property string xcodeVersion

    property bool multiArch: false
    property bool multiVariant: false

    DynamicLibrary {
        Depends { name: "cpp" }
        name: "A"
        version: "1.2.3"
        bundle.isBundle: false
        files: "lib.cpp"
        install: true
        installDir: ""
        qbs.architectures:
            multiArch ? Helpers.getArchitectures(qbs, project.xcodeVersion) : []
        qbs.buildVariants: project.multiVariant ? ["debug", "release"] : []
    }
}